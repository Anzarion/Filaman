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
 * Extract material name with UTF-8 support for binary encoding
 * Supports international characters and umlauts
 * Adapted from EnderPy/AnycubicNFCScript UTF-8 handling
 * 
 * @param spoolData Spoolman JSON filament object
 * @param materialHex Output buffer (minimum 20 bytes for padding)
 * @return true if extraction successful, false on error
 * 
 * Behavior:
 *   - Converts material string to individual byte encoding
 *   - Supports ASCII and UTF-8 characters
 *   - Pads remaining bytes with 0x00 to reach 20 bytes
 *   - Maximum 19 characters + null terminator
 * 
 * Examples:
 *   - "PLA" → [0x50, 0x4C, 0x41, 0x00, ...]
 *   - "PLA+" → [0x50, 0x4C, 0x41, 0x2B, 0x00, ...]
 *   - "PETG" → [0x50, 0x45, 0x54, 0x47, 0x00, ...]
 */
bool getMaterialUTF8(const JsonObject& spoolData, uint8_t* materialHex);

/**
 * Color Validation Result Structure
 * Contains validation status and detailed error information
 * Adapted from EnderPy/AnycubicNFCScript validation patterns
 */
struct ColorValidation {
    bool isValid;
    uint8_t errorCode;  // 0=OK, 1=invalid length, 2=invalid char, 3=parse error
    String errorMsg;
    uint32_t colorABGR; // Valid color in ABGR format (0 if invalid)
};

/**
 * Validate hex color string and convert to ABGR format
 * Supports both 6-digit (RGB) and 8-digit (RGBA) hex codes
 * Adapted from EnderPy/AnycubicNFCScript color handling
 * 
 * Input examples:
 *   - "#FF5533" (RGB, 6 digits)
 *   - "FF5533" (RGB without #)
 *   - "#AA123456" (RGBA with transparency, 8 digits)
 * 
 * Output: Validation struct with ABGR result or error details
 */
ColorValidation validateColorHex(const String& hexColor);

/**
 * Convert RGB hex from Spoolman to ABGR format for ACE Pro
 * Input:  "#FF5533" (RGB hex)
 * Output: 0xFF3357FF (ABGR uint32_t)
 * NOTE: Now uses validateColorHex() internally for robustness
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

/**
 * Separate validation for Brand + Material read from tag
 * Enables fallback matching even when Spoolman ID is missing
 * Used for tags that don't have sm_id (Pages 35-39) written
 * 
 * @param brand Brand string read from tag (Pages 10-14)
 * @param material Material string read from tag (Pages 15-19)
 * @return true if both fields are non-empty and valid ASCII
 */
bool validateACEProBrandMaterial(const String& brand, const String& material);

/**
 * Check if a page is protected/read-only on NTAG2xx tags
 * NTAG213/215/216 have locked pages at the beginning (0x00-0x03)
 * and at the end (configuration pages)
 * 
 * @param page Page number to check
 * @return true if page is protected, false if writable
 */
bool isPageProtected(uint8_t page);

/**
 * Pick the first available temperature range from A, B, or C
 * Used when Spoolman only provides one range profile
 * Falls back to next available if primary is missing
 * 
 * @param rangeA Temperature range A (primary)
 * @param rangeB Temperature range B (fallback 1)
 * @param rangeC Temperature range C (fallback 2)
 * @return First non-empty range, or rangeA if all empty
 */
struct TempRange {
    uint16_t nozzleMin;
    uint16_t nozzleMax;
    bool isValid;
};

TempRange pickAvailableRange(const TempRange& rangeA, const TempRange& rangeB, const TempRange& rangeC);

/**
 * Search for NDEF TLV message start in tag memory
 * Handles both standard (short) and extended-length TLV records
 * Supports NDEF messages starting at various positions
 * 
 * @param buffer Memory buffer from tag (typically 255+ bytes)
 * @param bufferSize Size of buffer
 * @return Offset of NDEF TLV marker (0x03) or -1 if not found
 */
int findNdefTlvStart(const uint8_t* buffer, size_t bufferSize);

#endif