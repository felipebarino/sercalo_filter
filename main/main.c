/**************************************************************************************************
* Arquivo:      sercalo_i2c.c
* Autor:        Felipe Oliveira Barino
* Data:         2024-07-22
* Versão:       1.0.0
*
* Descrição:    Implementação das funções de driver para comunicação I2C com o
* Filtro Óptico Sintonizável Sercalo TF1.
*
* Plataforma:   ESP32
* Compilador:   xtensa-esp32-elf-gcc (baseado no ESP-IDF do projeto original)
*
* Dependências: ESP-IDF
*
* Notas:        Adaptado do driver do Switch Óptico para o Filtro Óptico Sintonizável TF1.
*
* Histórico de Modificações:
* [Data] - [Autor] - [Versão] - [Descrição da Modificação]
* 2024-07-17 - Barino - 0.1.0 - Versão inicial (sem testes)
* 2024-07-18 - Barino - 0.1.1 - Documentação e comentários
* 2024-07-22 - Barino - 1.0.0 - Mínima versão funcional
* 
**************************************************************************************************/
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "driver/i2c.h"
#include "sercalo_i2c.h" // Inclui o driver de baixo nível do dispositivo Sercalo

// --- Configurações do Barramento I2C ---
#define I2C_MASTER_SCL_IO           22          // Pino GPIO para o clock I2C (SCL)
#define I2C_MASTER_SDA_IO           21          // Pino GPIO para os dados I2C (SDA)
#define I2C_MASTER_NUM              I2C_NUM_0   // Porta I2C a ser usada (0 ou 1)
#define I2C_MASTER_FREQ_HZ          100000      // Frequência do clock I2C (100 KHz)

// --- Endereços I2C dos Dispositivos ---
#define C_BAND_FILTER_ADDR          0x3F        // Endereço I2C do filtro da Banda C
#define L_BAND_FILTER_ADDR          0x7F        // Endereço I2C do filtro da Banda L

// --- Definições de Buffers ---
#define CMD_BUFFER_SIZE             128         // Tamanho máximo do buffer para comandos recebidos via UART.
#define RESPONSE_DATA_BUFFER_SIZE   256         // Tamanho máximo do buffer para respostas de comandos.

// --- Variáveis Globais ---
static const char *TAG = "SERCALO_FILTER_APP";

/**
 * @struct filter_channel_t
 * @brief  Agrupa todos os dados e estados de um único canal de filtro.
 */
typedef struct {
    sercalo_dev_t device_handle;    /*!< Handle para o driver de baixo nível do dispositivo Sercalo. */
    char name[2];                   /*!< Nome do canal para identificação ("C" ou "L"). */
    TaskHandle_t sweep_task_handle; /*!< Handle para a task de sweep, se ativa. NULL caso contrário. */
} filter_channel_t;

// Array global contendo os dois canais de filtro.
static filter_channel_t g_filter_channels[2]; // Posição 0: Banda C, Posição 1: Banda L

// --- Primitivas de Sincronização e Comunicação Inter-Task ---
static char g_received_cmd_buffer[CMD_BUFFER_SIZE];                             /*!< Buffer global para armazenar o último comando recebido da UART. */
static SemaphoreHandle_t g_command_mutex;                                       /*!< Mutex para garantir acesso exclusivo aos periféricos I2C, evitando colisões. */
static TaskHandle_t g_command_processor_task_handle = NULL;                     /*!< Handle da task processadora de comandos, para notificação. */
static portMUX_TYPE g_command_buffer_spinlock = portMUX_INITIALIZER_UNLOCKED;   /*!< Spinlock de baixo nível (mux) para proteger o acesso ao buffer global g_received_cmd_buffer. */

// --- Estrutura para Tabela de Despacho de Comandos (Command Dispatcher) ---

/**
 * @brief  Define a assinatura padrão para todas as funções que manipulam um comando. 
 */
typedef esp_err_t (*command_handler_t)(char *args, char *response_buf, size_t response_buf_len);

/**
 * @struct command_entry_t
 * @brief  Associa o nome de um comando (string) a uma função (ponteiro de função) que o implementa.
 */
