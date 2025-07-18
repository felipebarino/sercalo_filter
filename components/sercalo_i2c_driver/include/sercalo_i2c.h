/**************************************************************************************************
* Arquivo:      sercalo_i2c.h
* Autor:        Felipe Oliveira Barino
* Data:         2024-07-18
* Versão:       0.1.2
*
* Descrição:    Arquivo de cabeçalho (header) para o driver do Filtro Óptico
* Sintonizável Sercalo TF1. Define a interface pública do driver,
* incluindo estruturas de dados, códigos de comando e protótipos
* de função que serão usados pela camada de aplicação.
*
* Plataforma:   ESP32
* Compilador:   xtensa-esp32-elf-gcc (ESP-IDF)
*
* Histórico de Modificações:
* [2024-05-21] - [Barino] - [0.1.0] - Versão inicial para Switch
* [2024-07-14] - [Barino] - [0.1.1] - Modificado para controle do Filtro Óptico Sintonizável TF1
* [2024-07-18] - [Barino] - [0.1.2] - Documentação e comentários extensivos.
*
**************************************************************************************************/

#ifndef SERCALO_I2C_H
#define SERCALO_I2C_H

#include "driver/i2c.h"
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

// --- Macros de Comando ---
/**
 * @brief Endereço I2C padrão de fábrica do dispositivo Sercalo TF1 (7-bit).
 * O manual do TF1 especifica um endereço de fábrica de 0xFE (8-bit com R/W).
 * 0xFE >> 1 = 0x7F (7-bit).
 */
#define SERCALO_DEVICE_ADDRESS_DEFAULT  0x7F

// Códigos dos Comandos (Binário) para o Filtro Sintonizável TF1
#define SERCALO_CMD_ID          0x01 // Retorna a identificação do equipamento
#define SERCALO_CMD_RST         0x02 // Reseta o dispositivo
#define SERCALO_CMD_POW         0x03 // Retorna ou altera o modo de energia do dispositivo
#define SERCALO_CMD_ERM         0x04 // Retorna ou altera o modo de retorno de erro
#define SERCALO_CMD_TMP         0x08 // Retorna a temperatura do microcontrolador
#define SERCALO_CMD_UART        0x10 // Retorna ou altera a taxa de baud da UART
#define SERCALO_CMD_PTY         0x11 // Retorna ou altera a paridade da UART
#define SERCALO_CMD_IIC         0x20 // Retorna ou altera o endereço para SMBus/I²C
#define SERCALO_CMD_SET         0x50 // Move o espelho MEMS para a posição especificada
#define SERCALO_CMD_POS         0x51 // Retorna a posição atual do espelho MEMS
#define SERCALO_CMD_CHSET       0x52 // Define o canal definido pelo usuário especificado
#define SERCALO_CMD_CHGET       0x53 // Retorna o conteúdo do canal definido pelo usuário especificado
#define SERCALO_CMD_CHMOD       0x54 // Modifica o canal definido pelo usuário especificado
#define SERCALO_CMD_WVL         0x55 // Retorna ou define o comprimento de onda de saída
#define SERCALO_CMD_WVMIN       0x56 // Retorna o comprimento de onda mínimo selecionável
#define SERCALO_CMD_WVMAX       0x57 // Retorna o comprimento de onda máximo selecionável


// --- Estruturas e Tipos de Dados Públicos ---

/**
 * @brief Estrutura para representar o contexto de um dispositivo Sercalo.
 *
 * Armazena as informações necessárias para se comunicar com um dispositivo
 * específico no barramento I2C.
 */
typedef struct {
    i2c_port_t i2c_port;            /*!< Porta I2C do ESP32 (I2C_NUM_0 ou I2C_NUM_1). */
    uint8_t    device_address_7bit; /*!< Endereço I2C de 7 bits do dispositivo. */
} sercalo_dev_t;

/**
 * @brief Estrutura para armazenar os dados de identificação do dispositivo.
 */
