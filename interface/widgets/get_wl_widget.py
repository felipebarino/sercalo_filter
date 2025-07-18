# widgets/get_wl_widget.py

from PyQt5.QtWidgets import QPushButton, QComboBox, QHBoxLayout, QLabel
from .base_widget import BaseCommandWidget

class GetWlWidget(BaseCommandWidget):
    """Widget para o comando 'get-wl'."""
    def __init__(self, parent=None):
        super().__init__("Obter Comprimento de Onda (get-wl?)", parent)

        # Layout e componentes
        control_layout = QHBoxLayout()
        self.band_selector = QComboBox()
        self.band_selector.addItems(["C", "L"])
        self.get_button = QPushButton("Consultar")

        control_layout.addWidget(QLabel("Banda:"))
        control_layout.addWidget(self.band_selector)
        control_layout.addWidget(self.get_button)
        self.group_box_layout.insertLayout(0, control_layout)

        # Conectar sinal do botão a uma função
        self.get_button.clicked.connect(self.on_get_clicked)

    def on_get_clicked(self):
        """Formata e emite o comando a ser enviado."""
        band = self.band_selector.currentText()
        command = f"get-wl?{band}"
        self.send_command_requested.emit(command)