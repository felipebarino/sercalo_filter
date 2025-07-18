/**************************************************************************************************
* Arquivo:      sercalo_i2c.c
* Autor:        Felipe Oliveira Barino
* Data:         2024-07-18
* Versão:       0.1.2
*
* Descrição:    Implementação do driver de baixo nível para comunicação I2C com o
* Filtro Óptico Sintonizável Sercalo TF1. Este arquivo contém a lógica
* para construir, enviar e validar os pacotes de comando, além de
* abstrair as operações do dispositivo em funções de alto nível.
*
* Plataforma:   ESP32
* Compilador:   xtensa-esp32-elf-gcc (ESP-IDF)
*
* Histórico de Modificações:
* [2025-05-21] - [Barino] - [0.1.0] - Versão inicial para Switch
* [2024-07-14] - [Barino] - [0.1.1] - Adaptado para o Filtro Óptico Sintonizável TF1.
* [2024-07-18] - [Barino] - [0.1.2] - Documentação e comentários extensivos.
*
**************************************************************************************************/

#include "sercalo_i2c.h"
#include "esp_log.h"
#include <string.h> // Para memcpy, strtok_r

static const char *TAG = "sercalo_i2c";

// --- Funções Auxiliares Internas ---

/**
 * @brief Tabela de lookup para cálculo de CRC-8-ATM (HEC).
 *
 * Polinômio: 0x07 (x^8 + x^2 + x + 1).
 * Esta tabela pré-calculada é usada pela função `sercalo_calculate_crc8` para
 * calcular rapidamente o checksum dos pacotes I2C, garantindo a integridade
 * dos dados transmitidos e recebidos.
 */
static const uint8_t crc8_table[256] = {
    0x00, 0x07, 0x0E, 0x09, 0x1C, 0x1B, 0x12, 0x15, 0x38, 0x3F, 0x36, 0x31, 0x24, 0x23, 0x2A, 0x2D,
    0x70, 0x77, 0x7E, 0x79, 0x6C, 0x6B, 0x62, 0x65, 0x48, 0x4F, 0x46, 0x41, 0x54, 0x53, 0x5A, 0x5D,
    0xE0, 0xE7, 0xEE, 0xE9, 0xFC, 0xFB, 0xF2, 0xF5, 0xD8, 0xDF, 0xD6, 0xD1, 0xC4, 0xC3, 0xCA, 0xCD,
    0x90, 0x97, 0x9E, 0x99, 0x8C, 0x8B, 0x82, 0x85, 0xA8, 0xAF, 0xA6, 0xA1, 0xB4, 0xB3, 0xBA, 0xBD,
    0xC7, 0xC0, 0xC9, 0xCE, 0xDB, 0xDC, 0xD5, 0xD2, 0xFF, 0xF8, 0xF1, 0xF6, 0xE3, 0xE4, 0xED, 0xEA,
    0xB7, 0xB0, 0xB9, 0xBE, 0xAB, 0xAC, 0xA5, 0xA2, 0x8F, 0x88, 0x81, 0x86, 0x93, 0x94, 0x9D, 0x9A,
    0x27, 0x20, 0x29, 0x2E, 0x3B, 0x3C, 0x35, 0x32, 0x1F, 0x18, 0x11, 0x16, 0x03, 0x04, 0x0D, 0x0A,
    0x57, 0x50, 0x59, 0x5E, 0x4B, 0x4C, 0x45, 0x42, 0x6F, 0x68, 0x61, 0x66, 0x73, 0x74, 0x7D, 0x7A,
    0x89, 0x8E, 0x87, 0x80, 0x95, 0x92, 0x9B, 0x9C, 0xB1, 0xB6, 0xBF, 0xB8, 0xAD, 0xAA, 0xA3, 0xA4,
    0xF9, 0xFE, 0xF7, 0xF0, 0xE5, 0xE2, 0xEB, 0xEC, 0xC1, 0xC6, 0xCF, 0xC8, 0xDD, 0xDA, 0xD3, 0xD4,
    0x69, 0x6E, 0x67, 0x60, 0x75, 0x72, 0x7B, 0x7C, 0x51, 0x56, 0x5F, 0x58, 0x4D, 0x4A, 0x43, 0x44,
    0x19, 0x1E, 0x17, 0x10, 0x05, 0x02, 0x0B, 0x0C, 0x21, 0x26, 0x2F, 0x28, 0x3D, 0x3A, 0x33, 0x34,
    0x4E, 0x49, 0x40, 0x47, 0x52, 0x55, 0x5C, 0x5B, 0x76, 0x71, 0x78, 0x7F, 0x6A, 0x6D, 0x64, 0x63,
    0x3E, 0x39, 0x30, 0x37, 0x22, 0x25, 0x2C, 0x2B, 0x06, 0x01, 0x08, 0x0F, 0x1A, 0x1D, 0x14, 0x13,
    0xAE, 0xA9, 0xA0, 0xA7, 0xB2, 0xB5, 0xBC, 0xBB, 0x96, 0x91, 0x98, 0x9F, 0x8A, 0x8D, 0x84, 0x83,
    0xDE, 0xD9, 0xD0, 0xD7, 0xC2, 0xC5, 0xCC, 0xCB, 0xE6, 0xE1, 0xE8, 0xEF, 0xFA, 0xFD, 0xF4, 0xF3
};

