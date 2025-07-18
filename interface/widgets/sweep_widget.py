# widgets/sweep_widget.py

from PyQt5.QtWidgets import QPushButton, QComboBox, QLineEdit, QHBoxLayout, QVBoxLayout, QLabel, QFormLayout
from PyQt5.QtGui import QDoubleValidator, QIntValidator
from .base_widget import BaseCommandWidget

class SweepWidget(BaseCommandWidget):
    """
    Widget para o comando 'sweep'.
    Permite ao usuário configurar e iniciar uma varredura de comprimento de onda.
    """
    def __init__(self, parent=None):
        super().__init__("Varredura de Comprimento de Onda (sweep)", parent)

        # --- Layout e Componentes ---
        # Usaremos um FormLayout para organizar os campos de entrada de forma limpa.
        form_layout = QFormLayout()

        self.band_selector = QComboBox()
        self.band_selector.addItems(["C", "L"])
        
        # Campo para o comprimento de onda inicial
        self.min_wl_input = QLineEdit("1530.0")
        self.min_wl_input.setValidator(QDoubleValidator(1000.0, 2000.0, 3, self))

        # Campo para o comprimento de onda final
        self.max_wl_input = QLineEdit("1565.0")
        self.max_wl_input.setValidator(QDoubleValidator(1000.0, 2000.0, 3, self))

        # Campo para o passo (incremento) da varredura
        self.step_wl_input = QLineEdit("0.5")
        self.step_wl_input.setValidator(QDoubleValidator(0.001, 100.0, 3, self))

        # Campo para o intervalo de tempo entre cada passo
        self.time_ms_input = QLineEdit("500")
        self.time_ms_input.setValidator(QIntValidator(10, 60000, self))

        form_layout.addRow(QLabel("Banda:"), self.band_selector)
        form_layout.addRow(QLabel("WL Inicial (nm):"), self.min_wl_input)
        form_layout.addRow(QLabel("WL Final (nm):"), self.max_wl_input)
        form_layout.addRow(QLabel("Passo (nm):"), self.step_wl_input)
        form_layout.addRow(QLabel("Intervalo (ms):"), self.time_ms_input)

        self.start_button = QPushButton("Iniciar Varredura")

        # Adiciona o form layout e o botão ao GroupBox do widget base.
        self.group_box_layout.insertLayout(0, form_layout)
        self.group_box_layout.insertWidget(1, self.start_button)

        # --- Conexões ---
        self.start_button.clicked.connect(self.on_start_sweep_clicked)

    def on_start_sweep_clicked(self):
        """
        Coleta os dados dos campos de entrada, formata o comando 'sweep'
        e emite o sinal para enviá-lo.
        """
        # Coleta todos os valores dos campos da interface
        band = self.band_selector.currentText()
        min_wl = self.min_wl_input.text()
        max_wl = self.max_wl_input.text()
        step_wl = self.step_wl_input.text()
        time_ms = self.time_ms_input.text()

        # Validação simples para garantir que nenhum campo está vazio
        if not all([band, min_wl, max_wl, step_wl, time_ms]):
            self.update_status("Todos os campos devem ser preenchidos.", is_error=True)
            return
            
        # Formata o comando conforme especificado no firmware
        command = f"sweep:{band}:{min_wl}:{max_wl}:{step_wl}:{time_ms}"
        self.send_command_requested.emit(command)