Excelente\! Fico feliz em saber que a correção funcionou bem. O código está muito mais robusto agora.

Com base na sua versão `1.0.0` do `main.c`, reescrevi o `README.md` para refletir com precisão todas as funcionalidades e comandos atuais. Ele está mais claro e agora inclui os comandos de energia que você adicionou.

Aqui está a versão atualizada:

-----

# Controle do Filtro Óptico Sintonizável Sercalo TF1 via ESP32

Este projeto implementa um firmware para o microcontrolador ESP32 para controlar um Filtro Óptico Sintonizável Sercalo TF1. A comunicação com os filtros (Banda C e Banda L) é feita via I2C.

A aplicação principal de exemplo estabelece uma interface de controle via porta serial (UART), permitindo que um usuário ou um programa envie comandos para consultar o estado dos filtros, definir comprimentos de onda específicos ou iniciar varreduras de espectro contínuas.

## Funcionalidades

  * **Driver I2C Dedicado:** Um componente (`sercalo_i2c_driver`) encapsula a comunicação e os comandos do protocolo I2C do filtro Sercalo TF1, de acordo com o manual do fabricante, incluindo cálculo de CRC-8 para garantir a integridade dos dados.
  * **Arquitetura Baseada em FreeRTOS:** A aplicação utiliza o FreeRTOS para gerenciar tarefas de comunicação e controle, permitindo uma operação não bloqueante e facilmente expansível.
  * **Sistema de Comandos Escalável:** A lógica de processamento de comandos utiliza uma tabela de despacho (*dispatch table*), tornando a adição de novos comandos simples e organizada, sem a necessidade de alterar o fluxo principal.
  * **Controle Robusto via Serial:** Comandos para identificar os filtros, definir comprimentos de onda, obter o estado atual e iniciar varreduras foram implementados. O protocolo é similar ao SCPI, facilitando a automação.
  * **Gerenciamento Automático de Energia:** O firmware garante que os filtros sejam ativados (retirados do modo de repouso) automaticamente antes de executar comandos de operação, aumentando a confiabilidade do sistema.

## Hardware Necessário

  * Placa de desenvolvimento ESP32.
  * Filtro Óptico Sintonizável Sercalo TF1 (Banda C e/ou L).
  * Conexões I2C entre o ESP32 e os filtros (SDA, SCL).
      * No projeto, os pinos estão configurados como:
          * **SCL:** GPIO 22
          * **SDA:** GPIO 21
  * Fonte de alimentação para o ESP32 e para os filtros Sercalo.
  * Cabo USB para programação do ESP32 e para monitoramento/controle via terminal serial.

## Estrutura do Projeto

```text
sercalo_filter/
├── main/
│   ├── CMakeLists.txt
│   └── main.c                  # Lógica principal, tasks e handlers de comando
├── components/
│   └── sercalo_i2c_driver/
│       ├── CMakeLists.txt
│       ├── include/
│       │   └── sercalo_i2c.h   # Interface pública do driver
│       └── sercalo_i2c.c       # Implementação do driver I2C
├── CMakeLists.txt              # CMake principal do projeto
├── sdkconfig                   # Configuração do projeto ESP-IDF
└── README.md                   # Este arquivo
```

## Como Compilar e Gravar

1.  **Configure o Ambiente ESP-IDF:** Certifique-se de que o ESP-IDF está instalado e que as variáveis de ambiente foram exportadas (`. export.sh` ou `get_idf`).

2.  **Configure o Projeto:**

      * Defina o target do ESP32: `idf.py set-target esp32`.
      * Ajuste as configurações do projeto, se necessário, com `idf.py menuconfig`.
          * Verifique se as configurações de GPIO para I2C em `main/main.c` correspondem ao seu hardware.

3.  **Compile o Projeto:**

    ```bash
    idf.py build
    ```

4.  **Grave o Firmware no ESP32:**

      * Conecte o ESP32 ao computador.
      * Execute o comando abaixo, substituindo `/dev/ttyUSB0` pela porta serial correta.
        ```bash
        idf.py -p /dev/ttyUSB0 flash monitor
        ```
      * O comando `monitor` abrirá o terminal serial para interagir com o dispositivo.

## Protocolo de Comunicação

Todos os comandos são enviados via UART e devem seguir um formato específico.

1.  **Início do Comando:** Todo comando deve começar com o caractere dois-pontos (`:`).
2.  **Corpo do Comando:** Segue o nome do comando e seus argumentos, separados por `?` ou `:`.
3.  **Fim do Comando:** O comando é finalizado com um caractere de nova linha (`\n` ou `\r`).

