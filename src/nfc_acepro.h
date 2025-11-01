#ifndef NFC_ACEPRO_H
#define NFC_ACEPRO_H

#include <Arduino.h>
#include <ArduinoJson.h>

/**
 * ACE Pro NFC Tag Data Structure
 * Maps to NTAG215 pages following PR #29 extended format
 * 
 * NTAG215 Layout:
 * - Pages 0-3:    Standard (UID, CC)
 * - Pages 4-34:   ACE Pro Binary Format
 * - Pages 35-39:  sm_id (Spoolman ID for unique identification)
 * - Pages 40+:    NDEF (JSON format for NFC apps)
 * - Pages 130-132: Config/Lock
 * 
 * Page Layout (Binary Part):
 * - Pages 5-8:    SKU (16 bytes)
 * - Pages 10-14:  Brand (20 bytes) [PR #29 Extended]
 * - Pages 15-19:  Material (20 bytes) [PR #29 Extended]
 * - Page 20:      Color ABGR (4 bytes) [PRINTER USES THIS]
 * - Page 24:      Nozzle Temperature (4 bytes)
 * - Page 29:      Bed Temperature (4 bytes)
 * - Page 30:      Diameter + Length (4 bytes)
 * - Page 31:      Weight (4 bytes)
 * - Pages 35-39:  Spoolman ID (sm_id) [UNIQUE IDENTIFICATION]
 */
struct ACEProData {
    // Pages 5-8: SKU (16 bytes, null-terminated)
    // Format: "VENDOR-MATERIAL-CODE" (max 15 chars + null terminator)
    // Example: "SUNL-PLA-123" or "BAMB-PETG-456"
    char sku[16];
    
    // Pages 10-14: Brand/Manufacturer (20 bytes, null-terminated)
    // Format: Full vendor name with umlaut conversion
    // Examples: "Anycubic", "eSUN", "Bambu Lab", "Prusament", "SUNLU"
    // NOTE: Printer ignores this; for user reference
    char brand[20];
    
    // Pages 15-19: Material type (20 bytes, null-terminated)
    // Format: Material designation (max 18 chars + null terminator)
    // Examples: "PLA", "PLA+", "PETG", "ASA Carbon", "ABS-GF", "TPU"
    // NOTE: Printer ignores this; for user reference
    char material[20];
    
    // Page 20: Color in ABGR format (4 bytes)
    // Format: [Alpha=0xFF, Blue, Green, Red]
    // Example: "#FF5533" (RGB) → [0xFF, 0x33, 0x55, 0xFF] (ABGR)
    // NOTE: Printer reads and displays this value
    uint8_t colorABGR[4];
    
    // Page 24: Nozzle Temperature (4 bytes, 2×uint16 Little-Endian)
    // Format: Min temp (bytes 0-1), Max temp (bytes 2-3) in °C
    // Example: 200°C min, 230°C max
    // NOTE: Printer ignores; for user reference
    uint16_t nozzleTempMin;  // °C
    uint16_t nozzleTempMax;  // °C
    
    // Page 29: Bed Temperature (4 bytes, 2×uint16 Little-Endian)
    // Format: Min temp (bytes 0-1), Max temp (bytes 2-3) in °C
    // Fallback if not in Spoolman: Material-dependent
    // NOTE: Printer ignores; for user reference
    uint16_t bedTempMin;     // °C
    uint16_t bedTempMax;     // °C
    
    // Page 30 (bytes 0-1): Filament Diameter (uint16 Little-Endian)
    // Unit: 0.01mm (175 = 1.75mm)
    // Page 30 (bytes 2-3): Spool Length (uint16 Little-Endian)
    // Unit: Meters (typically 330m)
    // NOTE: Printer ignores; for user reference
    uint16_t diameterX100;   // Diameter * 100 (e.g., 175 for 1.75mm)
    uint16_t lengthMeters;   // Spool length in meters
    
    // Page 31: Weight (4 bytes, uint16 Little-Endian)
    // Unit: grams (remaining filament weight)
    // NOTE: Printer ignores; for user reference
    uint16_t weightGrams;    // Remaining weight in grams
    
    // Pages 35-39: Spoolman ID (sm_id) [20 bytes, null-terminated]
    // Format: ASCII decimal string representation of Spoolman spool ID
    // Example: "12345" (max 19 chars + null terminator)
    // NOTE: Enables 100% unique identification for weight updates
    // NOTE: Printer doesn't read this; FilaMan-specific extension
    char spoolId[20];
};

// ===== Helper Functions =====

/**
 * Generate SKU from Spoolman filament data
 * Format: VENDOR(4 chars max)-MATERIAL-SPOOLID
 * Example: "SUNL-PLA-123"
 * Output: Unique SKU per spool (max 15 chars + null terminator)
 */
String getSKU(const JsonObject& spoolData);

/**
 * Extract brand name from Spoolman
 * Handles UTF-8 umlauts by converting to ASCII
 * Output: ASCII brand name (max 18 chars + null terminator)
 */
String getBrand(const JsonObject& spoolData);

/**
 * Extract material name from Spoolman
 * Output: ASCII material designation (max 18 chars + null terminator)
 */
String getMaterial(const JsonObject& spoolData);

/**
 * Convert RGB hex from Spoolman to ABGR format for ACE Pro
 * Input:  "#FF5533" (RGB hex)
 * Output: 0xFF3357FF (ABGR uint32_t)
 */
uint32_t getColor(const JsonObject& spoolData);

/**
 * Auto-detect bed temperature based on material type
 * Used when Spoolman doesn't provide bed temperature settings
 */
void getDefaultBedTemp(const String& material, uint16_t& minTemp, uint16_t& maxTemp);

/**
 * Convert 16-bit value to Little-Endian bytes
 * Required for all multi-byte fields (temps, diameter, weight)
 */
void toLE16(uint16_t value, uint8_t* bytes);

/**
 * Write single page + verify immediately
 * Returns: true if success, false if all retries failed
 */
bool writePageVerify(uint8_t page, uint8_t* data);

/**
 * Write multiple pages (up to 5 pages) + verify immediately
 * Returns: true if success, false if any page write failed
 */
bool writePagesVerify(uint8_t startPage, uint8_t numPages, uint8_t* data);

/**
 * Main data extraction routine
 * Orchestrates all helper functions to build complete ACEProData
 * Handles fallbacks for missing Spoolman fields
 */
bool extractACEProData(const JsonObject& spoolData, ACEProData& aceData);

// ===== Main Tasks =====

/**
 * Entry point for ACE Pro binary write
 * Called from web API with spoolId
 * Fetches data from Spoolman and creates async write task
 */
void startWriteNfcTagBinary(const char* spoolId);

/**
 * Main write task running in FreeRTOS
 * Performs 9-step page write sequence with verification
 * Updates web interface with progress/results
 */
void writeNfcTagBinaryTask(void* param);

#endif