/**
 * @brief Converte um array de 4 bytes (Big-Endian) para um valor float.
 * @param b Ponteiro para o array de bytes (o primeiro byte é o MSB).
 * @return O valor float convertido.
 */
static float bytes_to_float_be(const uint8_t *b) {
    union {
        float f;
        uint8_t bytes[4];
    } converter;
    // Converte de Big-Endian (rede) para Little-Endian (host ESP32)
    converter.bytes[3] = b[0]; // MSB
    converter.bytes[2] = b[1];
    converter.bytes[1] = b[2];
    converter.bytes[0] = b[3]; // LSB
    return converter.f;
}

/**
 * @brief Converte um valor float para um array de 4 bytes (Big-Endian).
 * @param f O valor float a ser convertido.
 * @param b Ponteiro para o buffer de 4 bytes onde o resultado será armazenado.
 */
static void float_to_bytes_be(float f, uint8_t *b) {
    union {
        float val;
        uint8_t bytes[4];
    } converter;
    converter.val = f;
    // Converte de Little-Endian (host ESP32) para Big-Endian (rede)
    b[0] = converter.bytes[3]; // MSB
    b[1] = converter.bytes[2];
    b[2] = converter.bytes[1];
    b[3] = converter.bytes[0]; // LSB
}

// --- Funções Principais do Driver ---

/**
 * {@inheritdoc}
 */
esp_err_t sercalo_i2c_init_device(sercalo_dev_t *dev, i2c_port_t i2c_port, uint8_t device_address_7bit) {
    if (dev == NULL) {
        return ESP_ERR_INVALID_ARG;
    }
    dev->i2c_port = i2c_port;
    dev->device_address_7bit = device_address_7bit;
    ESP_LOGD(TAG, "Instância do dispositivo Sercalo inicializada na porta %d, endereço 0x%02X", dev->i2c_port, dev->device_address_7bit);
    return ESP_OK;
}

/**
 * {@inheritdoc}
 */
uint8_t sercalo_calculate_crc8(const uint8_t *msg, size_t len) {
    uint8_t crc = 0x00; // Valor inicial do CRC
    for (size_t i = 0; i < len; i++) {
        crc = crc8_table[crc ^ msg[i]];
    }
    return crc;
}

/**
 * {@inheritdoc}
 */
