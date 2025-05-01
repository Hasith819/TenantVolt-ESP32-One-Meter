# TenantVolt - ESP32 Smart Electricity Meter

A smart electricity monitoring system using ESP32 that measures voltage, current, and power consumption with remote control via Firebase.

## ✨ Features
- Real-time voltage/current monitoring
- Power consumption calculation
- Firebase cloud integration
- Remote connection control (ON/OFF)
- LCD display interface
- Automatic time synchronization (NTP)

## 🛠️ Hardware Components
| Component | Specification |
|-----------|---------------|
| ESP32 | ESP-WROOM-32 |
| Voltage Sensor | ZMPT101B |
| Current Sensor | ACS712 (20A) |
| Relay Module | 5V 10A |
| LCD Display | I2C 16x4 |
| Power Supply | 5V 2A |



### Hardware Connections

| ESP32 Pin | Connected To |
|-----------|--------------|
| GPIO34 | ZMPT101B Output |
| GPIO35 | ACS712 Output |
| GPIO23 | Relay Control |
| GPIO21 (SDA) | LCD I2C SDA |
| GPIO22 (SCL) | LCD I2C SCL |
| 3.3V | Sensors VCC |
| GND | Common Ground |



⚠️ Safety Notes
Use proper insulation for AC connections

Never work on live circuits

Double-check wiring before powering on

Use 3.3V logic level for ESP32