typedef struct {
    const char *command_name;       /*!< A string exata que aciona o comando. */
    command_handler_t handler;      /*!< Ponteiro para a função que executa a lógica do comando. */
} command_entry_t;

// Protótipos dos Handlers de Comando
esp_err_t handle_get_iden(char *args, char *response_buf, size_t response_buf_len);
esp_err_t handle_get_interval(char *args, char *response_buf, size_t response_buf_len);
esp_err_t handle_get_wl(char *args, char *response_buf, size_t response_buf_len);
esp_err_t handle_set_wl(char *args, char *response_buf, size_t response_buf_len);
esp_err_t handle_sweep(char *args, char *response_buf, size_t response_buf_len);
esp_err_t handle_powerup(char *args, char *response_buf, size_t response_buf_len);
esp_err_t handle_get_power(char *args, char *response_buf, size_t response_buf_len);

// Tabela de Comandos: adicionar novas linhas com comando e sua função.
static const command_entry_t command_table[] = {
    {"iden", handle_get_iden},
    {"get-interval", handle_get_interval},
    {"get-wl", handle_get_wl},
    {"set-wl", handle_set_wl},
    {"sweep", handle_sweep},
    {"powerup", handle_powerup},
    {"get-power", handle_get_power},
};
// Calcula o número de comandos na tabela em tempo de compilação.
static const int num_commands = sizeof(command_table) / sizeof(command_entry_t);


// --- Funções Auxiliares ---

/**
 * @brief Seleciona o canal de filtro (Banda C ou L) com base em um caractere.
 * @param band_char Caractere que representa a banda ('C' ou 'L', insensível a maiúsculas/minúsculas).
 * @return Ponteiro para a estrutura `filter_channel_t` correspondente, ou NULL se a banda for inválida.
 */
static filter_channel_t* select_filter_channel(const char band_char) {
    if (toupper(band_char) == 'C') {
        return &g_filter_channels[0];
    } else if (toupper(band_char) == 'L') {
        return &g_filter_channels[1];
    }
    return NULL;
}

/**
 * @brief Para e deleta uma tarefa de sweep, se ela estiver ativa para um determinado canal.
 * @param channel Ponteiro para o canal de filtro cuja tarefa de sweep deve ser parada.
 */
static void stop_sweep_if_active(filter_channel_t *channel) {
    if (channel->sweep_task_handle != NULL) {
        ESP_LOGI(TAG, "Parando task de sweep para o canal %s", channel->name);
        vTaskDelete(channel->sweep_task_handle);
        channel->sweep_task_handle = NULL;
    }
}

/**
 * @brief Garante que um canal de filtro esteja no modo de energia normal.
 *
 * Esta função de ajuda verifica o modo de energia atual do canal. Se estiver
 * em baixo consumo (idle), ela envia o comando para ativar o modo normal
 * e aguarda um tempo para a estabilização do dispositivo.
 *
 * @param channel Ponteiro para o canal de filtro a ser verificado e ativado.
 * @return ESP_OK se o canal está ou foi colocado com sucesso em modo normal.
 * @return ESP_FAIL se a comunicação ou ativação falhar.
 */
static esp_err_t ensure_power_on(filter_channel_t *channel) {
    sercalo_power_mode_t current_mode;
    esp_err_t ret;

    // 1. Verifica o estado de energia atual.
    ret = sercalo_get_set_power_mode(&channel->device_handle, NULL, &current_mode);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Falha ao obter o modo de energia para o canal %s", channel->name);
        return ESP_FAIL;
    }

    // 2. Se estiver em modo de baixo consumo, ativa o modo normal.
    if (current_mode == SERCALO_POWER_LOW) {
        ESP_LOGI(TAG, "Canal %s está em modo de repouso. Ativando...", channel->name);
        sercalo_power_mode_t power_on = SERCALO_POWER_NORMAL;
        ret = sercalo_get_set_power_mode(&channel->device_handle, &power_on, NULL);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "Falha ao ativar o modo de energia para o canal %s", channel->name);
            return ESP_FAIL;
        }
        // Adiciona um delay para garantir que o dispositivo tenha tempo para estabilizar.
        vTaskDelay(pdMS_TO_TICKS(100)); 
    }

    return ESP_OK;
}