esp_err_t sercalo_send_cmd_receive_reply(sercalo_dev_t *dev, uint8_t cmd_code,
                                         const uint8_t *params_write, uint8_t params_write_len,
                                         uint8_t *reply_data_buffer, uint8_t *actual_reply_data_len, size_t max_reply_data_len) {
    if (dev == NULL) return ESP_ERR_INVALID_STATE;

    esp_err_t ret;
    uint8_t tx_buffer[32];
    uint8_t rx_buffer[32];
    size_t tx_len = 0;

    // 1. Monta o pacote de transmissão (payload)
    tx_buffer[tx_len++] = cmd_code;
    tx_buffer[tx_len++] = params_write_len;
    if (params_write_len > 0 && params_write != NULL) {
        if (tx_len + params_write_len + 1 > sizeof(tx_buffer)) {
            ESP_LOGE(TAG, "Buffer TX (cmd 0x%02X) pequeno demais", cmd_code);
            return ESP_ERR_NO_MEM;
        }
        memcpy(&tx_buffer[tx_len], params_write, params_write_len);
        tx_len += params_write_len;
    }

    // 2. Calcula o CRC8 do pacote de transmissão
    // O CRC inclui o endereço de escrita do dispositivo.
    uint8_t crc_calc_buffer_write[1 + sizeof(tx_buffer)];
    size_t crc_calc_len_write = 0;
    crc_calc_buffer_write[crc_calc_len_write++] = (dev->device_address_7bit << 1) | I2C_MASTER_WRITE;
    memcpy(&crc_calc_buffer_write[crc_calc_len_write], tx_buffer, tx_len);
    crc_calc_len_write += tx_len;
    tx_buffer[tx_len++] = sercalo_calculate_crc8(crc_calc_buffer_write, crc_calc_len_write);

    ESP_LOGD(TAG, "TX (cmd 0x%02X, addr 0x%02X, len %zu): ...", cmd_code, dev->device_address_7bit, tx_len);

    // 3. Envia o comando via I2C
    ret = i2c_master_write_to_device(dev->i2c_port, dev->device_address_7bit, tx_buffer, tx_len, pdMS_TO_TICKS(200));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Erro ao enviar comando 0x%02X: %s", cmd_code, esp_err_to_name(ret));
        return ret;
    }

    // 4. Aguarda o dispositivo processar o comando
    vTaskDelay(pdMS_TO_TICKS(150));

    // 5. Lê a resposta do dispositivo
    size_t rx_read_attempt_len = 1 + 1 + max_reply_data_len + 1; // Cmd_echo + Len/Err + Max_Payload + CRC
    if (rx_read_attempt_len > sizeof(rx_buffer)) {
        rx_read_attempt_len = sizeof(rx_buffer);
    }
    ret = i2c_master_read_from_device(dev->i2c_port, dev->device_address_7bit, rx_buffer, rx_read_attempt_len, pdMS_TO_TICKS(200));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Erro ao ler resposta do comando 0x%02X: %s", cmd_code, esp_err_to_name(ret));
        return ret;
    }

    // 6. Valida a resposta recebida
    if (rx_read_attempt_len < 3) { // Mínimo: Cmd_echo + Len/Err + CRC
        ESP_LOGE(TAG, "Resposta RX muito curta (%zu bytes)", rx_read_attempt_len);
        return ESP_ERR_INVALID_RESPONSE;
    }
    
    uint8_t response_cmd_echo = rx_buffer[0];
    uint8_t response_payload_len_or_err_num = rx_buffer[1];
    size_t total_msg_len_from_device;
    bool is_error_response = (response_cmd_echo == (cmd_code | 0x80));
    
    // 7. Determina o tamanho total da mensagem e valida o eco do comando
    if (is_error_response) {
        total_msg_len_from_device = 3; // Cmd_echo_err + Err_code + CRC
    } else if (response_cmd_echo == cmd_code) {
        total_msg_len_from_device = 2 + response_payload_len_or_err_num + 1; // Cmd_echo + Len + Payload + CRC
    } else {
        ESP_LOGE(TAG, "Eco de comando inesperado! Recebido: 0x%02X", response_cmd_echo);
        return ESP_ERR_INVALID_RESPONSE;
    }

    // 8. Valida o CRC da resposta
    uint8_t crc_calc_buffer_read[1 + sizeof(rx_buffer)];
    size_t crc_calc_len_read = 0;
    crc_calc_buffer_read[crc_calc_len_read++] = (dev->device_address_7bit << 1) | I2C_MASTER_READ;
    memcpy(&crc_calc_buffer_read[crc_calc_len_read], rx_buffer, total_msg_len_from_device - 1);
    crc_calc_len_read += (total_msg_len_from_device - 1);
    uint8_t received_crc = rx_buffer[total_msg_len_from_device - 1];
    uint8_t calculated_crc = sercalo_calculate_crc8(crc_calc_buffer_read, crc_calc_len_read);

    if (received_crc != calculated_crc) {
        ESP_LOGE(TAG, "Erro de CRC na resposta! Recebido: 0x%02X, Calculado: 0x%02X", received_crc, calculated_crc);
        return ESP_ERR_INVALID_CRC;
    }

    // 9. Processa a resposta (erro ou dados)
    if (is_error_response) {
        ESP_LOGE(TAG, "Dispositivo retornou erro para cmd 0x%02X: Código %d", cmd_code, response_payload_len_or_err_num);
        return ESP_FAIL; // Retorna um erro genérico
    }

    if (actual_reply_data_len != NULL) {
        *actual_reply_data_len = response_payload_len_or_err_num;
    }
    if (reply_data_buffer != NULL && response_payload_len_or_err_num > 0) {
        if (response_payload_len_or_err_num > max_reply_data_len) {
            ESP_LOGE(TAG, "Buffer de resposta (cmd 0x%02X) pequeno demais!", cmd_code);
            return ESP_ERR_NO_MEM;
        }
        memcpy(reply_data_buffer, &rx_buffer[2], response_payload_len_or_err_num);
    }
    return ESP_OK;
}