typedef struct {
    char model[16];         /*!< Buffer para o modelo do produto. */
    char serial_number[16]; /*!< Buffer para o número de série. */
    char fw_version[8];     /*!< Buffer para a versão do firmware. */
} sercalo_id_t;

/**
 * @brief Estrutura para armazenar a posição do espelho MEMS (4 eixos).
 */
typedef struct {
    uint16_t x_neg; /*!< Posição do atuador do eixo X negativo. */
    uint16_t x_pos; /*!< Posição do atuador do eixo X positivo. */
    uint16_t y_neg; /*!< Posição do atuador do eixo Y negativo. */
    uint16_t y_pos; /*!< Posição do atuador do eixo Y positivo. */
} sercalo_mirror_pos_t;

/**
 * @brief Enumeração dos modos de energia do filtro.
 */
typedef enum {
    SERCALO_POWER_LOW = 0,    /*!< Modo de baixo consumo. */
    SERCALO_POWER_NORMAL = 1  /*!< Modo de operação normal. */
} sercalo_power_mode_t;


// --- Protótipos de Funções Públicas ---

/**
 * @brief Inicializa a estrutura de um dispositivo Sercalo.
 *
 * Preenche a estrutura `sercalo_dev_t` com a porta I2C e o endereço do dispositivo.
 * Esta função deve ser chamada antes de qualquer outra função do driver.
 *
 * @param dev Ponteiro para a estrutura `sercalo_dev_t` a ser inicializada.
 * @param i2c_port A porta I2C do ESP32 onde o dispositivo está conectado.
 * @param device_address_7bit O endereço de 7 bits do dispositivo no barramento I2C.
 * @return ESP_OK em caso de sucesso, ESP_ERR_INVALID_ARG se `dev` for nulo.
 */
esp_err_t sercalo_i2c_init_device(sercalo_dev_t *dev, i2c_port_t i2c_port, uint8_t device_address_7bit);

/**
 * @brief Envia um comando e recebe uma resposta do dispositivo Sercalo.
 *
 * Esta é a função central de comunicação. Ela constrói o pacote de comando,
 * calcula e anexa o CRC, envia via I2C, aguarda, lê a resposta, valida o CRC
 * da resposta e extrai os dados do payload.
 *
 * @param dev Ponteiro para o dispositivo inicializado.
 * @param cmd_code O código do comando a ser enviado (ex: `SERCALO_CMD_ID`).
 * @param params_write Ponteiro para os dados a serem enviados como parâmetros do comando. NULL se não houver.
 * @param params_write_len Número de bytes de parâmetros a serem enviados. 0 se não houver.
 * @param[out] reply_data_buffer Buffer para armazenar os dados da resposta. Pode ser NULL se não se espera resposta.
 * @param[out] actual_reply_data_len Ponteiro para armazenar o tamanho real dos dados da resposta. Pode ser NULL.
 * @param max_reply_data_len O tamanho máximo do `reply_data_buffer`.
 * @return ESP_OK em sucesso, ou um código de erro do ESP-IDF em caso de falha.
 */
esp_err_t sercalo_send_cmd_receive_reply(sercalo_dev_t *dev, uint8_t cmd_code,
                                         const uint8_t *params_write, uint8_t params_write_len,
                                         uint8_t *reply_data_buffer, uint8_t *actual_reply_data_len, size_t max_reply_data_len);
/**
 * @brief Calcula o checksum CRC-8 para uma mensagem.
 *
 * @param msg Ponteiro para o buffer da mensagem.
 * @param len Comprimento da mensagem em bytes.
 * @return O valor do CRC-8 calculado.
 */
uint8_t sercalo_calculate_crc8(const uint8_t *msg, size_t len);

// --- Funções da API de Alto Nível ---

/**
 * @brief Obtém os dados de identificação do dispositivo.
 * @param dev Ponteiro para o dispositivo.
 * @param[out] id_data Ponteiro para a estrutura onde os dados de ID serão armazenados.
 * @return ESP_OK em sucesso, ou um código de erro.
 */
esp_err_t sercalo_get_id(sercalo_dev_t *dev, sercalo_id_t *id_data);

