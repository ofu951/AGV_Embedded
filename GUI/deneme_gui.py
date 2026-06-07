import sys
import struct
import serial
import serial.tools.list_ports
from PyQt6.QtWidgets import (QApplication, QMainWindow, QWidget, QVBoxLayout, 
                             QHBoxLayout, QLabel, QPushButton, QComboBox, QGroupBox, QGridLayout, QSlider)
from PyQt6.QtCore import QThread, pyqtSignal, Qt, QTimer

# DİKKAT: Paket formatına bir adet 'B' (i2c_status) ve checksum eklendi. Toplam 27 Byte.
PACKET_FORMAT = '<BB B HHHHHH f h i B B'
PACKET_SIZE = 27

class SerialWorker(QThread):
    data_received = pyqtSignal(dict)
    
    def __init__(self, port, baudrate):
        super().__init__()
        self.port = port
        self.baudrate = baudrate
        self.serial_conn = None
        self.running = True

    def run(self):
        try:
            self.serial_conn = serial.Serial(self.port, self.baudrate, timeout=0.1)
            buffer = bytearray()
            
            while self.running:
                if self.serial_conn.in_waiting > 0:
                    buffer += self.serial_conn.read(self.serial_conn.in_waiting)
                    
                    while len(buffer) >= 4:
                        if buffer[0] == 0xAA and buffer[1] == 0x55:
                            msg_id = buffer[2]
                            
                            # YENİ: Paket boyutu artık 27 Byte
                            if msg_id == 0x02 and len(buffer) >= 27:
                                packet = buffer[:27]
                                calc_checksum = 0
                                
                                # 2'den 26'ya kadar checksum hesapla (26. byte gelen checksum)
                                for i in range(2, 26):
                                    calc_checksum ^= packet[i]
                                    
                                if calc_checksum == packet[26]:
                                    parsed = struct.unpack(PACKET_FORMAT, packet)
                                    
                                    # YENİ: 12. İndeksteki i2c_status byte'ını bitlerine ayır
                                    status_byte = parsed[12]
                                    rgb_online = bool(status_byte & 0x01)    # 0. Bit (0x29)
                                    as5600_online = bool(status_byte & 0x02) # 1. Bit (0x36)
                                    
                                    telemetry = {
                                        'msg_id': parsed[2],
                                        'ir_left': parsed[3], 'ir_right': parsed[4],
                                        'r': parsed[5], 'g': parsed[6], 'b': parsed[7], 'c': parsed[8],
                                        'angle': parsed[9],
                                        'dc_speed': parsed[10], 'step_pos': parsed[11],
                                        'rgb_ok': rgb_online,
                                        'as5600_ok': as5600_online
                                    }
                                    self.data_received.emit({'type': 'telemetry', 'data': telemetry})
                                buffer = buffer[27:]
                            
                            elif msg_id == 0x01 and len(buffer) >= 4:
                                self.data_received.emit({'type': 'heartbeat'})
                                buffer = buffer[4:]
                            else:
                                break
                        else:
                            buffer.pop(0)
        except Exception as e:
            print(f"Seri Port Hatası: {e}")

    def send_data(self, data_bytes):
        if self.serial_conn and self.serial_conn.is_open:
            self.serial_conn.write(data_bytes)

    def stop(self):
        self.running = False
        if self.serial_conn and self.serial_conn.is_open:
            self.serial_conn.close()