// --- Implementação das Funções de Comando para o Filtro Sintonizável ---

/**
 * {@inheritdoc}
 */
esp_err_t sercalo_get_id(sercalo_dev_t *dev, sercalo_id_t *id_data) {
    if (dev == NULL || id_data == NULL) return ESP_ERR_INVALID_ARG;
    
    uint8_t rx_data_payload[30];
    uint8_t actual_len;
    esp_err_t ret = sercalo_send_cmd_receive_reply(dev, SERCALO_CMD_ID, NULL, 0, rx_data_payload, &actual_len, sizeof(rx_data_payload) - 1);

    if (ret == ESP_OK) {
        rx_data_payload[actual_len] = '\0'; // Assegura que a string é nula-terminada
        char *str_payload = (char*)rx_data_payload;
        char *saveptr;
        
        // Extrai os campos separados por '|'
        char *token = strtok_r(str_payload, "|", &saveptr);
        if (token) strncpy(id_data->model, token, sizeof(id_data->model) - 1);
        
        token = strtok_r(NULL, "|", &saveptr);
        if (token) strncpy(id_data->serial_number, token, sizeof(id_data->serial_number) - 1);

        token = strtok_r(NULL, "|", &saveptr);
        if (token) strncpy(id_data->fw_version, token, sizeof(id_data->fw_version) - 1);

        ESP_LOGD(TAG, "ID (addr 0x%02X): Modelo=%s, S/N=%s, FW=%s", dev->device_address_7bit, id_data->model, id_data->serial_number, id_data->fw_version);
    }
    return ret;
}

/**
 * {@inheritdoc}
 */
esp_err_t sercalo_reset_device(sercalo_dev_t *dev) {
    if (dev == NULL) return ESP_ERR_INVALID_ARG;
    ESP_LOGD(TAG, "Resetando dispositivo (addr 0x%02X)...", dev->device_address_7bit);
    return sercalo_send_cmd_receive_reply(dev, SERCALO_CMD_RST, NULL, 0, NULL, NULL, 0);
}