// --- Tasks ---

/**
 * @struct sweep_params_t
 * @brief  Estrutura para passar todos os parâmetros necessários para a `wavelength_sweep_task`.
 */
typedef struct {
    filter_channel_t *channel;
    float min_wl;
    float max_wl;
    float wl_interval;
    int time_interval_ms;
} sweep_params_t;

/**
 * @brief Task que realiza uma varredura contínua de comprimento de onda.
 *
 * Esta tarefa entra em um loop infinito, varrendo de um `min_wl` a um `max_wl`
 * com um passo e atraso definidos. A tarefa é criada pelo comando 'sweep' e
 * destruída pelos comandos 'set-wl' ou por um novo comando 'sweep' no mesmo canal.
 * @param pvParameters Ponteiro para uma estrutura `sweep_params_t` contendo os parâmetros da varredura.
 */
void wavelength_sweep_task(void *pvParameters) {
    // Copia os parâmetros para a stack da task para liberar a memória do chamador.
    sweep_params_t params = *(sweep_params_t *)pvParameters;
    filter_channel_t *channel = params.channel;

    char task_tag[32];
    snprintf(task_tag, sizeof(task_tag), "SWEEP_%s", channel->name);

    ESP_LOGI(task_tag, "Iniciando varredura: min=%.3f, max=%.3f, step=%.3f, delay=%dms",
             params.min_wl, params.max_wl, params.wl_interval, params.time_interval_ms);

    while (1) {
        for (float current_wl = params.min_wl; current_wl <= params.max_wl; current_wl += params.wl_interval) {
            ESP_LOGD(task_tag, "Definindo wl: %.3f nm", current_wl);
            float target_wl = current_wl;

            // Usa o mutex para garantir que esta operação não conflite com outros comandos I2C.
            if (xSemaphoreTake(g_command_mutex, portMAX_DELAY) == pdTRUE) {
                sercalo_get_set_wavelength(&channel->device_handle, &target_wl, NULL);
                xSemaphoreGive(g_command_mutex);
            }
            vTaskDelay(pdMS_TO_TICKS(params.time_interval_ms));
        }
        ESP_LOGI(task_tag, "Varredura concluída. Reiniciando...");
    }
}

// --- Implementações dos Handlers de Comando ---

/**
 * @brief Handler para o comando `iden?`.
 *
 * Obtém os dados de identificação (Modelo, S/N, FW) de ambos os canais (C e L)
 * e os concatena no buffer de resposta.
 *
 * @param args Não utilizado neste comando.
 * @param response_buf Buffer para onde a string de resposta formatada será escrita.
 * @param response_buf_len Tamanho total do buffer de resposta.
 *
 * @return ESP_OK Sempre retorna sucesso, mesmo que a leitura de um dos canais falhe
 * (nesse caso, uma mensagem de falha é incluída na própria resposta).
 *
 * @note **Respostas pela Serial:**
 * - **Sucesso (:ACK):** `:ACK: Canal C: Modelo=..., S/N=..., FW=... | Canal L: Modelo=..., S/N=..., FW=... |\n`
 * - **Falha (:NACK):** Este comando não gera NACK. Falhas de leitura são reportadas dentro da string de ACK.
 */
esp_err_t handle_get_iden(char *args, char *response_buf, size_t response_buf_len) {
    char temp_buf[RESPONSE_DATA_BUFFER_SIZE / 2];
    response_buf[0] = '\0'; // Assegura que o buffer de resposta está vazio.

    for (int i = 0; i < 2; i++) { // Itera sobre os dois canais
        filter_channel_t *channel = &g_filter_channels[i];
        sercalo_id_t id_data;
        esp_err_t ret;

        if (xSemaphoreTake(g_command_mutex, portMAX_DELAY) == pdTRUE) {
            ret = sercalo_get_id(&channel->device_handle, &id_data);
            xSemaphoreGive(g_command_mutex);

            if (ret == ESP_OK) {
                snprintf(temp_buf, sizeof(temp_buf), "Canal %s: Modelo=%s, S/N=%s, FW=%s | ",
                         channel->name, id_data.model, id_data.serial_number, id_data.fw_version);
            } else {
                snprintf(temp_buf, sizeof(temp_buf), "Canal %s: Falha ao ler ID | ", channel->name);
            }
            // Concatena a resposta do canal no buffer final, com segurança.
            strncat(response_buf, temp_buf, response_buf_len - strlen(response_buf) - 1);
        }
    }
    return ESP_OK;
}

