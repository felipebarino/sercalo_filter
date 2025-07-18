# Controle do Filtro Óptico Sintonizável Sercalo TF1 via ESP32

Este projeto implementa um firmware para o microcontrolador ESP32 para controlar um Filtro Óptico Sintonizável Sercalo TF1. A comunicação com o filtro é feita via I2C.

A aplicação principal de exemplo realiza uma varredura de espectro contínua, ajustando o comprimento de onda do filtro desde o valor mínimo até o máximo, com um passo e intervalo configuráveis. O progresso da varredura é exibido no monitor serial.

## Funcionalidades

  * **Driver I2C Dedicado:** Um componente (`sercalo_i2c_driver`) foi desenvolvido para encapsular a comunicação e os comandos do protocolo I2C do filtro Sercalo TF1, de acordo com o manual do fabricante. A implementação inclui cálculo de CRC-8 para garantir a integridade dos dados.
  * **Arquitetura Baseada em FreeRTOS:** A aplicação utiliza o FreeRTOS para gerenciar os comandos e tarefas, permitindo fácil expansão para funcionalidades mais complexas no futuro. O uso de handles de função e associação destas à comandos por uma estrutura em tabela facilita a escalabiliodade.
  * **Controle dos filtros via USB:** Comandos para definir comprimento de onda, obter o atual ou fazer varredura foram implementados. Os comandos seguem uma estrutura semelhante à SCPI.

## Hardware Necessário

  * Placa de desenvolvimento ESP32.
  * Filtro Óptico Sintonizável Sercalo TF1 (com a placa de interface I2C).
  * Conexões I2C entre o ESP32 e o filtro Sercalo (SDA, SCL).
      * No projeto, os pinos estão configurados como:
          * **SCL:** GPIO 22
          * **SDA:** GPIO 21
  * Fonte de alimentação para o ESP32 e para o filtro Sercalo.
  * Cabo USB para programação do ESP32 e para monitoramento da saída serial.

## Software e Ferramentas

  * **ESP-IDF:** Espressif IoT Development Framework.
  * **Compilador C:** Parte do toolchain do ESP-IDF (Xtensa).
  * **Terminal Serial:** Monitor Serial do ESP-IDF, PuTTY, minicom, ou similar para visualizar os logs.

## Estrutura do Projeto

```text
sercalo_filter/
├── main/
│   ├── CMakeLists.txt
│   └── main.c                  # Lógica principal da aplicação (varredura de espectro)
├── components/
│   └── sercalo_i2c_driver/
│       ├── CMakeLists.txt
│       ├── include/
│       │   └── sercalo_i2c.h   # Interface do driver (funções e structs)
│       └── sercalo_i2c.c       # Implementação do driver I2C
├── _docs/
│   ├── Datahseet Tunable Filter TF1-_-50-9N.pdf
│   └── Manual Tunable Filter TF1-_-50-9N.pdf
├── CMakeLists.txt              # CMake principal do projeto
├── sdkconfig                   # Configuração do projeto ESP-IDF
└── README.md                   # Este arquivo
```

## Como Compilar e Gravar

1.  **Configure o Ambiente ESP-IDF:** Certifique-se de que o ESP-IDF está instalado e que as variáveis de ambiente foram exportadas (`. export.sh` ou `get_idf`).

2.  **Clone o Repositório:**

    ```bash
    git clone <url_do_seu_repositorio>
    cd sercalo_filter
    ```

3.  **Configure o Projeto:**

      * Defina o target do ESP32: `idf.py set-target esp32`.
      * Ajuste as configurações do projeto, se necessário, com `idf.py menuconfig`.
          * Verifique se as configurações de GPIO para I2C em `main/main.c` (`I2C_MASTER_SCL_IO`, `I2C_MASTER_SDA_IO`) correspondem ao seu hardware.
          * Você pode ajustar o passo da varredura (`SWEEP_STEP_NM`) e o intervalo (`SWEEP_DELAY_MS`) no mesmo arquivo.

4.  **Compile o Projeto:**

    ```bash
    idf.py build
    ```

5.  **Grave o Firmware no ESP32:**

      * Conecte o ESP32 ao computador.
      * Execute o comando abaixo, substituindo `/dev/ttyUSB0` pela porta serial correta do seu sistema (ex: `COM3` no Windows).
        ```bash
        idf.py -p /dev/ttyUSB0 flash monitor
        ```
      * O comando `monitor` abrirá automaticamente o terminal serial após a gravação.

## Protocolo de Comunicação

Todos os comandos são enviados via UART e devem seguir um formato específico:

1.  **Início do Comando:** Todo comando deve começar com o caractere dois-pontos (`:`).
2.  **Corpo do Comando:** Segue o nome do comando e seus argumentos, separados por `?` ou `:`.
3.  **Fim do Comando:** O comando é finalizado com um caractere de nova linha (`\n` ou `\r`).

**Formato Geral:**

```
:comando[?|:][argumentos]\n
```