/**
 * {@inheritdoc}
 */
esp_err_t sercalo_get_set_power_mode(sercalo_dev_t *dev, sercalo_power_mode_t *mode_to_set, sercalo_power_mode_t *current_mode) {
    if (dev == NULL) return ESP_ERR_INVALID_ARG;

    uint8_t param_byte_tx = 0;
    uint8_t params_len_tx = (mode_to_set != NULL) ? 1 : 0;
    if (params_len_tx > 0) {
        param_byte_tx = (uint8_t)(*mode_to_set);
    }

    uint8_t reply_data_byte;
    uint8_t actual_reply_len;
    esp_err_t ret = sercalo_send_cmd_receive_reply(dev, SERCALO_CMD_POW, (params_len_tx > 0 ? &param_byte_tx : NULL), params_len_tx, &reply_data_byte, &actual_reply_len, 1);

    if (ret == ESP_OK && actual_reply_len == 1 && current_mode != NULL) {
        *current_mode = (sercalo_power_mode_t)reply_data_byte;
        ESP_LOGD(TAG, "Modo de Energia (addr 0x%02X): %s", dev->device_address_7bit, *current_mode == SERCALO_POWER_NORMAL ? "NORMAL" : "LOW POWER");
    }
    return ret;
}

/**
 * {@inheritdoc}
 */
esp_err_t sercalo_get_temperature(sercalo_dev_t *dev, int8_t *temperature) {
    if (dev == NULL || temperature == NULL) return ESP_ERR_INVALID_ARG;
    
    uint8_t reply_data_byte;
    uint8_t actual_reply_len;
    esp_err_t ret = sercalo_send_cmd_receive_reply(dev, SERCALO_CMD_TMP, NULL, 0, &reply_data_byte, &actual_reply_len, 1);
    
    if (ret == ESP_OK && actual_reply_len == 1) {
        *temperature = (int8_t)reply_data_byte;
        ESP_LOGD(TAG, "Temperatura (addr 0x%02X): %d C", dev->device_address_7bit, *temperature);
    }
    return ret;
}

/**
 * {@inheritdoc}
 */
esp_err_t sercalo_set_mirror_position(sercalo_dev_t *dev, const sercalo_mirror_pos_t *pos) {
    if (dev == NULL || pos == NULL) return ESP_ERR_INVALID_ARG;

    uint8_t params_tx[8]; // 4 eixos * 2 bytes/eixo
    params_tx[0] = (pos->x_neg >> 8) & 0xFF; params_tx[1] = pos->x_neg & 0xFF;
    params_tx[2] = (pos->x_pos >> 8) & 0xFF; params_tx[3] = pos->x_pos & 0xFF;
    params_tx[4] = (pos->y_neg >> 8) & 0xFF; params_tx[5] = pos->y_neg & 0xFF;
    params_tx[6] = (pos->y_pos >> 8) & 0xFF; params_tx[7] = pos->y_pos & 0xFF;

    ESP_LOGD(TAG, "Definindo posição do espelho (addr 0x%02X)...", dev->device_address_7bit);
    return sercalo_send_cmd_receive_reply(dev, SERCALO_CMD_SET, params_tx, sizeof(params_tx), NULL, NULL, 0);
}

/**
 * {@inheritdoc}
 */
esp_err_t sercalo_get_mirror_position(sercalo_dev_t *dev, sercalo_mirror_pos_t *pos) {
    if (dev == NULL || pos == NULL) return ESP_ERR_INVALID_ARG;
    
    uint8_t reply_data[8];
    uint8_t actual_reply_len;
    esp_err_t ret = sercalo_send_cmd_receive_reply(dev, SERCALO_CMD_POS, NULL, 0, reply_data, &actual_reply_len, sizeof(reply_data));

    if (ret == ESP_OK && actual_reply_len == 8) {
        pos->x_neg = (reply_data[0] << 8) | reply_data[1];
        pos->x_pos = (reply_data[2] << 8) | reply_data[3];
        pos->y_neg = (reply_data[4] << 8) | reply_data[5];
        pos->y_pos = (reply_data[6] << 8) | reply_data[7];
        ESP_LOGD(TAG, "Posição atual do espelho (addr 0x%02X): ...", dev->device_address_7bit);
    }
    return ret;
}

