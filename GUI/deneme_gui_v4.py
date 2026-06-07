import sys
import struct
import serial
import serial.tools.list_ports
from PyQt6.QtWidgets import (QApplication, QMainWindow, QWidget, QVBoxLayout, 
                             QHBoxLayout, QLabel, QPushButton, QComboBox, QGroupBox, QGridLayout, QSlider)
from PyQt6.QtCore import QThread, pyqtSignal, Qt, QTimer

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
                    
                    while len(buffer) >= PACKET_SIZE:
                        if buffer[0] == 0xAA and buffer[1] == 0x55:
                            msg_id = buffer[2]
                            
                            if msg_id == 0x02 and len(buffer) >= PACKET_SIZE:
                                packet = buffer[:PACKET_SIZE]
                                calc_checksum = 0
                                
                                for i in range(2, PACKET_SIZE - 1):
                                    calc_checksum ^= packet[i]
                                    
                                if calc_checksum == packet[PACKET_SIZE - 1]:
                                    parsed = struct.unpack(PACKET_FORMAT, packet)
                                    
                                    status_byte = parsed[12]
                                    rgb_online = bool(status_byte & 0x01)    
                                    as5600_online = bool(status_byte & 0x02) 
                                    
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
                                buffer = buffer[PACKET_SIZE:]
                            
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
        self.setWindowTitle("AGV Tam Kontrol Paneli (DC Sürüş + Step Mekanizma)")
        self.resize(950, 600) 
        self.worker = None
        
        self.watchdog_timer = QTimer()
        self.watchdog_timer.timeout.connect(self.set_hb_offline)
        
        self.steer_timer = QTimer()
        self.steer_timer.timeout.connect(self.send_steer_chunk)
        self.current_steer_value = 0
        
        self.init_ui()
        
    def init_ui(self):
        main_widget = QWidget()
        self.setCentralWidget(main_widget)
        main_layout = QVBoxLayout(main_widget)
        
        # --- Üst Bağlantı Barı ---
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
        
        self.btn_hb_send = QPushButton("Sistemi Yokla (FF 01)")
        self.btn_hb_send.clicked.connect(self.send_hb_request)
        self.btn_hb_send.setEnabled(False)
        top_layout.addWidget(self.btn_hb_send)
        
        top_layout.addStretch()
        top_layout.addWidget(QLabel("Heartbeat Status:"))
        top_layout.addWidget(self.lbl_hb_led)
        main_layout.addLayout(top_layout)
        
        body_layout = QHBoxLayout()
        
        # ================= SOL TARAF: TELEMETRİ VERİLERİ =================
        grid_telemetry = QGridLayout()
        self.lbl_ir_l = QLabel("0")
        self.lbl_ir_r = QLabel("0")
        self.lbl_rgb = QLabel("R:0 G:0 B:0 C:0")
        self.lbl_angle = QLabel("0.0°")
        self.lbl_dc = QLabel("0")
        self.lbl_step = QLabel("0")
        
        self.lbl_rgb_led = QLabel()
        self.lbl_rgb_led.setFixedSize(15, 15)
        self.set_sensor_led(self.lbl_rgb_led, False)
        
        self.lbl_as5600_led = QLabel()
        self.lbl_as5600_led.setFixedSize(15, 15)
        self.set_sensor_led(self.lbl_as5600_led, False)
        
        grid_telemetry.addWidget(QLabel("<b>IR Sol (ADC):</b>"), 0, 0)
        grid_telemetry.addWidget(self.lbl_ir_l, 0, 1)
        
        grid_telemetry.addWidget(QLabel("<b>IR Sağ (ADC):</b>"), 1, 0)
        grid_telemetry.addWidget(self.lbl_ir_r, 1, 1)
        
        grid_telemetry.addWidget(QLabel("<b>Renk (RGB):</b>"), 2, 0)
        grid_telemetry.addWidget(self.lbl_rgb, 2, 1)
        grid_telemetry.addWidget(self.lbl_rgb_led, 2, 2)
        
        grid_telemetry.addWidget(QLabel("<b>Manyetik Açı:</b>"), 3, 0)
        grid_telemetry.addWidget(self.lbl_angle, 3, 1)
        grid_telemetry.addWidget(self.lbl_as5600_led, 3, 2)
        
        grid_telemetry.addWidget(QLabel("<b>DC Motor (PWM):</b>"), 4, 0)
        grid_telemetry.addWidget(self.lbl_dc, 4, 1)
        
        grid_telemetry.addWidget(QLabel("<b>Step Konumu:</b>"), 5, 0)
        grid_telemetry.addWidget(self.lbl_step, 5, 1)
        
        grid_telemetry.setColumnStretch(1, 1)
        
        group_telemetry = QGroupBox("Canlı Sensör Verileri")
        group_telemetry.setLayout(grid_telemetry)
        body_layout.addWidget(group_telemetry, stretch=3)
        
        # ================= SAĞ TARAF: KONTROLLER =================
        controls_layout = QVBoxLayout()
        
        # 1. BÖLÜM: DC MOTOR KONTROLÜ (İLERİ/GERİ ve SAĞ/SOL)
        dc_group = QGroupBox("AGV Sürüş Kontrolü (DC Motorlar)")
        dc_layout = QGridLayout()
        
        self.lbl_dc_fw_bw = QLabel("İleri / Geri: % 0")
        self.lbl_dc_fw_bw.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self.slider_dc_fw_bw = QSlider(Qt.Orientation.Vertical)
        self.slider_dc_fw_bw.setMinimum(-100)
        self.slider_dc_fw_bw.setMaximum(100)
        self.slider_dc_fw_bw.setValue(0)
        self.slider_dc_fw_bw.setTickPosition(QSlider.TickPosition.TicksBothSides)
        self.slider_dc_fw_bw.setStyleSheet("min-height: 150px; max-width: 40px;")
        self.slider_dc_fw_bw.valueChanged.connect(self.on_dc_fw_bw_change)
        self.slider_dc_fw_bw.sliderReleased.connect(self.reset_dc_fw_bw)
        
        self.lbl_dc_turn = QLabel("Tank Dönüşü (Sağ/Sol): % 0")
        self.lbl_dc_turn.setAlignment(Qt.AlignmentFlag.AlignCenter)
        self.slider_dc_turn = QSlider(Qt.Orientation.Horizontal)
        self.slider_dc_turn.setMinimum(-100)
        self.slider_dc_turn.setMaximum(100)
        self.slider_dc_turn.setValue(0)
        self.slider_dc_turn.setTickPosition(QSlider.TickPosition.TicksBothSides)
        self.slider_dc_turn.setStyleSheet("min-width: 200px;")
        self.slider_dc_turn.valueChanged.connect(self.on_dc_turn_change)
        self.slider_dc_turn.sliderReleased.connect(self.reset_dc_turn)
        
        dc_layout.addWidget(self.lbl_dc_fw_bw, 0, 0)
        dc_layout.addWidget(self.slider_dc_fw_bw, 1, 0, 2, 1, Qt.AlignmentFlag.AlignCenter)
        dc_layout.addWidget(self.lbl_dc_turn, 0, 1)
        dc_layout.addWidget(self.slider_dc_turn, 1, 1, 1, 1, Qt.AlignmentFlag.AlignCenter)
        
        dc_group.setLayout(dc_layout)
        controls_layout.addWidget(dc_group)
        
        # 2. BÖLÜM: STEP MOTOR KONTROLÜ (HARİCİ MEKANİZMA)
        step_group = QGroupBox("Harici Mekanizma Kontrolü (Step Motor)")
        step_layout = QVBoxLayout()
        
        self.lbl_step_val = QLabel("Durum: Merkez")
        self.lbl_step_val.setAlignment(Qt.AlignmentFlag.AlignCenter)
        
        self.slider_step = QSlider(Qt.Orientation.Horizontal)
        self.slider_step.setMinimum(-100)
        self.slider_step.setMaximum(100)
        self.slider_step.setValue(0)
        self.slider_step.setTickPosition(QSlider.TickPosition.TicksBothSides)
        
        self.slider_step.valueChanged.connect(self.on_step_change)
        self.slider_step.sliderReleased.connect(self.reset_step)
        
        step_h_layout = QHBoxLayout()
        lbl_ccw = QLabel("<b><< CCW</b>")
        lbl_ccw.setStyleSheet("color: #e74c3c;")
        lbl_cw = QLabel("<b>CW >></b>")
        lbl_cw.setStyleSheet("color: #2ecc71;")
        
        step_h_layout.addWidget(lbl_ccw)
        step_h_layout.addWidget(self.slider_step)
        step_h_layout.addWidget(lbl_cw)
        
        step_layout.addWidget(self.lbl_step_val)
        step_layout.addLayout(step_h_layout)
        
        step_group.setLayout(step_layout)
        controls_layout.addWidget(step_group)
        
        body_layout.addLayout(controls_layout, stretch=4)
        main_layout.addLayout(body_layout)
        
        self.set_controls_enabled(False)

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
        if hasattr(self, 'lbl_rgb_led') and hasattr(self, 'lbl_as5600_led'):
            self.set_sensor_led(self.lbl_rgb_led, False)
            self.set_sensor_led(self.lbl_as5600_led, False)

    def set_controls_enabled(self, enabled):
        self.slider_dc_fw_bw.setEnabled(enabled)
        self.slider_dc_turn.setEnabled(enabled)
        self.slider_step.setEnabled(enabled)
        self.btn_hb_send.setEnabled(enabled)

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
                self.set_controls_enabled(True)
                self.watchdog_timer.start(2000)
        else:
            self.worker.stop()
            self.worker.wait()
            self.worker = None
            self.btn_connect.setText("Bağlan")
            self.set_controls_enabled(False)
            self.watchdog_timer.stop()
            self.set_hb_offline()

    def send_hb_request(self):
        if self.worker:
            self.worker.send_data(bytes([0xFF, 0x01]))

    # --- 1. DC Motor İleri / Geri (MSG ID: 0x03) ---
    def on_dc_fw_bw_change(self, value):
        self.lbl_dc_fw_bw.setText(f"İleri / Geri: % {value}")
        if self.worker:
            pwm_val = int(value * 9.99)
            speed_bytes = struct.pack('<h', pwm_val) 
            calc_checksum = 0x03 ^ speed_bytes[0] ^ speed_bytes[1]
            cmd_packet = struct.pack('<BBB', 0xAA, 0x55, 0x03) + speed_bytes + struct.pack('<B', calc_checksum)
            self.worker.send_data(cmd_packet)

    def reset_dc_fw_bw(self):
        self.slider_dc_fw_bw.setValue(0)

    # --- 2. DC Motor Tank Dönüşü Sağ / Sol (MSG ID: 0x05) ---
    def on_dc_turn_change(self, value):
        self.lbl_dc_turn.setText(f"Tank Dönüşü: % {value}")
        if self.worker:
            pwm_val = int(value * 9.99)
            speed_bytes = struct.pack('<h', pwm_val) 
            calc_checksum = 0x05 ^ speed_bytes[0] ^ speed_bytes[1] # Yeni MSG_ID: 0x05
            cmd_packet = struct.pack('<BBB', 0xAA, 0x55, 0x05) + speed_bytes + struct.pack('<B', calc_checksum)
            self.worker.send_data(cmd_packet)

    def reset_dc_turn(self):
        self.slider_dc_turn.setValue(0)

    # --- 3. Step Motor Kontrolü (MSG ID: 0x04) ---
    def on_step_change(self, value):
        if value < 0:
            self.lbl_step_val.setText(f"CCW Yönünde % {abs(value)} Gücünde Dönüyor")
        elif value > 0:
            self.lbl_step_val.setText(f"CW Yönünde % {value} Gücünde Dönüyor")
        else:
            self.lbl_step_val.setText("Durum: Merkez")
            
        if value != 0:
            self.current_steer_value = value
            if not self.steer_timer.isActive():
                self.steer_timer.start(50) 
        else:
            self.steer_timer.stop()

    def reset_step(self):
        self.slider_step.setValue(0)
        self.steer_timer.stop()

    def send_steer_chunk(self):
        if self.worker and self.current_steer_value != 0:
            ticks = int(self.current_steer_value * 0.3)
            if ticks == 0 and self.current_steer_value > 0: ticks = 1
            if ticks == 0 and self.current_steer_value < 0: ticks = -1
            
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
