# FilaMan - Filament Management System (Enhanced Fork)

[Deutsche Version](README.de.md)

> **Note:** This is an enhanced fork of [ManuelW77/Filaman](https://github.com/ManuelW77/Filaman) with additional features for multi-color filaments, Anycubic ACE Pro NFC tag support, and enhanced web interface capabilities.

FilaMan is a filament management system for 3D printing. It uses ESP32 hardware for weight measurement and NFC tag management.
Users can manage filament spools, monitor the status of the Automatic Material System (AMS) and make settings via a web interface.
The system integrates seamlessly with [Bambulab](https://bambulab.com/en-us) 3D printers and [Spoolman](https://github.com/Donkie/Spoolman) filament management as well as the [Openspool](https://github.com/spuder/OpenSpool) NFC-TAG format.


![Scale](./img/scale_trans.png)


More Images can be found in the [img Folder](/img/)
Original website: [FilaMan Website](https://www.filaman.app)
German explanatory video: [Youtube](https://youtu.be/uNDe2wh9SS8?si=b-jYx4I1w62zaOHU)
Discord Server: [https://discord.gg/my7Gvaxj2v](https://discord.gg/my7Gvaxj2v)

---

## ✨ Enhanced Features in This Fork

This fork adds several powerful enhancements to the original FilaMan project:

### 🎨 Multi-Color Filament Support
- **Multi-Color Display:** Visual representation of multi-color filaments in the web interface
- **Enhanced Color Validation:** Support for RGB and RGBA color formats with detailed validation
- **Large Color Display:** Improved 2-column layout showing filament colors prominently
- **Extended NDEF Support:** Handles large payloads (>255 bytes) for complex filament data

### 🏷️ Anycubic ACE Pro NFC Tag Support
- **ACE Pro Tag Writing:** Write NFC tags compatible with Anycubic ACE Pro printers
- **Hybrid Format:** Supports both FilaMan JSON format and ACE Pro binary format
- **Automatic SKU Generation:** Creates unique identifiers for each spool
- **Extended Brand/Material Fields:** Support for full brand names and material designations
- **Color Format Conversion:** Automatic conversion between RGB and ABGR formats

### 📊 Enhanced Web Interface
- **Bed Temperature Display:** Shows bed temperature settings for each filament
- **Temperature Ranges:** Displays both nozzle and bed temperature ranges
- **SKU Information:** Shows unique spool identifiers
- **Improved NFC Card Layout:** Better organized filament information display
- **Real-time Cache Updates:** Improved JavaScript reload mechanism

### ⚖️ Improved Scale Functionality
- **Extended Calibration Time:** 10-second calibration window for more accurate weight setup
- **Better User Feedback:** Enhanced calibration process with clearer instructions

### 🔌 Enhanced API Integration
- **Multi-Color API Support:** Extracts and displays multi-color hex values from Spoolman
- **Temperature Field Extraction:** Properly extracts nozzle and bed temperature settings
- **Fallback Matching:** Intelligent brand and material matching with fallback support
- **Backward Compatibility:** Maintains compatibility with existing Spoolman installations

---

## NEW: Recycling Fabrik

<a href="https://www.recyclingfabrik.com" target="_blank">
    <img src="img/rf-logo.png" alt="Recycling Fabrik" width="200">
</a>

FilaMan is supported by [Recycling Fabrik](https://www.recyclingfabrik.com).
Recycling Fabrik will soon offer a FilaMan-compatible NFC tag on their spools. This has the advantage
that the spools can be automatically recognized and imported into Spoolman directly via FilaMan.

**What is Recycling Fabrik?**

Recycling Fabrik is a German company dedicated to developing and manufacturing sustainable 3D printing filament.
Their filaments are made from 100% recycled material from both end customers and industry – for an environmentally conscious and resource-saving future.

More information and products can be found here: [www.recyclingfabrik.com](https://www.recyclingfabrik.com)

---

### For detailed information about the original FilaMan: [Wiki](https://github.com/ManuelW77/Filaman/wiki)

### ESP32 Hardware Features
- **Weight Measurement:** Using a load cell with HX711 amplifier for precise weight tracking.
- **NFC Tag Reading/Writing:** PN532 module for reading and writing filament data to NFC tags.
- **OLED Display:** Shows current weight, connection status (WiFi, Bambu Lab, Spoolman).
- **WiFi Connectivity:** WiFiManager for easy network configuration.
- **MQTT Integration:** Connects to Bambu Lab printer for AMS control.
- **NFC-Tag NTAG213 NTAG215:** Use NTAG213, better NTAG215 because of enough space on the Tag

### Web Interface Features
- **Real-time Updates:** WebSocket connection for live data updates.
- **NFC Tag Management:**
	- Write filament data to NFC tags
	- Support for multiple NFC tag formats:
		- [Openspool](https://github.com/spuder/OpenSpool) format for Bambu Lab AMS
		- Anycubic ACE Pro binary format
	- Automatic Spool detection in AMS
	- **Manufacturer Tag Support:** Automatic creation of Spoolman entries from manufacturer NFC tags
- **Enhanced Filament Display:**
	- Multi-color filament visualization
	- Bed and nozzle temperature ranges
	- SKU and material information
	- Large color preview
- **Bambulab AMS Integration:**
  - Display current AMS tray contents
  - Assign filaments to AMS slots
  - Support for external spool holder
- **Spoolman Integration:**
  - List available filament spools
  - Filter and select filaments
  - Update spool weights automatically
  - Track NFC tag assignments
  - Multi-color filament support
  - Supports Spoolman Octoprint Plugin

### If you want to support the original creator's work:

<a href="https://www.buymeacoffee.com/manuelw" target="_blank"><img src="https://cdn.buymeacoffee.com/buttons/v2/default-yellow.png" alt="Buy Me A Coffee" style="height: 30px !important;width: 108px !important;" ></a>

## Manufacturer Tags Support

🎉 **Exciting News!** FilaMan now supports **Manufacturer Tags** - NFC tags that come pre-programmed directly from filament manufacturers!

### First Manufacturer Partner: RecyclingFabrik

We're thrilled to announce that [**RecyclingFabrik**](https://www.recyclingfabrik.de) will be the **first filament manufacturer** to support FilaMan by offering NFC tags in the FilaMan format on their spools!

**Coming Soon:** RecyclingFabrik spools will include NFC tags that automatically integrate with your FilaMan system, eliminating manual setup and ensuring perfect compatibility.

### How Manufacturer Tags Work

When you scan a manufacturer NFC tag for the first time:
1. **Automatic Brand Detection:** FilaMan recognizes the manufacturer and creates the brand in Spoolman
2. **Filament Type Creation:** All material specifications are automatically added
3. **Spool Registration:** Your specific spool is registered with proper weight and specifications
4. **Future Fast Recognition:** Subsequent scans use fast-path detection for instant weight measurement

### Benefits for Users
- ✅ **Zero Manual Setup** - Just scan and weigh
- ✅ **Perfect Data Accuracy** - Manufacturer-verified specifications
- ✅ **Instant Integration** - Seamless Spoolman compatibility
- ✅ **Future-Proof** - Tags work with any FilaMan-compatible system

## Detailed Functionality

### ESP32 Functionality
- **Control and Monitor Print Jobs:** The ESP32 communicates with the Bambu Lab printer to control and monitor print jobs.
- **Printer Communication:** Uses MQTT for real-time communication with the printer.
- **User Interactions:** The OLED display provides immediate feedback on the system status, including weight measurements and connection status.
- **Improved Calibration:** Extended calibration timing for more accurate weight measurements.

### Web Interface Functionality
- **User Interactions:** The web interface allows users to interact with the system, select filaments, write NFC tags, and monitor AMS status.
- **Enhanced UI Elements:**
	- Dropdowns for selecting manufacturers and filaments
	- Buttons for writing NFC tags in multiple formats
	- Real-time status indicators
	- Multi-color filament visualization
	- Temperature range displays
	- SKU information panels

## Hardware Requirements

### Components (Affiliate Links)
- **ESP32 Development Board:** Any ESP32 variant.
[Amazon Link](https://amzn.to/3FHea6D)
- **HX711 5kg Load Cell Amplifier:** For weight measurement.
[Amazon Link](https://amzn.to/4ja1KTe)
- **OLED 0.96 Zoll I2C white/yellow Display:** 128x64 SSD1306.
[Amazon Link](https://amzn.to/445aaa9)
- **PN532 NFC NXP RFID-Modul V3:** For NFC tag operations.
[Amazon Link](https://amzn.eu/d/gy9vaBX)
- **NFC Tags NTAG213 NTAG215:** RFID Tag
[Amazon Link](https://amzn.to/3E071xO)
- **TTP223 Touch Sensor (optional):** For reTARE per Button/Touch
[Amazon Link](https://amzn.to/4hTChMK)


### Pin Configuration
| Component          | ESP32 Pin |
|-------------------|-----------|
| HX711 DOUT        | 16        |
| HX711 SCK         | 17        |
| OLED SDA          | 21        |
| OLED SCL          | 22        |
| PN532 IRQ         | 32        |
| PN532 RESET       | 33        |
| PN532 SDA         | 21        |
| PN532 SCL         | 22        |
| TTP223 I/O        | 25        |

**!! Make sure that the DIP switches on the PN532 are set to I2C**
**Use the 3V pin from the ESP for the touch sensor**

![Wiring](./img/Schaltplan.png)

![myWiring](./img/IMG_2589.jpeg)
![myWiring](./img/IMG_2590.jpeg)

*The load cell is connected to most HX711 modules as follows:
E+ red
E- black
A- white
A+ green*

## Software Dependencies

### ESP32 Libraries
- `WiFiManager`: Network configuration
- `ESPAsyncWebServer`: Web server functionality
- `ArduinoJson`: JSON parsing and creation
- `PubSubClient`: MQTT communication
- `Adafruit_PN532`: NFC functionality
- `Adafruit_SSD1306`: OLED display control
- `HX711`: Load cell communication

## Installation

### Prerequisites
- **Software:**
  - [VS Code](https://code.visualstudio.com/)
  - [PlatformIO Extension](https://platformio.org/install/ide?install=vscode) for VS Code
  - [Spoolman](https://github.com/Donkie/Spoolman) instance
- **Hardware:**
  - ESP32 Development Board
  - HX711 Load Cell Amplifier
  - Load Cell (weight sensor)
  - OLED Display (128x64 SSD1306)
  - PN532 NFC Module
  - Connecting wires

### Important Note about Spoolman
You have to activate Spoolman in debug mode, because you are not able to set CORS Domains in Spoolman yet:

```
# Enable debug mode
# If enabled, the client will accept requests from any host
# This can be useful when developing, but is also a security risk
# Default: FALSE
SPOOLMAN_DEBUG_MODE=TRUE
```

### Step-by-Step Installation

1. **Install VS Code and PlatformIO:**
   - Download and install [VS Code](https://code.visualstudio.com/)
   - Open VS Code and install the [PlatformIO Extension](https://platformio.org/install/ide?install=vscode)

2. **Clone this Repository:**
   ```bash
   git clone https://github.com/Anzarion/Filaman.git
   cd Filaman
   ```

3. **Open Project in VS Code:**
   - Open VS Code
   - Click "File" → "Open Folder"
   - Select the cloned `Filaman` folder

4. **Install Dependencies:**
   - PlatformIO will automatically install dependencies when you first open the project
   - Alternatively: Open PlatformIO terminal and run:
     ```bash
     pio lib install
     ```

5. **Connect and Flash the ESP32:**
   - Connect your ESP32 via USB
   - In VS Code: Click the PlatformIO icon in the sidebar
   - Click "Upload" under PROJECT TASKS → env:esp32dev

6. **Initial Setup:**
   - After flashing, the ESP32 creates a WiFi network named "FilaMan"
   - Connect to this network
   - Configure your WiFi settings through the captive portal
   - After connection, access the web interface at `http://filaman.local` or the IP address

## Documentation

### Relevant Links
- [Original FilaMan Repository](https://github.com/ManuelW77/Filaman)
- [PlatformIO Documentation](https://docs.platformio.org/)
- [Spoolman Documentation](https://github.com/Donkie/Spoolman)
- [Bambu Lab Printer Documentation](https://www.bambulab.com/)

### Tutorials and Examples
- [PlatformIO Getting Started](https://docs.platformio.org/en/latest/tutorials/espressif32/arduino_debugging_unit_testing.html)
- [ESP32 Web Server Tutorial](https://randomnerdtutorials.com/esp32-web-server-arduino-ide/)

## License

This project is licensed under the MIT License. See the [LICENSE](LICENSE) file for details.

## Materials

### Useful Resources
- [ESP32 Official Documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/)
- [Arduino Libraries](https://www.arduino.cc/en/Reference/Libraries)
- [NFC Tag Information](https://learn.adafruit.com/adafruit-pn532-rfid-nfc/overview)

### Community and Support
- [PlatformIO Community](https://community.platformio.org/)
- [Arduino Forum](https://forum.arduino.cc/)
- [ESP32 Forum](https://www.esp32.com/)
- [Original FilaMan Discord Server](https://discord.gg/my7Gvaxj2v)

## Availability

**Original FilaMan:** The original code can be tested and downloaded from the [official repository](https://github.com/ManuelW77/Filaman).

**This Enhanced Fork:** Available at [https://github.com/Anzarion/Filaman](https://github.com/Anzarion/Filaman)

---

## Credits

- **Original FilaMan:** Created by [ManuelW77](https://github.com/ManuelW77)
- **Enhanced Fork:** Maintained by [Anzarion](https://github.com/Anzarion)

### Support the Original Creator

If you want to support Manuel's amazing work on the original FilaMan project:

<a href="https://www.buymeacoffee.com/manuelw" target="_blank"><img src="https://cdn.buymeacoffee.com/buttons/v2/default-yellow.png" alt="Buy Me A Coffee" style="height: 30px !important;width: 108px !important;" ></a>