/**
 * {@inheritdoc}
 */
esp_err_t sercalo_get_set_wavelength(sercalo_dev_t *dev, float *lambda_to_set, float *current_lambda) {
    if (dev == NULL) return ESP_ERR_INVALID_ARG;

    uint8_t params_tx[4];
    uint8_t params_len_tx = (lambda_to_set != NULL) ? sizeof(params_tx) : 0;
    if (params_len_tx > 0) {
        float_to_bytes_be(*lambda_to_set, params_tx);
        ESP_LOGD(TAG, "Definindo wl para %.3f nm", *lambda_to_set);
    }

    uint8_t reply_data[4];
    uint8_t actual_reply_len;
    esp_err_t ret = sercalo_send_cmd_receive_reply(dev, SERCALO_CMD_WVL, (params_len_tx > 0 ? params_tx : NULL), params_len_tx, reply_data, &actual_reply_len, sizeof(reply_data));

    if (ret == ESP_OK && actual_reply_len == 4 && current_lambda != NULL) {
        *current_lambda = bytes_to_float_be(reply_data);
        ESP_LOGD(TAG, "Wl atual (addr 0x%02X): %.3f nm", dev->device_address_7bit, *current_lambda);
    }
    return ret;
}

/**
 * {@inheritdoc}
 */
esp_err_t sercalo_get_min_wavelength(sercalo_dev_t *dev, float *min_lambda) {
    if (dev == NULL || min_lambda == NULL) return ESP_ERR_INVALID_ARG;
    
    uint8_t reply_data[4];
    uint8_t actual_reply_len;
    esp_err_t ret = sercalo_send_cmd_receive_reply(dev, SERCALO_CMD_WVMIN, NULL, 0, reply_data, &actual_reply_len, sizeof(reply_data));

    if (ret == ESP_OK && actual_reply_len == 4) {
        *min_lambda = bytes_to_float_be(reply_data);
        ESP_LOGD(TAG, "Wl mínimo (addr 0x%02X): %.3f nm", dev->device_address_7bit, *min_lambda);
    }
    return ret;
}

/**
 * {@inheritdoc}
 */
esp_err_t sercalo_get_max_wavelength(sercalo_dev_t *dev, float *max_lambda) {
    if (dev == NULL || max_lambda == NULL) return ESP_ERR_INVALID_ARG;

    uint8_t reply_data[4];
    uint8_t actual_reply_len;
    esp_err_t ret = sercalo_send_cmd_receive_reply(dev, SERCALO_CMD_WVMAX, NULL, 0, reply_data, &actual_reply_len, sizeof(reply_data));

    if (ret == ESP_OK && actual_reply_len == 4) {
        *max_lambda = bytes_to_float_be(reply_data);
        ESP_LOGD(TAG, "Wl máximo (addr 0x%02X): %.3f nm", dev->device_address_7bit, *max_lambda);
    }
    return ret;
}

/**
 * {@inheritdoc}
 */
esp_err_t sercalo_set_i2c_address(sercalo_dev_t *dev, uint8_t new_address_7bit) {
    if (dev == NULL) return ESP_ERR_INVALID_ARG;

    uint8_t param_tx = new_address_7bit;
    ESP_LOGI(TAG, "Tentando alterar o endereço I2C de 0x%02X para 0x%02X...", dev->device_address_7bit, new_address_7bit);
    return sercalo_send_cmd_receive_reply(dev, SERCALO_CMD_IIC, &param_tx, 1, NULL, NULL, 0);
}