### Formato das Respostas

O sistema responderá de duas formas possíveis:

  * **Sucesso:** A resposta começa com `ACK`, opcionalmente seguida por dados.
    ```
    :ACK
    :ACK:[dados da resposta]
    ```
  * **Erro:** A resposta começa com `NACK`, seguida por uma mensagem que descreve o erro.
    ```
    :NACK:[mensagem de erro]
    ```

-----

## Referência de Comandos

Aqui está a lista detalhada de todos os comandos implementados.

### `iden?`

Recupera as informações de identificação de ambos os filtros (Banda C e Banda L).

  * **Descrição:** Solicita modelo, número de série (S/N) e versão de firmware de cada canal.
  * **Sintaxe:**
    ```
    :iden?\n
    ```
  * **Argumentos:** Nenhum.
  * **Exemplo de Uso:**
      * **Comando:** `:iden?\n`
      * **Resposta de Sucesso:** `:ACK: Canal C: Modelo=TF1-50-9N, S/N=1234, FW=1.0 | Canal L: Modelo=TF1-50-9N, S/N=5678, FW=1.1 |`

-----

### `get-interval?`

Obtém o intervalo operacional (comprimentos de onda mínimo e máximo) de um filtro específico.

  * **Descrição:** Retorna os limites de sintonia do canal especificado.
  * **Sintaxe:**
    ```
    :get-interval?[banda]\n
    ```
  * **Argumentos:**

| Argumento | Tipo | Descrição |
| :--- | :--- | :--- |
| `banda` | char | O canal do filtro a ser consultado. `C` para Banda C, `L` para Banda L. |

  * **Exemplo de Uso:**
      * **Comando:** `:get-interval?C\n`
      * **Resposta de Sucesso:** `:ACK: (1527.608,1565.503)`

-----

### `get-wl?`

Obtém o comprimento de onda (`wavelength`) atual de um filtro específico.

  * **Descrição:** Retorna o comprimento de onda em que o canal especificado está sintonizado no momento.
  * **Sintaxe:**
    ```
    :get-wl?[banda]\n
    ```
  * **Argumentos:**

| Argumento | Tipo | Descrição |
| :--- | :--- | :--- |
| `banda` | char | O canal do filtro a ser consultado. `C` para Banda C, `L` para Banda L. |

  * **Exemplo de Uso:**
      * **Comando:** `:get-wl?L\n`
      * **Resposta de Sucesso:** `:ACK: 1575.500`

-----

### `set-wl`

Define um novo comprimento de onda para um filtro específico.

  * **Descrição:** Sintoniza o canal especificado para um novo comprimento de onda.
  * **Nota:** Se uma tarefa de varredura (`sweep`) estiver ativa para o canal, ela será **interrompida** antes de definir o novo valor.
  * **Sintaxe:**
    ```
    :set-wl:[banda]:[wavelength]\n
    ```
  * **Argumentos:**

| Argumento | Tipo | Descrição |
| :--- | :--- | :--- |
| `banda` | char | O canal do filtro a ser sintonizado (`C` ou `L`). |
| `wavelength`| float | O comprimento de onda desejado em nanômetros (nm). |

  * **Exemplo de Uso:**
      * **Comando:** `:set-wl:C:1550.5\n`
      * **Resposta de Sucesso:** `:ACK`

-----

### `sweep`

Inicia uma varredura contínua de comprimento de onda para um filtro específico. ⚙️

  * **Descrição:** Inicia uma tarefa em segundo plano que varre uma faixa de comprimentos de onda em intervalos de tempo definidos. A varredura reinicia automaticamente ao chegar ao fim.
  * **Nota Importante:** Se uma tarefa de varredura já estiver ativa para o mesmo canal, ela será **interrompida** e substituída pela nova.
  * **Sintaxe:**
    ```
    :sweep:[banda]:[min_wl]:[max_wl]:[passo_wl]:[passo_tempo_ms]\n
    ```
  * **Argumentos:**

| Argumento | Tipo | Descrição |
| :--- | :--- | :--- |
| `banda` | char | O canal do filtro a ser usado na varredura (`C` ou `L`). |
| `min_wl` | float | Comprimento de onda inicial da varredura (nm). |
| `max_wl` | float | Comprimento de onda final da varredura (nm). |
| `passo_wl` | float | O incremento de comprimento de onda a cada passo (nm). |
| `passo_tempo_ms` | int | O intervalo de tempo em milissegundos entre cada passo. |

  * **Exemplo de Uso:**
      * **Comando:** `:sweep:L:1570:1605:0.5:1000\n`
      * **Resposta de Sucesso:** `:ACK` (indica que a tarefa de varredura foi iniciada com sucesso).


## Implementação de Novos Comandos

Este firmware foi projetado para ser facilmente extensível. Adicionar um novo comando é um processo simples que envolve modificar apenas algumas seções do arquivo `main.c`, sem a necessidade de alterar a lógica principal de processamento.

### Lógica do Sistema de Comandos