class AGVControlPanel(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle("AGV Telemetri Kontrol Paneli")
        self.resize(650, 550)
        self.worker = None
        
        self.watchdog_timer = QTimer()
        self.watchdog_timer.timeout.connect(self.set_hb_offline)
        
        self.step_timer = QTimer()
        self.step_timer.timeout.connect(self.send_step_chunk)
        self.current_step_dir = 0
        
        self.init_ui()
        
    def init_ui(self):
        main_widget = QWidget()
        self.setCentralWidget(main_widget)
        layout = QVBoxLayout(main_widget)
        
        # --- Üst Bar ---
        top_layout = QHBoxLayout()
        self.combo_ports = QComboBox()
        self.refresh_ports()
        self.btn_connect = QPushButton("Bağlan")
        self.btn_connect.clicked.connect(self.toggle_connection)
        
        self.lbl_hb_led = QLabel()
        self.lbl_hb_led.setFixedSize(20, 20)
        self.set_hb_offline()
        
        top_layout.addWidget(QLabel("Port:"))
        top_layout.addWidget(self.combo_ports)
        top_layout.addWidget(self.btn_connect)
        top_layout.addStretch()
        top_layout.addWidget(QLabel("Heartbeat Status:"))
        top_layout.addWidget(self.lbl_hb_led)
        layout.addLayout(top_layout)
        
        # --- Telemetri Grid ---
        grid = QGridLayout()
        self.lbl_ir_l = QLabel("0")
        self.lbl_ir_r = QLabel("0")
        self.lbl_rgb = QLabel("R:0 G:0 B:0 C:0")
        self.lbl_angle = QLabel("0.0°")
        self.lbl_dc = QLabel("0")
        self.lbl_step = QLabel("0")
        
        # YENİ: Sensör Durum LED'leri
        self.lbl_rgb_led = QLabel()
        self.lbl_rgb_led.setFixedSize(15, 15)
        self.set_sensor_led(self.lbl_rgb_led, False)
        
        self.lbl_as5600_led = QLabel()
        self.lbl_as5600_led.setFixedSize(15, 15)
        self.set_sensor_led(self.lbl_as5600_led, False)
        
        # Layout Yerleşimi (LED'ler değerlerin sağına eklendi)
        grid.addWidget(QLabel("<b>IR Sol (ADC):</b>"), 0, 0)
        grid.addWidget(self.lbl_ir_l, 0, 1)
        
        grid.addWidget(QLabel("<b>IR Sağ (ADC):</b>"), 1, 0)
        grid.addWidget(self.lbl_ir_r, 1, 1)
        
        # RGB Satırı: Başlık | Değer | LED
        grid.addWidget(QLabel("<b>Renk (RGB):</b>"), 2, 0)
        grid.addWidget(self.lbl_rgb, 2, 1)
        grid.addWidget(self.lbl_rgb_led, 2, 2)
        
        # AS5600 Satırı: Başlık | Değer | LED
        grid.addWidget(QLabel("<b>Manyetik Açı:</b>"), 3, 0)
        grid.addWidget(self.lbl_angle, 3, 1)
        grid.addWidget(self.lbl_as5600_led, 3, 2)
        
        grid.addWidget(QLabel("<b>Mevcut DC Hızı (PWM):</b>"), 4, 0)
        grid.addWidget(self.lbl_dc, 4, 1)
        
        grid.addWidget(QLabel("<b>Mevcut Step Konumu:</b>"), 5, 0)
        grid.addWidget(self.lbl_step, 5, 1)
        
        # Sütun genişliklerini ayarla ki LED'ler sağa yaslansın
        grid.setColumnStretch(1, 1)
        grid.setColumnStretch(2, 0)
        
        group_telemetry = QGroupBox("Canlı Sensör Verileri")
        group_telemetry.setLayout(grid)
        layout.addWidget(group_telemetry)
        
        # --- Kontroller ---
        ctrl_layout = QVBoxLayout()
        
        self.btn_hb_send = QPushButton("Sistemi Yokla / Heartbeat Gönder (FF 01)")
        self.btn_hb_send.clicked.connect(self.send_hb_request)
        self.btn_hb_send.setEnabled(False)
        ctrl_layout.addWidget(self.btn_hb_send)

        self.lbl_slider_dc_val = QLabel("DC Motor Hedef Güç: % 0")
        self.slider_dc = QSlider(Qt.Orientation.Horizontal)
        self.slider_dc.setMinimum(-100)
        self.slider_dc.setMaximum(100)
        self.slider_dc.setValue(0)
        self.slider_dc.valueChanged.connect(self.on_dc_slider_change)
        
        self.btn_send_dc = QPushButton("DC Hızını Gönder")
        self.btn_send_dc.clicked.connect(self.send_dc_command)
        self.btn_send_dc.setEnabled(False)
        
        ctrl_layout.addWidget(self.lbl_slider_dc_val)
        ctrl_layout.addWidget(self.slider_dc)
        ctrl_layout.addWidget(self.btn_send_dc)
        
        self.lbl_step_ctrl = QLabel("Step Motor Yön Kontrolü (Sürmek için Basılı Tutun):")
        step_btn_layout = QHBoxLayout()
        self.btn_step_ccw = QPushButton("<< CCW (Sola Dön)")
        self.btn_step_cw = QPushButton("CW (Sağa Dön) >>")
        
        self.btn_step_cw.pressed.connect(lambda: self.start_step_jog(1))
        self.btn_step_cw.released.connect(self.stop_step_jog)
        self.btn_step_ccw.pressed.connect(lambda: self.start_step_jog(-1))
        self.btn_step_ccw.released.connect(self.stop_step_jog)
        
        self.btn_step_ccw.setEnabled(False)
        self.btn_step_cw.setEnabled(False)
        
        step_btn_layout.addWidget(self.btn_step_ccw)
        step_btn_layout.addWidget(self.btn_step_cw)
        
        ctrl_layout.addWidget(self.lbl_step_ctrl)
        ctrl_layout.addLayout(step_btn_layout)
        
        group_ctrl = QGroupBox("Motor Kontrol Paneli")
        group_ctrl.setLayout(ctrl_layout)
        layout.addWidget(group_ctrl)

    # YENİ: Sensör LED'lerini renklendiren yardımcı fonksiyon
    def set_sensor_led(self, label, is_online):
        if is_online:
            label.setStyleSheet("background-color: #2ecc71; border-radius: 7px; border: 1px solid gray;")
        else:
            label.setStyleSheet("background-color: #e74c3c; border-radius: 7px; border: 1px solid gray;")

    def set_hb_online(self):
        self.lbl_hb_led.setStyleSheet("background-color: #2ecc71; border-radius: 10px; border: 1px solid gray;")
        self.watchdog_timer.start(2000)

    def set_hb_offline(self):
        self.lbl_hb_led.setStyleSheet("background-color: #e74c3c; border-radius: 10px; border: 1px solid gray;")
        
        # Arayüz tam yüklenmeden çağrılırsa hata vermemesi için obje kontrolü ekliyoruz
        if hasattr(self, 'lbl_rgb_led') and hasattr(self, 'lbl_as5600_led'):
            self.set_sensor_led(self.lbl_rgb_led, False)
            self.set_sensor_led(self.lbl_as5600_led, False)

    def update_gui(self, msg):
        if msg['type'] == 'telemetry':
            data = msg['data']
            self.set_hb_online() 
            self.lbl_ir_l.setText(str(data['ir_left']))
            self.lbl_ir_r.setText(str(data['ir_right']))
            self.lbl_rgb.setText(f"R:{data['r']} G:{data['g']} B:{data['b']} C:{data['c']}")
            self.lbl_angle.setText(f"{data['angle']:.2f}°")
            self.lbl_dc.setText(str(data['dc_speed']))
            self.lbl_step.setText(str(data['step_pos']))
            
            # YENİ: Sensör LED'lerini gelen bayraklara göre güncelle
            self.set_sensor_led(self.lbl_rgb_led, data.get('rgb_ok', False))
            self.set_sensor_led(self.lbl_as5600_led, data.get('as5600_ok', False))
            
        elif msg['type'] == 'heartbeat':
            self.set_hb_online()

    def toggle_connection(self):
        if self.worker is None or not self.worker.isRunning():
            port = self.combo_ports.currentText()
            if port:
                self.worker = SerialWorker(port, 115200)
                self.worker.data_received.connect(self.update_gui)
                self.worker.start()
                self.btn_connect.setText("Bağlantıyı Kes")
                self.btn_send_dc.setEnabled(True)
                self.btn_hb_send.setEnabled(True)
                self.btn_step_ccw.setEnabled(True)
                self.btn_step_cw.setEnabled(True)
                self.watchdog_timer.start(2000)
        else:
            self.worker.stop()
            self.worker.wait()
            self.worker = None
            self.btn_connect.setText("Bağlan")
            self.btn_send_dc.setEnabled(False)
            self.btn_hb_send.setEnabled(False)
            self.btn_step_ccw.setEnabled(False)
            self.btn_step_cw.setEnabled(False)
            self.watchdog_timer.stop()
            self.set_hb_offline()

    def send_hb_request(self):
        if self.worker:
            self.worker.send_data(bytes([0xFF, 0x01]))

    def on_dc_slider_change(self, value):
        self.lbl_slider_dc_val.setText(f"DC Motor Hedef Güç: % {value}")

    def send_dc_command(self):
        if self.worker:
            percent = self.slider_dc.value()
            pwm_val = int(percent * 9.99)
            speed_bytes = struct.pack('<h', pwm_val) 
            calc_checksum = 0x03 ^ speed_bytes[0] ^ speed_bytes[1]
            cmd_packet = struct.pack('<BBB', 0xAA, 0x55, 0x03) + speed_bytes + struct.pack('<B', calc_checksum)
            self.worker.send_data(cmd_packet)

    def start_step_jog(self, direction):
        self.current_step_dir = direction
        self.step_timer.start(50)

    def stop_step_jog(self):
        self.step_timer.stop()

    def send_step_chunk(self):
        if self.worker:
            ticks = 20 * self.current_step_dir
            tick_bytes = struct.pack('<h', ticks) 
            calc_checksum = 0x04 ^ tick_bytes[0] ^ tick_bytes[1]
            cmd_packet = struct.pack('<BBB', 0xAA, 0x55, 0x04) + tick_bytes + struct.pack('<B', calc_checksum)
            self.worker.send_data(cmd_packet)

    def refresh_ports(self):
        self.combo_ports.clear()
        for p in serial.tools.list_ports.comports():
            self.combo_ports.addItem(p.device)

    def closeEvent(self, event):
        if self.worker:
            self.worker.stop()
            self.worker.wait()
        event.accept()

if __name__ == '__main__':
    app = QApplication(sys.argv)
    window = AGVControlPanel()
    window.show()
    sys.exit(app.exec())