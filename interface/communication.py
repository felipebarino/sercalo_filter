# communication.py

import sys
import glob
import serial
from PyQt5.QtCore import QObject, QThread, pyqtSignal, pyqtSlot

def serial_ports():
    """ Lists serial port names

        :raises EnvironmentError:
            On unsupported or unknown platforms
        :returns:
            A list of the serial ports available on the system
    """
    if sys.platform.startswith('win'):
        ports = ['COM%s' % (i + 1) for i in range(256)]
    elif sys.platform.startswith('linux') or sys.platform.startswith('cygwin'):
        # this excludes your current terminal "/dev/tty"
        ports = glob.glob('/dev/tty[A-Za-z]*')
    elif sys.platform.startswith('darwin'):
        ports = glob.glob('/dev/tty.*')
    else:
        raise EnvironmentError('Unsupported platform')

    result = []
    for port in ports:
        try:
            s = serial.Serial(port)
            s.close()
            result.append(port)
        except (OSError, serial.SerialException):
            pass
    return result


class SerialCommunicator(QObject):
    """
    Gerencia a comunicação com o dispositivo serial em uma thread separada.
    """
    # Sinais emitidos para a thread principal da GUI
    response_received = pyqtSignal(str) # Emite uma resposta bem-sucedida (:ACK)
    error_received = pyqtSignal(str)    # Emite uma resposta de erro (:NACK)
    port_closed = pyqtSignal()          # Emite quando a porta é fechada

    def __init__(self, port, baudrate=115200, parent=None):
        super().__init__(parent)
        self.serial_port = None
        self._port_name = port
        self._baudrate = baudrate
        self._is_running = False

    @pyqtSlot()
    def connect(self):
        """Tenta abrir a porta serial e iniciar a leitura."""
        try:
            self.serial_port = serial.Serial(self._port_name, self._baudrate, timeout=1)
            self._is_running = True
            self.run() # Inicia o loop de leitura no contexto da thread
        except serial.SerialException as e:
            self.error_received.emit(f"Falha ao abrir porta {self._port_name}: {e}")

    @pyqtSlot(str)
    def send_command(self, command):
        """Envia um comando para o dispositivo serial."""
        if self.serial_port and self.serial_port.is_open:
            try:
                # O firmware espera o comando formatado com ':' e '\n'
                full_command = f":{command}\n"
                self.serial_port.write(full_command.encode('utf-8'))
                print(f"Enviado: {full_command.strip()}")
            except serial.SerialException as e:
                self.error_received.emit(f"Erro ao enviar comando: {e}")

    def run(self):
        """Loop principal que lê continuamente da porta serial."""
        while self._is_running and self.serial_port and self.serial_port.is_open:
            try:
                line = self.serial_port.readline().decode('utf-8').strip()
                if line:
                    print(f"Recebido: {line}")
                    if line.startswith(':ACK'):
                        self.response_received.emit(line)
                    elif line.startswith(':NACK'):
                        self.error_received.emit(line)
            except TypeError:
                # Ocorre quando a porta é fechada enquanto readline() está bloqueado
                break
            except Exception as e:
                self.error_received.emit(f"Erro de leitura: {e}")
                break
        self.port_closed.emit()

    @pyqtSlot()
    def disconnect(self):
        """Para o loop de leitura e fecha a porta serial."""
        self._is_running = False
        if self.serial_port and self.serial_port.is_open:
            self.serial_port.close()