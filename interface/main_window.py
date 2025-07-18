# main_window.py

import serial
import sys
from PyQt5.QtWidgets import (QApplication, QMainWindow, QWidget, QVBoxLayout,
                             QComboBox, QPushButton, QHBoxLayout, QLabel, QMessageBox)
from PyQt5.QtCore import QThread, pyqtSlot
from communication import SerialCommunicator, serial_ports
from widgets.get_wl_widget import GetWlWidget
from widgets.set_wl_widget import SetWlWidget
from widgets.get_iden_widget import GetIdenWidget
from widgets.sweep_widget import SweepWidget

class MainWindow(QMainWindow):
    def __init__(self, parent=None):
        super().__init__(parent)
        self.setWindowTitle("Controlador de Filtro Sercalo")
        self.setGeometry(100, 100, 500, 400)

        # Thread de comunicação
        self.comm_thread = None
        self.communicator = None

        # --- Layout Principal ---
        self.central_widget = QWidget()
        self.main_layout = QVBoxLayout()
        self.central_widget.setLayout(self.main_layout)
        self.setCentralWidget(self.central_widget)

        # --- Seção de Conexão ---
        connection_layout = QHBoxLayout()
        self.port_selector = QComboBox()
        self.refresh_button = QPushButton("Atualizar")
        self.connect_button = QPushButton("Conectar")
        connection_layout.addWidget(QLabel("Porta Serial:"))
        connection_layout.addWidget(self.port_selector)
        connection_layout.addWidget(self.refresh_button)
        connection_layout.addWidget(self.connect_button)
        self.main_layout.addLayout(connection_layout)

        # --- Widgets de Comando (aqui está a escalabilidade!) ---
        self.command_widgets = []
        
        # Crie instâncias dos seus widgets
        self.get_wl_widget = GetWlWidget()
        self.set_wl_widget = SetWlWidget()
        self.get_iden_widget = GetIdenWidget()
        self.sweep_widget = SweepWidget()
        
        # Adicione-os ao layout e a uma lista de gerenciamento
        self.main_layout.addWidget(self.get_wl_widget)
        self.command_widgets.append(self.get_wl_widget)
        
        self.main_layout.addWidget(self.set_wl_widget)
        self.command_widgets.append(self.set_wl_widget)

        self.main_layout.addWidget(self.get_iden_widget)
        self.command_widgets.append(self.get_iden_widget)

        self.main_layout.addWidget(self.sweep_widget)
        self.command_widgets.append(self.sweep_widget)
        
        # Adicione aqui outros widgets que você criar
        
        self.main_layout.addStretch() # Empurra tudo para cima

        # --- Conectar Sinais e Slots ---
        self.refresh_button.clicked.connect(self.populate_ports)
        self.connect_button.clicked.connect(self.toggle_connection)
        
        # Conecta o sinal de cada widget ao comunicador
        for widget in self.command_widgets:
            widget.send_command_requested.connect(self.send_command_from_widget)

        self.populate_ports()

    def populate_ports(self):
        """Busca e exibe as portas seriais disponíveis."""
        self.port_selector.clear()
        port_names = serial_ports()
        for port in port_names:
            self.port_selector.addItem(port)

    def toggle_connection(self):
        """Conecta ou desconecta do dispositivo serial."""
        if self.communicator is None: # Se não está conectado
            port_name = self.port_selector.currentText()
            if not port_name:
                QMessageBox.warning(self, "Sem Porta", "Nenhuma porta serial selecionada.")
                return

            self.comm_thread = QThread()
            self.communicator = SerialCommunicator(port_name)
            self.communicator.moveToThread(self.comm_thread)

            # Conecta os sinais do comunicador à GUI
            self.comm_thread.started.connect(self.communicator.connect)
            self.communicator.response_received.connect(self.handle_response)
            self.communicator.error_received.connect(self.handle_error)
            self.communicator.port_closed.connect(self.on_disconnected)

            self.comm_thread.start()
            self.connect_button.setText("Desconectar")
            self.refresh_button.setEnabled(False)
            self.port_selector.setEnabled(False)

        else: # Se está conectado
            self.communicator.disconnect()

    def on_disconnected(self):
        """Chamado quando a porta é fechada."""
        if self.comm_thread:
            self.comm_thread.quit()
            self.comm_thread.wait()
        self.comm_thread = None
        self.communicator = None
        
        self.connect_button.setText("Conectar")
        self.refresh_button.setEnabled(True)
        self.port_selector.setEnabled(True)

    @pyqtSlot(str)
    def send_command_from_widget(self, command):
        """Envia um comando vindo de qualquer widget."""
        if self.communicator:
            self.communicator.send_command(command)
        else:
            QMessageBox.warning(self, "Desconectado", "Conecte-se a uma porta serial primeiro.")

    def handle_response(self, response):
        """Distribui a resposta para o widget correto (se necessário)."""
        # Lógica futura: se a resposta precisar atualizar um widget específico,
        # você pode implementar um sistema de roteamento aqui.
        # Por simplicidade, atualizamos todos por enquanto.
        for widget in self.command_widgets:
            widget.update_status(response)

    def handle_error(self, error_message):
        """Distribui a mensagem de erro."""
        for widget in self.command_widgets:
            widget.update_status(error_message, is_error=True)

    def closeEvent(self, event):
        """Garante que a conexão seja fechada ao fechar a janela."""
        if self.communicator:
            self.communicator.disconnect()
        event.accept()