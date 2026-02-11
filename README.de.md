# FilaMan - Filament Management System (Erweiterte Fork)

[English Version](README.md)

> **Hinweis:** Dies ist eine erweiterte Fork von [ManuelW77/Filaman](https://github.com/ManuelW77/Filaman). Die wichtigste Ergänzung ist die **Anycubic ACE Pro NFC-Tag-Unterstützung** — Tags können so beschrieben werden, dass sie sowohl vom Bambu Lab AMS als auch vom Anycubic ACE Pro lesbar sind. Weitere kleinere Verbesserungen sind unten aufgeführt.

FilaMan ist ein Filament-Managementsystem für den 3D-Druck. Es verwendet ESP32-Hardware für Gewichtsmessungen und NFC-Tag-Management.
Benutzer können Filamentspulen verwalten, den Status des Automatic Material System (AMS) von Bambulab Druckern überwachen und Einstellungen über eine Weboberfläche vornehmen.
Das System integriert sich nahtlos mit [Bambulab](https://bambulab.com/en-us) 3D-Druckern, [Spoolman](https://github.com/Donkie/Spoolman) und dem [Openspool](https://github.com/spuder/OpenSpool) NFC-TAG Format.

Allgemeine Dokumentation, Hardware-Aufbau und Original-Features sind im [Original FilaMan Wiki](https://github.com/ManuelW77/Filaman/wiki) zu finden.

---

## ✨ Was diese Fork unterscheidet

### 🏷️ Anycubic ACE Pro NFC-Tag-Unterstützung
Das Hauptfeature dieser Fork: NFC-Tags können jetzt in einem **Hybrid-Format** beschrieben werden, das sowohl vom Bambu Lab AMS (über den [OpenSpool](https://github.com/spuder/OpenSpool)-Standard) als auch vom Anycubic ACE Pro automatischen Filamentsystem lesbar ist.

Da Bambu Lab proprietäre, RSA-signierte RFID-Tags verwendet, setzen Drittanbieter-Tags auf das OpenSpool-Format. Der Anycubic ACE Pro nutzt ein eigenes binäres NFC-Format. Diese Fork schreibt beide Formate auf einen einzigen NTAG215-Tag — ein Tag funktioniert damit in beiden Drucker-Ökosystemen.

### Kleinere Verbesserungen
- **Mehrfarbige Filament-Anzeige:** Visuelle Darstellung mehrfarbiger Filamente in der Weboberfläche
- **Temperaturbereiche:** Bett- und Düsen-Temperaturbereiche werden in der Weboberfläche angezeigt
- **Erweiterte Kalibrierungszeit:** 10-Sekunden-Fenster für präzisere Waagen-Kalibrierung

---

## Installation

### ⚡ Einfache Installation (Empfohlen)

1. Den [FilaMan Web Installer](https://anzarion.github.io/Filaman/) öffnen
2. ESP32 per USB anschließen und auf **Connect** klicken
3. Den Geräte-Port auswählen und auf **Install** klicken
4. Nach dem Flashen mit dem WLAN **"FilaMan"** verbinden
5. WiFi-Einstellungen über das Captive Portal konfigurieren
6. Weboberfläche unter `http://filaman.local` oder der Geräte-IP aufrufen

### 🔧 Manuelle Installation (PlatformIO)

1. [VS Code](https://code.visualstudio.com/) und die [PlatformIO Extension](https://platformio.org/install/ide?install=vscode) installieren
2. Repository klonen:
   ```bash
   git clone https://github.com/Anzarion/Filaman.git
   cd Filaman
   ```
3. Ordner in VS Code öffnen — PlatformIO installiert die Abhängigkeiten automatisch
4. ESP32 per USB anschließen und auf **Upload** unter PROJECT TASKS → `env:esp32dev` klicken
5. Ersteinrichtung wie in der einfachen Installation ab Schritt 4 durchführen

### Wichtiger Hinweis zu Spoolman

Spoolman muss im Debug-Modus betrieben werden, da CORS-Domains in Spoolman bisher nicht konfigurierbar sind:

```
SPOOLMAN_DEBUG_MODE=TRUE
```

---

## Credits & Lizenz

- **Original FilaMan:** Erstellt von [ManuelW77](https://github.com/ManuelW77) — [Repository](https://github.com/ManuelW77/Filaman)
- **Erweiterte Fork:** Gepflegt von [Anzarion](https://github.com/Anzarion)
- Lizenziert unter der MIT-Lizenz. Siehe [LICENSE](LICENSE) für Details.

### Original-Ersteller unterstützen

<a href="https://www.buymeacoffee.com/manuelw" target="_blank"><img src="https://cdn.buymeacoffee.com/buttons/v2/default-yellow.png" alt="Buy Me A Coffee" style="height: 30px !important;width: 108px !important;" ></a>