/**
 * @brief Envia um comando de reset para o dispositivo.
 * @param dev Ponteiro para o dispositivo.
 * @return ESP_OK em sucesso, ou um código de erro.
 */
esp_err_t sercalo_reset_device(sercalo_dev_t *dev);

/**
 * @brief Obtém e/ou define o modo de energia do dispositivo.
 * @param dev Ponteiro para o dispositivo.
 * @param mode_to_set Ponteiro para o novo modo a ser definido. Se NULL, apenas lê o modo atual.
 * @param[out] current_mode Ponteiro para armazenar o modo de energia atual. Se NULL, não armazena.
 * @return ESP_OK em sucesso, ou um código de erro.
 */
esp_err_t sercalo_get_set_power_mode(sercalo_dev_t *dev, sercalo_power_mode_t *mode_to_set, sercalo_power_mode_t *current_mode);

/**
 * @brief Obtém a temperatura interna do microcontrolador do dispositivo.
 * @param dev Ponteiro para o dispositivo.
 * @param[out] temperature Ponteiro para armazenar a temperatura em graus Celsius.
 * @return ESP_OK em sucesso, ou um código de erro.
 */
esp_err_t sercalo_get_temperature(sercalo_dev_t *dev, int8_t *temperature);

/**
 * @brief Define a posição do espelho MEMS.
 * @param dev Ponteiro para o dispositivo.
 * @param pos Ponteiro para a estrutura com as novas posições dos 4 eixos.
 * @return ESP_OK em sucesso, ou um código de erro.
 */
esp_err_t sercalo_set_mirror_position(sercalo_dev_t *dev, const sercalo_mirror_pos_t *pos);

/**
 * @brief Obtém a posição atual do espelho MEMS.
 * @param dev Ponteiro para o dispositivo.
 * @param[out] pos Ponteiro para a estrutura onde a posição atual será armazenada.
 * @return ESP_OK em sucesso, ou um código de erro.
 */
esp_err_t sercalo_get_mirror_position(sercalo_dev_t *dev, sercalo_mirror_pos_t *pos);

/**
 * @brief Obtém e/ou define o comprimento de onda do filtro.
 * @param dev Ponteiro para o dispositivo.
 * @param lambda_to_set Ponteiro para o novo comprimento de onda a ser definido. Se NULL, apenas lê o valor atual.
 * @param[out] current_lambda Ponteiro para armazenar o comprimento de onda atual. Se NULL, não armazena.
 * @return ESP_OK em sucesso, ou um código de erro.
 */
esp_err_t sercalo_get_set_wavelength(sercalo_dev_t *dev, float *lambda_to_set, float *current_lambda);

/**
 * @brief Obtém o comprimento de onda mínimo suportado pelo filtro.
 * @param dev Ponteiro para o dispositivo.
 * @param[out] min_lambda Ponteiro para armazenar o valor mínimo.
 * @return ESP_OK em sucesso, ou um código de erro.
 */
esp_err_t sercalo_get_min_wavelength(sercalo_dev_t *dev, float *min_lambda);

/**
 * @brief Obtém o comprimento de onda máximo suportado pelo filtro.
 * @param dev Ponteiro para o dispositivo.
 * @param[out] max_lambda Ponteiro para armazenar o valor máximo.
 * @return ESP_OK em sucesso, ou um código de erro.
 */
esp_err_t sercalo_get_max_wavelength(sercalo_dev_t *dev, float *max_lambda);

/**
 * @brief Altera o endereço I2C do dispositivo Sercalo.
 * @param dev Ponteiro para o dispositivo.
 * @param new_address_7bit O novo endereço I2C de 7 bits a ser gravado no dispositivo.
 * @return ESP_OK se o comando for enviado com sucesso (não confirma a alteração).
 */
esp_err_t sercalo_set_i2c_address(sercalo_dev_t *dev, uint8_t new_address_7bit);

#ifdef __cplusplus
}
#endif

#endif // SERCALO_I2C_H