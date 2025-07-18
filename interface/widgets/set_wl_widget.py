# widgets/set_wl_widget.py

from PyQt5.QtWidgets import QPushButton, QComboBox, QLineEdit, QHBoxLayout, QLabel
from PyQt5.QtGui import QDoubleValidator
from .base_widget import BaseCommandWidget

class SetWlWidget(BaseCommandWidget):
    """Widget para o comando 'set-wl'."""
    def __init__(self, parent=None):
        super().__init__("Definir Comprimento de Onda (set-wl)", parent)

        # Layout e componentes
        control_layout = QHBoxLayout()
        self.band_selector = QComboBox()
        self.band_selector.addItems(["C", "L"])
        self.wl_input = QLineEdit("1550.0")
        self.wl_input.setValidator(QDoubleValidator(1000.0, 2000.0, 3)) # Valida a entrada
        self.set_button = QPushButton("Definir")

        control_layout.addWidget(QLabel("Banda:"))
        control_layout.addWidget(self.band_selector)
        control_layout.addWidget(QLabel("Wavelength (nm):"))
        control_layout.addWidget(self.wl_input)
        control_layout.addWidget(self.set_button)
        self.group_box_layout.insertLayout(0, control_layout)

        # Conectar sinal
        self.set_button.clicked.connect(self.on_set_clicked)

    def on_set_clicked(self):
        """Formata e emite o comando a ser enviado."""
        band = self.band_selector.currentText()
        wl = self.wl_input.text()
        if wl:
            command = f"set-wl:{band}:{wl}"
            self.send_command_requested.emit(command)