/**
 * @brief Handler para o comando `get-interval`.
 *
 * Obtém o intervalo de comprimento de onda operacional (mínimo e máximo)
 * para um canal especificado.
 *
 * @param args Ponteiro para a string de argumentos. Espera um caractere de banda ('C' ou 'L'). Ex: "C"
 * @param response_buf Buffer para onde a resposta `(min,max)` será escrita.
 * @param response_buf_len Tamanho do buffer de resposta.
 *
 * @return ESP_OK se a leitura do intervalo for bem-sucedida.
 * @return ESP_ERR_INVALID_ARG se a banda especificada for inválida.
 * @return ESP_FAIL se a comunicação I2C com o dispositivo falhar.
 *
 * @note **Respostas pela Serial:**
 * - **Sucesso (:ACK):** `:ACK: (1527.608,1565.503)\n`
 * - **Falha (:NACK):** `:NACK: ESP_ERR_INVALID_ARG\n` ou `:NACK: ESP_FAIL\n`
 */
esp_err_t handle_get_interval(char *args, char *response_buf, size_t response_buf_len) {
    char *band_char_str = strtok_r(args, "?", &args);
    if (!band_char_str) return ESP_ERR_INVALID_ARG;
    
    filter_channel_t *channel = select_filter_channel(band_char_str[0]);
    if (!channel) return ESP_ERR_INVALID_ARG;

    float min_lambda, max_lambda;
    esp_err_t ret_min, ret_max;

    if (xSemaphoreTake(g_command_mutex, portMAX_DELAY) == pdTRUE) {
        ret_min = sercalo_get_min_wavelength(&channel->device_handle, &min_lambda);
        ret_max = sercalo_get_max_wavelength(&channel->device_handle, &max_lambda);
        xSemaphoreGive(g_command_mutex);
    } else { return ESP_FAIL; }

    if (ret_min == ESP_OK && ret_max == ESP_OK) {
        snprintf(response_buf, response_buf_len, "(%.3f,%.3f)", min_lambda, max_lambda);
        return ESP_OK;
    }
    return ESP_FAIL;
}

/**
 * @brief Handler para o comando `get-wl`.
 *
 * Obtém o comprimento de onda atual em que um canal específico está sintonizado.
 *
 * @param args Ponteiro para a string de argumentos. Espera um caractere de banda ('C' ou 'L'). Ex: "L"
 * @param response_buf Buffer para onde o valor do comprimento de onda será escrito.
 * @param response_buf_len Tamanho do buffer de resposta.
 *
 * @return ESP_OK se a leitura for bem-sucedida.
 * @return ESP_ERR_INVALID_ARG se a banda especificada for inválida.
 * @return ESP_FAIL se a comunicação I2C falhar.
 *
 * @note **Respostas pela Serial:**
 * - **Sucesso (:ACK):** `:ACK: 1550.123\n`
 * - **Falha (:NACK):** `:NACK: ESP_ERR_INVALID_ARG\n` ou `:NACK: ESP_FAIL\n`
 */
esp_err_t handle_get_wl(char *args, char *response_buf, size_t response_buf_len) {
    char *band_char_str = strtok_r(args, "?", &args);
    if (!band_char_str) return ESP_ERR_INVALID_ARG;

    filter_channel_t *channel = select_filter_channel(band_char_str[0]);
    if (!channel) return ESP_ERR_INVALID_ARG;

    float current_lambda;
    esp_err_t ret;

    if (xSemaphoreTake(g_command_mutex, portMAX_DELAY) == pdTRUE) {
        ensure_power_on(channel); // Garante que o canal está no modo normal antes de ler o comprimento de onda.
        xSemaphoreGive(g_command_mutex);
    } else { return ESP_FAIL; }
    
    if (xSemaphoreTake(g_command_mutex, portMAX_DELAY) == pdTRUE) {
        ret = sercalo_get_set_wavelength(&channel->device_handle, NULL, &current_lambda);
        xSemaphoreGive(g_command_mutex);
    } else { return ESP_FAIL; }

    if (ret == ESP_OK) {
        snprintf(response_buf, response_buf_len, "%.3f", current_lambda);
        return ESP_OK;
    }
    return ESP_FAIL;
}

