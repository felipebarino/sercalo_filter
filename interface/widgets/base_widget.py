# widgets/base_widget.py

from PyQt5.QtWidgets import QWidget, QVBoxLayout, QGroupBox, QLabel
from PyQt5.QtCore import pyqtSignal

class BaseCommandWidget(QWidget):
    """
    Classe base para todos os widgets de comando.
    Define a interface comum e um sinal para enviar comandos.
    """
    # Sinal que será emitido com o comando formatado pronto para ser enviado
    send_command_requested = pyqtSignal(str)

    def __init__(self, title, parent=None):
        super().__init__(parent)
        self.group_box = QGroupBox(title)
        self.main_layout = QVBoxLayout(self)
        self.group_box_layout = QVBoxLayout()

        self.group_box.setLayout(self.group_box_layout)
        self.main_layout.addWidget(self.group_box)
        self.setLayout(self.main_layout)

        # Widget para exibir o resultado ou status do último comando
        self.status_label = QLabel("Pronto.")
        self.status_label.setStyleSheet("color: gray;")
        self.group_box_layout.addWidget(self.status_label)

    def update_status(self, message, is_error=False):
        """Atualiza o label de status com uma mensagem."""
        if is_error:
            # Remove o prefixo :NACK: para clareza
            clean_message = message.replace(":NACK:", "").strip()
            self.status_label.setText(f"Erro: {clean_message}")
            self.status_label.setStyleSheet("color: red; font-style: italic;")
        else:
            # Remove o prefixo :ACK:
            clean_message = message.replace(":ACK:", "").strip()
            self.status_label.setText(f"OK: {clean_message}" if clean_message else "OK")
            self.status_label.setStyleSheet("color: green;")