**Formato Geral:**

```
:comando[?|:][argumentos]\n
```

### Formato das Respostas

  * **Sucesso:** A resposta começa com `:ACK`, opcionalmente seguida por dados.
    ```
    :ACK
    :ACK:[dados da resposta]
    ```
  * **Erro:** A resposta começa com `:NACK`, seguida por uma mensagem que descreve o erro.
    ```
    :NACK:[mensagem de erro]
    ```

-----

## Referência de Comandos

### `iden`

Recupera as informações de identificação de ambos os filtros (Banda C e Banda L).

  * **Descrição:** Solicita modelo, número de série (S/N) e versão de firmware de cada canal.
  * **Sintaxe:**
    ```
    :iden\n
    ```
  * **Exemplo de Resposta:**
    ```
    :ACK: Canal C: Modelo=TF, S/N=N/A, FW=7.0 | Canal L: Modelo=TF, S/N=N/A, FW=6.0 | 
    ```

### `get-interval`

Obtém o intervalo operacional (comprimentos de onda mínimo e máximo) de um filtro específico.

  * **Sintaxe:**
    ```
    :get-interval?[banda]\n
    ```
  * **Argumentos:**
      * `banda`: O canal do filtro (`C` ou `L`).
  * **Exemplo de Uso:**
      * **Comando:** `:get-interval?C\n`
      * **Resposta:** `:ACK:(1527.608,1565.503)`

### `get-wl`

Obtém o comprimento de onda (`wavelength`) atual de um filtro específico.

  * **Sintaxe:**
    ```
    :get-wl?[banda]\n
    ```
  * **Argumentos:**
      * `banda`: O canal do filtro (`C` ou `L`).
  * **Exemplo de Uso:**
      * **Comando:** `:get-wl?L\n`
      * **Resposta:** `:ACK:1575.500`

### `set-wl`

Define um novo comprimento de onda para um filtro específico.

  * **Descrição:** Sintoniza o canal para um novo comprimento de onda. Se uma varredura (`sweep`) estiver ativa, ela será interrompida.
  * **Sintaxe:**
    ```
    :set-wl:[banda]:[wavelength]\n
    ```
  * **Argumentos:**
      * `banda`: O canal do filtro a ser sintonizado (`C` ou `L`).
      * `wavelength`: O comprimento de onda desejado em nanômetros (nm).
  * **Exemplo de Uso:**
      * **Comando:** `:set-wl:C:1550.5\n`
      * **Resposta:** `:ACK`

### `sweep`

Inicia uma varredura contínua de comprimento de onda para um filtro. ⚙️

  * **Descrição:** Inicia uma tarefa que varre uma faixa de comprimentos de onda em intervalos de tempo definidos. A varredura reinicia automaticamente ao chegar ao fim. Se uma varredura já estiver ativa no canal, ela será substituída.
  * **Sintaxe:**
    ```
    :sweep:[banda]:[min_wl]:[max_wl]:[passo_wl]:[passo_tempo_ms]\n
    ```
  * **Argumentos:**
      * `banda`: O canal do filtro (`C` ou `L`).
      * `min_wl`: Comprimento de onda inicial (nm).
      * `max_wl`: Comprimento de onda final (nm).
      * `passo_wl`: Incremento do comprimento de onda a cada passo (nm).
      * `passo_tempo_ms`: Intervalo de tempo entre cada passo (milissegundos).
  * **Exemplo de Uso:**
      * **Comando:** `:sweep:L:1570:1605:0.5:1000\n`
      * **Resposta:** `:ACK`

### `powerup`

Força a ativação (modo de energia normal) de ambos os filtros.

  * **Descrição:** Garante que ambos os canais estejam prontos para receber comandos de operação.
  * **Sintaxe:**
    ```
    :powerup\n
    ```
  * **Exemplo de Resposta:**
    ```
    :ACK:Canal C: Ligado | Canal L: Ligado | 
    ```

### `get-power`

Verifica o estado de energia atual de ambos os filtros.

  * **Descrição:** Retorna `1` para modo normal (ligado) e `0` para modo de baixo consumo (repouso).
  * **Sintaxe:**
    ```
    :get-power\n
    ```
  * **Exemplo de Resposta:**
    ```
    :ACK:Canal C: 1 | Canal L: 1 | 
    ```