/**
 * @brief Handler para o comando `set-wl`.
 *
 * Define um novo comprimento de onda para um canal específico. Se uma tarefa de
 * varredura (`sweep`) estiver ativa no canal, ela será interrompida.
 *
 * @param args Ponteiro para os argumentos. Formato esperado: "[banda]:[wavelength]". Ex: "C:1550.5"
 * @param response_buf Não utilizado neste comando (a resposta de sucesso não contém dados).
 * @param response_buf_len Não utilizado.
 *
 * @return ESP_OK se o comprimento de onda for definido com sucesso.
 * @return ESP_ERR_INVALID_ARG se os argumentos forem malformados, a banda for inválida ou o valor de wl for inválido.
 * @return ESP_FAIL se a comunicação I2C falhar.
 *
 * @note **Respostas pela Serial:**
 * - **Sucesso (:ACK):** `:ACK\n`
 * - **Falha (:NACK):** `:NACK: ESP_ERR_INVALID_ARG\n` ou `:NACK: ESP_FAIL\n`
 */
esp_err_t handle_set_wl(char *args, char *response_buf, size_t response_buf_len) {
    char *band_str = strtok_r(args, ":", &args);
    char *wl_str = strtok_r(NULL, ":", &args);

    if (!band_str || !wl_str) return ESP_ERR_INVALID_ARG;

    filter_channel_t *channel = select_filter_channel(band_str[0]);
    if (!channel) return ESP_ERR_INVALID_ARG;

    if (xSemaphoreTake(g_command_mutex, portMAX_DELAY) == pdTRUE) {
        ensure_power_on(channel); // Garante que o canal está no modo normal antes de ler o comprimento de onda.
        xSemaphoreGive(g_command_mutex);
    } else { return ESP_FAIL; }
    
    float target_wl = atof(wl_str);
    if (target_wl <= 0) return ESP_ERR_INVALID_ARG;

    stop_sweep_if_active(channel);
    
    esp_err_t ret;
    if (xSemaphoreTake(g_command_mutex, portMAX_DELAY) == pdTRUE) {
        ret = sercalo_get_set_wavelength(&channel->device_handle, &target_wl, NULL);
        xSemaphoreGive(g_command_mutex);
    } else { return ESP_FAIL; }
    
    return ret;
}

/**
 * @brief Handler para o comando `sweep`.
 *
 * Inicia uma tarefa de varredura contínua de comprimento de onda para um canal.
 * Se uma varredura já estiver ativa, ela é parada e substituída pela nova.
 *
 * @param args Ponteiro para os argumentos. Formato: "[banda]:[min_wl]:[max_wl]:[passo_wl]:[passo_tempo_ms]".
 * Ex: "L:1570:1605:0.5:1000"
 * @param response_buf Não utilizado (a resposta de sucesso não contém dados).
 * @param response_buf_len Não utilizado.
 *
 * @return ESP_OK se a tarefa de sweep for criada com sucesso.
 * @return ESP_ERR_INVALID_ARG se os argumentos forem malformados ou inválidos.
 * @return ESP_FAIL se a criação da task falhar (por exemplo, por falta de memória).
 *
 * @note **Respostas pela Serial:**
 * - **Sucesso (:ACK):** `:ACK\n`
 * - **Falha (:NACK):** `:NACK: ESP_ERR_INVALID_ARG\n` ou `:NACK: ESP_FAIL\n`
 */
