# FilaMan - Filament Management System (Enhanced Fork)

[Deutsche Version](README.de.md)

> **Note:** This is an enhanced fork of [ManuelW77/Filaman](https://github.com/ManuelW77/Filaman). The key addition is **Anycubic ACE Pro NFC tag support** — you can write tags that are readable by both Bambu Lab AMS and Anycubic ACE Pro printers. Additional cosmetic improvements are listed below.

FilaMan is a filament management system for 3D printing. It uses ESP32 hardware for weight measurement and NFC tag management.
Users can manage filament spools, monitor the status of the Automatic Material System (AMS) and make settings via a web interface.
The system integrates seamlessly with [Bambulab](https://bambulab.com/en-us) 3D printers and [Spoolman](https://github.com/Donkie/Spoolman) filament management as well as the [Openspool](https://github.com/spuder/OpenSpool) NFC-TAG format.

For general documentation, hardware setup, and original features see the [original FilaMan Wiki](https://github.com/ManuelW77/Filaman/wiki).

---

## ✨ What's Different in This Fork

### 🏷️ Anycubic ACE Pro NFC Tag Support
The main feature of this fork: NFC tags can now be written in a **hybrid format** that is readable by both the Bambu Lab AMS (via the [OpenSpool](https://github.com/spuder/OpenSpool) standard) and the Anycubic ACE Pro automatic filament system.

Since Bambu Lab uses proprietary RSA-signed RFID tags, third-party tags rely on the OpenSpool format. The Anycubic ACE Pro uses its own binary NFC format. This fork writes both formats onto a single NTAG215 tag, so one tag works with both printer ecosystems.

### Minor Improvements
- **Multi-color filament display:** Visual representation of multi-color filaments in the web interface
- **Temperature ranges:** Bed and nozzle temperature ranges shown in the web interface
- **Extended calibration time:** 10-second window for more accurate scale calibration

---

## Installation

### ⚡ Easy Installation (Recommended)

1. Go to the [FilaMan Web Installer](https://anzarion.github.io/Filaman/)
2. Plug in your ESP32 via USB and click **Connect**
3. Select your device port and click **Install**
4. After flashing, connect to the **"FilaMan"** WiFi access point
5. Configure your WiFi settings through the captive portal
6. Access the web interface at `http://filaman.local` or the device IP

### 🔧 Manual Installation (PlatformIO)

1. Install [VS Code](https://code.visualstudio.com/) and the [PlatformIO Extension](https://platformio.org/install/ide?install=vscode)
2. Clone this repository:
   ```bash
   git clone https://github.com/Anzarion/Filaman.git
   cd Filaman
   ```
3. Open the folder in VS Code — PlatformIO will install dependencies automatically
4. Connect your ESP32 via USB and click **Upload** under PROJECT TASKS → `env:esp32dev`
5. Follow the initial setup steps from the Easy Installation above (steps 4–6)

### Important Note about Spoolman

You have to activate Spoolman in debug mode, because CORS domains cannot be configured in Spoolman yet:

```
SPOOLMAN_DEBUG_MODE=TRUE
```

---

## Credits & License

- **Original FilaMan:** Created by [ManuelW77](https://github.com/ManuelW77) — [Repository](https://github.com/ManuelW77/Filaman)
- **Enhanced Fork:** Maintained by [Anzarion](https://github.com/Anzarion)
- Licensed under the MIT License. See [LICENSE](LICENSE) for details.

### Support the Original Creator

<a href="https://www.buymeacoffee.com/manuelw" target="_blank"><img src="https://cdn.buymeacoffee.com/buttons/v2/default-yellow.png" alt="Buy Me A Coffee" style="height: 30px !important;width: 108px !important;" ></a>
