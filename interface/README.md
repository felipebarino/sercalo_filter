# Interface de controle do filtro sintonizável

Este programa é um exemplo para controle do filtro sintonizável via interface USB usando PyQt5 para construir a interface do usuário.

### Estrutura do Projeto (interface)

```
interface/
├── widgets/
│   ├── __init__.py
│   ├── base_widget.py      # Template para o widget de comando
│   ├── get_iden_widget.py  # Obter dados dos dispositivos
│   ├── get_wl_widget.py    # Obter comprimento de onda
│   ├── set_wl_widget.py    # Controle do comprimento de onda
│   └── sweep_widget.py     # Controle da varredura
├── main.py                 # Ponto de entrada
├── main_window.py          # Tela principal
├── communication.py        # Módulo de comunicações
└── README.md               # Este arquivo de documentação
```

## Referência de Comandos da UART

O dispositivo se comunica via UART usando o seguinte protocolo:

  - **Formato do Comando:** `:comando[?|:][argumentos]\n`
  - **Resposta de Sucesso:** `:ACK:[dados]\n` ou `:ACK\n`
  - **Resposta de Falha:** `:NACK:[mensagem_erro]\n`

| Comando | Descrição | Exemplo de Uso | Resposta de Sucesso Esperada |
| :--- | :--- | :--- | :--- |
| `iden?` | Obtém a identificação (Modelo, S/N, FW) de ambos os filtros. | `:iden?\n` | `:ACK: Canal C: ... \| Canal L: ... \|` |
| `get-interval?[B]`| Obtém o intervalo de WL (min, max) da banda `[B]` (`C` ou `L`). | `:get-interval?C\n` | `:ACK: (1527.608,1565.503)` |
| `get-wl?[B]` | Obtém o WL atual da banda `[B]`. | `:get-wl?L\n` | `:ACK: 1575.500` |
| `set-wl:[B]:[W]` | Define o WL `[W]` para a banda `[B]`. Para a varredura se ativa. | `:set-wl:C:1550.5\n` | `:ACK` |
| `sweep:[B]:[..]`| Inicia uma varredura. Args: `B:min:max:passo_wl:passo_t`. | `:sweep:L:1570:1605:0.5:1000\n` | `:ACK` |

## Requisitos

  - Python 3.7+
  - Bibliotecas: `PyQt5` e `pyserial`

## Instalação

1.  **Navegue até a pasta da interface:**
    ```bash
    cd sercalo_filter/interface
    ```
2.  **Instale as dependências:**
    ```bash
    pip install PyQt5 pyserial
    ```

## Como Usar

1.  **Execute a Aplicação:**
    ```bash
    python main.py
    ```
2.  **Conecte ao Dispositivo:**
      - Selecione a porta serial correta na lista suspensa (onde o seu ESP32 está conectado).
      - Clique em "Atualizar" se a porta não aparecer.
      - Clique em "Conectar". O botão mudará para "Desconectar".
3.  **Opere os Filtros:**
      - Use os diferentes "widgets" na interface para enviar comandos.
      - Cada widget possui um campo de status (`Pronto.`, `OK`, ou `Erro: ...`) que exibe o resultado do último comando enviado por ele.


## Estrutura do Projeto e Extensibilidade

A interface utiliza uma arquitetura de **widgets modulares**. Para adicionar um controle para um novo comando:

1.  **Crie um novo arquivo** em `widgets/`, por exemplo, `widgets/meu_novo_widget.py`.
2.  **Crie uma classe** que herde de `BaseCommandWidget`.
3.  **Desenvolva a interface** específica para o seu comando dentro desta classe.
4.  **Emita o sinal** `send_command_requested` com a string do comando formatada.
5.  **Importe e instancie** seu novo widget no `main_window.py`.
    O sistema de comunicação e a janela principal cuidarão do resto.