esp_err_t handle_sweep(char *args, char *response_buf, size_t response_buf_len) {
    // Extrai todos os 5 parâmetros do comando.
    char *band_str = strtok_r(args, ":", &args);
    char *min_wl_str = strtok_r(NULL, ":", &args);
    char *max_wl_str = strtok_r(NULL, ":", &args);
    char *wl_interval_str = strtok_r(NULL, ":", &args);
    char *time_interval_str = strtok_r(NULL, ":", &args);

    if (!band_str || !min_wl_str || !max_wl_str || !wl_interval_str || !time_interval_str) {
        return ESP_ERR_INVALID_ARG;
    }

    filter_channel_t *channel = select_filter_channel(band_str[0]);
    if (!channel) return ESP_ERR_INVALID_ARG;

    sweep_params_t params = {
        .channel = channel,
        .min_wl = atof(min_wl_str),
        .max_wl = atof(max_wl_str),
        .wl_interval = atof(wl_interval_str),
        .time_interval_ms = atoi(time_interval_str)
    };
    
    if (params.min_wl <= 0 || params.max_wl <= params.min_wl || params.wl_interval <= 0 || params.time_interval_ms <= 0) {
        return ESP_ERR_INVALID_ARG;
    }

    stop_sweep_if_active(channel);

    char task_name[16];
    snprintf(task_name, sizeof(task_name), "sweep_%s_task", channel->name);

    // Cria a task de varredura.
    if (xTaskCreate(wavelength_sweep_task, task_name, 4096, &params, 5, &channel->sweep_task_handle) != pdPASS) {
        channel->sweep_task_handle = NULL;
        return ESP_FAIL;
    }
    
    return ESP_OK;
}

/**
 * @brief Handler para o comando `powerup`.
 *
 * Liga os dispositivos
 *
 * @param args Não utilizado neste comando.
 * @param response_buf Buffer para onde a string de resposta formatada será escrita.
 * @param response_buf_len Tamanho total do buffer de resposta.
 *
 * @return ESP_OK Sempre retorna sucesso, mesmo que a leitura de um dos canais falhe
 * (nesse caso, uma mensagem de falha é incluída na própria resposta).
 *
 */
esp_err_t handle_powerup(char *args, char *response_buf, size_t response_buf_len) {
    char temp_buf[RESPONSE_DATA_BUFFER_SIZE / 2];
    response_buf[0] = '\0'; // Assegura que o buffer de resposta está vazio.

    for (int i = 0; i < 2; i++) { // Itera sobre os dois canais
        filter_channel_t *channel = &g_filter_channels[i];
        sercalo_power_mode_t powerup = SERCALO_POWER_NORMAL; // Define o modo de energia para "ligado" (1)
        esp_err_t ret;
        
        if (xSemaphoreTake(g_command_mutex, portMAX_DELAY) == pdTRUE) {
            ret = sercalo_get_set_power_mode(&channel->device_handle, &powerup, NULL);
            xSemaphoreGive(g_command_mutex);

            if (ret == ESP_OK) {
                snprintf(temp_buf, sizeof(temp_buf), "Canal %s: Ligado ", channel->name);
            } else {
                snprintf(temp_buf, sizeof(temp_buf), "Canal %s: Falha ao ligar | ", channel->name);
            }
            // Concatena a resposta do canal no buffer final, com segurança.
            strncat(response_buf, temp_buf, response_buf_len - strlen(response_buf) - 1);
        }
    }
    return ESP_OK;
}

/**
 * @brief Handler para o comando `get_power`.
 *
 * Ver estado dos dispositivos
 *
 * @param args Não utilizado neste comando.
 * @param response_buf Buffer para onde a string de resposta formatada será escrita.
 * @param response_buf_len Tamanho total do buffer de resposta.
 *
 * @return ESP_OK Sempre retorna sucesso, mesmo que a leitura de um dos canais falhe
 * (nesse caso, uma mensagem de falha é incluída na própria resposta).
 *
 */
