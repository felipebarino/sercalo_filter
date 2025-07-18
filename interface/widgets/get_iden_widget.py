# widgets/get_iden_widget.py

from PyQt5.QtWidgets import QPushButton, QHBoxLayout
from .base_widget import BaseCommandWidget

class GetIdenWidget(BaseCommandWidget):
    """
    Widget para o comando 'iden?'.
    Envia uma solicitação para obter os dados de identificação dos filtros.
    """
    def __init__(self, parent=None):
        super().__init__("Obter Identificação (iden?)", parent)

        # --- Layout e Componentes ---
        # Como este comando não tem argumentos, precisamos apenas de um botão.
        control_layout = QHBoxLayout()
        self.get_button = QPushButton("Obter Identificação dos Dispositivos")
        control_layout.addWidget(self.get_button)
        
        # Insere o layout de controle no topo do GroupBox
        self.group_box_layout.insertLayout(0, control_layout)

        # --- Conexões ---
        self.get_button.clicked.connect(self.on_get_iden_clicked)

    def on_get_iden_clicked(self):
        """
        Emite o sinal com o comando 'iden?' formatado para ser enviado.
        """
        # O comando 'iden?' é fixo e não precisa de argumentos.
        command = "iden?"
        self.send_command_requested.emit(command)