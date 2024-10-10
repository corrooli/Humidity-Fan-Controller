# Humidity and Temperature Control System (WIP)

This project is an ESP32-based system designed to monitor and control humidity levels using a DHT22 sensor and a relay module. The system displays temperature and humidity data on an I2C LCD and automatically activates a fan when humidity levels exceed defined thresholds.

**Note:** This is still a work in progress. Contributions and suggestions are welcome!

## Features
- Monitors temperature and humidity using a DHT22 sensor.
- Displays readings on a 16x2 LCD with I2C interface.
- Controls a relay to activate/deactivate a fan based on humidity levels.
- Includes error handling for sensor malfunctions.

## Devices Required
- **ESP32 Development Board** (e.g., ESP32-WROOM-32D)
- **DHT22 Temperature and Humidity Sensor**
- **Relay Module** (for fan control)
- **16x2 LCD with I2C Interface** (address 0x27 recommended)
- **Connecting Wires**
- **Power Supply** (suitable for ESP32 and connected devices)
- **Optional: LED** (to indicate relay state)

## Installation
1. Connect the devices according to the pin definitions in the code:
   - DHT22: Data pin connected to GPIO 4.
   - Relay: Control pin connected to GPIO 16.
   - LCD: SDA and SCL connected to ESP32’s I2C pins.
   - LED: Connected to GPIO 2.
2. Upload the code to your ESP32 using Arduino IDE.

## Libraries Required
- **WiFi.h** (included in ESP32 core)
- **Wire.h** (for I2C communication)
- **LiquidCrystal_PCF8574** (for LCD control over I2C)
- **DHTesp** (by beegee-tokyo, for reading DHT sensors)

## Known Issues
- Wi-Fi is disabled to prevent interference with other components.
- The system currently supports only DHT22 sensors.

## Future Improvements
- Implement Wi-Fi-based monitoring and control via a web interface.
- Add support for additional sensor types.
- Optimize power consumption.

## License
This project is licensed under the GNU GPL v3 License. See the LICENSE file for details.

## Contributions
Feel free to open issues or submit pull requests if you find bugs or want to contribute to the project!