esp_err_t handle_get_power(char *args, char *response_buf, size_t response_buf_len) {
    char temp_buf[RESPONSE_DATA_BUFFER_SIZE / 2];
    response_buf[0] = '\0'; // Assegura que o buffer de resposta está vazio.

    for (int i = 0; i < 2; i++) { // Itera sobre os dois canais
        filter_channel_t *channel = &g_filter_channels[i];
        sercalo_power_mode_t state;
        esp_err_t ret;
        
        if (xSemaphoreTake(g_command_mutex, portMAX_DELAY) == pdTRUE) {
            ret = sercalo_get_set_power_mode(&channel->device_handle, NULL, &state);
            xSemaphoreGive(g_command_mutex);

            if (ret == ESP_OK) {
                snprintf(temp_buf, sizeof(temp_buf), "Canal %s: %i ", channel->name, state);
            } else {
                snprintf(temp_buf, sizeof(temp_buf), "Canal %s: Falha ao ler | ", channel->name);
            }
            // Concatena a resposta do canal no buffer final, com segurança.
            strncat(response_buf, temp_buf, response_buf_len - strlen(response_buf) - 1);
        }
    }
    return ESP_OK;
}

// --- Tasks de Monitoramento e Processamento ---

/**
 * @brief Task que monitora a entrada UART, detecta e enquadra comandos.
 *
 * Esta task roda em um loop contínuo, lendo caracteres da UART. Ela implementa
 * uma máquina de estados simples para detectar o início de um comando (':') e
 * seu fim ('\n' ou '\r'). Uma vez que um comando válido é recebido, ele é
 * copiado para o buffer global e a task `command_processor_task` é notificada.
 * @param pvParameters Não utilizado.
 */
void uart_command_monitor_task(void *pvParameters) {
    char uart_buf[CMD_BUFFER_SIZE];
    int idx = 0;
    bool cmd_started = false;

    while (1) {
        int c = getchar(); // Bloqueia até um caractere ser recebido.
        if (c == EOF) {
            vTaskDelay(pdMS_TO_TICKS(10)); // Evita busy-waiting se o stream fechar.
            continue;
        }

        if (!cmd_started) {
            // Estado: Aguardando o início de um comando.
            if (c == ':') {
                cmd_started = true;
                idx = 0; // Reseta o índice do buffer.
            }
        } else {
            // Estado: Recebendo o corpo do comando.
            if (c == '\n' || c == '\r') {
                if (idx > 0) { // Se algum caractere foi recebido.
                    uart_buf[idx] = '\0'; // Termina a string.

                    // Usa mutex para garantir escrita segura no buffer global e notificar.
                    if (xSemaphoreTake(g_command_mutex, portMAX_DELAY) == pdTRUE) {
                        strncpy(g_received_cmd_buffer, uart_buf, CMD_BUFFER_SIZE - 1);
                        g_received_cmd_buffer[CMD_BUFFER_SIZE - 1] = '\0';
                        xTaskNotifyGive(g_command_processor_task_handle);
                        xSemaphoreGive(g_command_mutex);
                    }
                }
                cmd_started = false; // Retorna ao estado inicial.
            } else if (idx < CMD_BUFFER_SIZE - 1) {
                uart_buf[idx++] = (char)c; // Adiciona caractere ao buffer.
            } else {
                // Buffer cheio, descarta o comando para evitar overflow.
                ESP_LOGE(TAG, "Comando UART excedeu o tamanho do buffer. Descartado.");
                cmd_started = false;
            }
        }
    }
}

/**
 * @brief Task que processa os comandos recebidos.
 *
 * Esta tarefa permanece bloqueada até ser notificada pela `uart_command_monitor_task`.
 * Ao ser notificada, ela copia o comando do buffer global, analisa-o, encontra o
 * handler correspondente na `command_table` e o executa. Finalmente, ela imprime
 * a resposta formatada de volta para a UART.
 * @param pvParameters Não utilizado.
 */