O coração do sistema é uma **tabela de despacho** (ou *dispatch table*). Em vez de usar uma longa cadeia de `if-else if` para comparar strings de comando, o sistema utiliza uma abordagem mais elegante e eficiente:

1.  **`command_handler_t`**: É um tipo de ponteiro de função. Ele define a "assinatura" que toda função que implementa um comando deve ter. Essencialmente, é um contrato que diz: "Se você quer ser uma função de comando, você deve aceitar estes três argumentos: `char *args`, `char *response_buf` e `size_t response_buf_len`".

2.  **`command_entry_t`**: É uma estrutura que associa duas coisas: o nome de um comando (uma string, como `"get-wl"`) a um ponteiro para a função que o implementa (o *handler*, como `&handle_get_wl`).

3.  **`command_table[]`**: É um array de estruturas `command_entry_t`. Esta tabela é o nosso "dicionário" de comandos. Cada linha na tabela representa um comando válido no sistema.

4.  **`command_processor_task`**: Esta tarefa, ao receber um novo comando, não precisa saber quais comandos existem. Ela simplesmente percorre a `command_table`, comparando o nome do comando recebido com cada `command_name` na tabela. Ao encontrar uma correspondência, ela invoca o `handler` associado àquela entrada, passando os argumentos necessários.

### Passos para Adicionar um Novo Comando

Para adicionar um novo comando, por exemplo, um comando chamado **`reset`** que reinicia o microcontrolador, siga estes três passos:

#### Passo 1: Escrever a Função Handler

Primeiro, crie a função que conterá a lógica do novo comando. Ela **deve** seguir a assinatura definida por `command_handler_t`. Coloque esta função junto com as outras implementações de handlers.

```c
// --- Implementações dos Handlers de Comando ---

// ... handlers existentes ...

/**
 * @brief Handler para o comando `reset`.
 *
 * Reinicia o microcontrolador. Não possui argumentos e não gera resposta
 * em caso de sucesso, pois o dispositivo reiniciará imediatamente.
 *
 * @param args Não utilizado.
 * @param response_buf Não utilizado.
 * @param response_buf_len Não utilizado.
 *
 * @return ESP_OK se o comando for aceito. Na prática, não retorna se bem-sucedido.
 *
 * @note **Respostas pela Serial:**
 * - **Sucesso (:ACK):** `:ACK\n` (enviado pouco antes de reiniciar)
 * - **Falha (:NACK):** Este comando, em sua forma simples, não tem um caso de falha.
 */
esp_err_t handle_reset(char *args, char *response_buf, size_t response_buf_len) {
    ESP_LOGI(TAG, "Comando de reset recebido. Reiniciando em 1 segundo...");
    // A resposta de ACK é enviada pela command_processor_task.
    // Damos um pequeno delay para garantir que a resposta serial seja enviada.
    vTaskDelay(pdMS_TO_TICKS(1000));
    esp_restart();
    return ESP_OK; // Esta linha nunca será alcançada.
}
```

#### Passo 2: Adicionar o Protótipo do Handler

Para que a tabela de comandos "enxergue" a sua nova função, adicione o seu protótipo na seção de protótipos, junto com os outros.

```c
// Protótipos dos Handlers de Comando
esp_err_t handle_get_iden(char *args, char *response_buf, size_t response_buf_len);
esp_err_t handle_get_interval(char *args, char *response_buf, size_t response_buf_len);
esp_err_t handle_get_wl(char *args, char *response_buf, size_t response_buf_len);
esp_err_t handle_set_wl(char *args, char *response_buf, size_t response_buf_len);
esp_err_t handle_sweep(char *args, char *response_buf, size_t response_buf_len);
esp_err_t handle_reset(char *args, char *response_buf, size_t response_buf_len); // <-- Adicionar aqui
```

#### Passo 3: Registrar o Comando na Tabela

Finalmente, adicione uma nova entrada na `command_table` para associar a string `"reset"` à função `handle_reset`.

```c
// Tabela de Comandos: adicionar novas linhas com comando e sua função.
static const command_entry_t command_table[] = {
    {"iden?", handle_get_iden},
    {"get-interval", handle_get_interval},
    {"get-wl", handle_get_wl},
    {"set-wl", handle_set_wl},
    {"sweep", handle_sweep},
    {"reset", handle_reset}, // <-- Adicionar a nova linha aqui
};
```

EO `num_commands` é calculado automaticamente. Na próxima vez que você compilar e rodar o firmware, o comando `:reset\n`, deste exempo será reconhecido e executado.

## Futuras Melhorias e Ideias

  * **Interface de Controle:** Criar uma interface via UART ou um servidor web para permitir o controle manual do comprimento de onda, em vez da varredura automática.
  * **Armazenamento em NVS:** Salvar a última configuração de comprimento de onda ou configurações específicas na memória não volátil (NVS) do ESP32.
  * **Controle em Python:** Desenvolver uma interface de controle em python para controlar o dispositivo pelo PC.