void command_processor_task(void *pvParameters)
{
    g_command_processor_task_handle = xTaskGetCurrentTaskHandle();

    char local_cmd_buffer[CMD_BUFFER_SIZE];
    char response_buffer[RESPONSE_DATA_BUFFER_SIZE];

    while (1) {
        // Aguarda a notificação de forma eficiente, sem consumir CPU.
        if (ulTaskNotifyTake(pdTRUE, portMAX_DELAY) > 0) {
            
            // Copia o comando para um buffer local para processamento seguro.
            // A seção crítica garante que a cópia seja atômica.
            taskENTER_CRITICAL(&g_command_buffer_spinlock);
            strncpy(local_cmd_buffer, g_received_cmd_buffer, CMD_BUFFER_SIZE - 1);
            local_cmd_buffer[CMD_BUFFER_SIZE - 1] = '\0';
            taskEXIT_CRITICAL(&g_command_buffer_spinlock);

            ESP_LOGI(TAG, "Processando comando: \"%s\"", local_cmd_buffer);

            // Analisa o comando para separar o nome dos argumentos.
            char *saveptr;
            char *cmd_name = strtok_r(local_cmd_buffer, "?:", &saveptr);
            char *cmd_args = saveptr;

            if (cmd_name == NULL) {
                ESP_LOGE(TAG, "Comando inválido ou vazio.");
                continue;
            }

            // Procura e executa o comando correspondente na tabela.
            bool command_found = false;
            for (int i = 0; i < num_commands; i++) {
                if (strcmp(cmd_name, command_table[i].command_name) == 0) {
                    command_found = true;
                    response_buffer[0] = '\0';

                    ESP_LOGD(TAG, "Executando handler para: %s", cmd_name);
                    esp_err_t result = command_table[i].handler(cmd_args, response_buffer, RESPONSE_DATA_BUFFER_SIZE);

                    // Imprime a resposta formatada.
                    if (result == ESP_OK) {
                        if (strlen(response_buffer) > 0) {
                            printf(":ACK: %s\n", response_buffer);
                        } else {
                            printf(":ACK\n");
                        }
                    } else {
                        printf(":NACK: %s\n", esp_err_to_name(result));
                    }
                    
                    break; // Comando encontrado e executado, sai do loop.
                }
            }

            if (!command_found) {
                ESP_LOGE(TAG, "Comando desconhecido: \"%s\"", cmd_name);
                printf(":NACK: Comando desconhecido\n");
            }
        }
    }
}


// --- Funções de Inicialização ---

/**
 * @brief Inicializa o periférico I2C do ESP32 no modo Master.
 * @return `ESP_OK` em caso de sucesso, ou um código de erro em caso de falha.
 */
static esp_err_t i2c_master_init(void) {
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };
    i2c_param_config(I2C_MASTER_NUM, &conf);
    return i2c_driver_install(I2C_MASTER_NUM, conf.mode, 0, 0, 0);
}

/**
 * @brief Ponto de entrada principal da aplicação.
 */
void app_main(void) {
    ESP_LOGI(TAG, "Iniciando aplicação de controle de Filtros Sercalo.");

    ESP_ERROR_CHECK(i2c_master_init());
    ESP_LOGI(TAG, "Driver I2C inicializado com sucesso.");

    // Inicializa o Canal da Banda C.
    strncpy(g_filter_channels[0].name, "C", 2);
    g_filter_channels[0].sweep_task_handle = NULL;
    ESP_ERROR_CHECK(sercalo_i2c_init_device(&g_filter_channels[0].device_handle, I2C_MASTER_NUM, C_BAND_FILTER_ADDR));
    ESP_LOGI(TAG, "Filtro Banda C inicializado no endereço 0x%02X.", C_BAND_FILTER_ADDR);

    // Inicializa o Canal da Banda L.
    strncpy(g_filter_channels[1].name, "L", 2);
    g_filter_channels[1].sweep_task_handle = NULL;
    ESP_ERROR_CHECK(sercalo_i2c_init_device(&g_filter_channels[1].device_handle, I2C_MASTER_NUM, L_BAND_FILTER_ADDR));
    ESP_LOGI(TAG, "Filtro Banda L inicializado no endereço 0x%02X.", L_BAND_FILTER_ADDR);

    // Cria o mutex para proteger o acesso ao I2C.
    g_command_mutex = xSemaphoreCreateMutex();

    // Cria as tasks principais da aplicação.
    xTaskCreate(command_processor_task, "CmdProcessorTask", 4096, NULL, 5, NULL); // Prioridade 5
    xTaskCreate(uart_command_monitor_task, "UartMonitorTask", 4096, NULL, 6, NULL); // Prioridade maior para não perder comandos

    ESP_LOGI(TAG, "Sistema pronto. Aguardando comandos via UART...");
}
