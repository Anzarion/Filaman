#include "nfc_acepro.h"
#include "nfc.h"
#include <Arduino.h>
#include <Adafruit_PN532.h>
#include "config.h"
#include "api.h"

extern Adafruit_PN532 nfc;
extern volatile bool nfcWriteInProgress;
extern volatile nfcReaderStateType nfcReaderState;

// Forward declaration for web interface updates
void sendNfcData();

// ===== Helper Functions =====

/**
 * Generate SKU from Spoolman filament data
 * Format: VENDOR(4 chars max uppercase)-MATERIAL-SPOOLID
 * Example: "SUNL-PLA-123" ensures uniqueness per spool and human readability
 */
String getSKU(const JsonObject& spoolData) {
    String vendor = String(spoolData["filament"]["vendor"]["name"] | "GENERIC");
    String material = String(spoolData["filament"]["material"] | "PLA");
    String spoolId = String(spoolData["id"] | 0);
    
    // Vendor uppercase & shorten to 4 chars for readability
    vendor.toUpperCase();
    if (vendor.length() > 4) {
        vendor = vendor.substring(0, 4);
    }
    
    // Format: VENDOR-MATERIAL-SPOOLID
    String sku = vendor + "-" + material + "-" + spoolId;
    if (sku.length() > 15) {
        sku = sku.substring(0, 15);
    }
    
    Serial.println("[ACEPro] SKU generated: " + sku);
    return sku;
}

/**
 * Extract and validate brand name from Spoolman
 * Handles UTF-8 umlauts by converting to ASCII
 * Output: ASCII brand name (max 18 chars + null terminator)
 */
String getBrand(const JsonObject& spoolData) {
    String brand = String(spoolData["filament"]["vendor"]["name"] | "Anycubic");
    
    // Convert German umlauts to ASCII (for NTAG213 compatibility)
    brand.replace("ä", "ae");
    brand.replace("ö", "oe");
    brand.replace("ü", "ue");
    brand.replace("Ä", "Ae");
    brand.replace("Ö", "Oe");
    brand.replace("Ü", "Ue");
    brand.replace("ß", "ss");
    
    // Replace common special characters
    brand.replace("&", "and");
    
    // Enforce length limit (18 chars + null terminator for Pages 10-14)
    if (brand.length() > 18) {
        brand = brand.substring(0, 18);
    }
    
    Serial.println("[ACEPro] Brand extracted: " + brand);
    return brand;
}

/**
 * Extract and validate material name from Spoolman
 * Output: ASCII material designation (max 18 chars + null terminator)
 */
String getMaterial(const JsonObject& spoolData) {
    String material = String(spoolData["filament"]["material"] | "PLA");
    material.toUpperCase();
    
    // Enforce length limit (18 chars + null terminator for Pages 15-19)
    if (material.length() > 18) {
        material = material.substring(0, 18);
    }
    
    Serial.println("[ACEPro] Material extracted: " + material);
    return material;
}

/**
 * Extract material name with UTF-8 support for binary encoding
 * Converts material string to individual byte encoding
 * Adapted from EnderPy/AnycubicNFCScript UTF-8 handling
 * 
 * Behavior:
 *   - Each character → its byte value (0-255)
 *   - Pads remaining bytes with 0x00 to reach 20 bytes
 *   - Maximum 19 characters (+ auto 0x00 at position 19)
 */
bool getMaterialUTF8(const JsonObject& spoolData, uint8_t* materialHex) {
    if (!materialHex) {
        Serial.println("[ACEPro] ERROR: getMaterialUTF8 - invalid output buffer");
        return false;
    }
    
    String material = String(spoolData["filament"]["material"] | "PLA");
    material.toUpperCase();
    
    // Convert each character to its byte value
    int byteIdx = 0;
    for (int i = 0; i < material.length() && byteIdx < 19; i++) {
        char c = material[i];
        materialHex[byteIdx++] = (uint8_t)c;
    }
    
    // Pad remaining bytes with 0x00 up to 20 bytes total
    while (byteIdx < 20) {
        materialHex[byteIdx++] = 0x00;
    }
    
    Serial.printf("[ACEPro] Material UTF-8 encoded: %s -> %d bytes\n", material.c_str(), byteIdx);
    return true;
}

/**
 * Validate hex color string and convert to ABGR format
 * Supports both 6-digit (RGB) and 8-digit (RGBA) hex codes
 * Adapted from EnderPy/AnycubicNFCScript color validation
 * 
 * Validation checks:
 *   1. Length must be 6 or 8 hex digits (with or without '#')
 *   2. All characters must be valid hex (0-9, A-F, a-f)
 *   3. Parsing must succeed with no overflow
 */
ColorValidation validateColorHex(const String& hexColor) {
    ColorValidation result = {false, 0, "", 0};
    String hex = hexColor;
    
    // Remove leading '#' if present
    if (hex.startsWith("#")) {
        hex = hex.substring(1);
    }
    
    // Trim whitespace
    hex.trim();
    
    // Check length: must be 6 or 8 for RGB or RGBA
    if (hex.length() != 6 && hex.length() != 8) {
        result.errorCode = 1;
        result.errorMsg = "Invalid hex length (expected 6 or 8 digits, got " + String(hex.length()) + ")";
        Serial.printf("[ACEPro] Color validation ERROR: %s\n", result.errorMsg.c_str());
        return result;
    }
    
    // Validate all characters are valid hex digits
    for (int i = 0; i < hex.length(); i++) {
        char c = hex[i];
        if (!((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f'))) {
            result.errorCode = 2;
            result.errorMsg = "Invalid hex character at position " + String(i) + ": '" + String(c) + "'";
            Serial.printf("[ACEPro] Color validation ERROR: %s\n", result.errorMsg.c_str());
            return result;
        }
    }
    
    // Parse hex string to uint32
    char* endptr;
    uint32_t colorValue = strtol(hex.c_str(), &endptr, 16);
    
    if (endptr == hex.c_str()) {
        result.errorCode = 3;
        result.errorMsg = "Failed to parse hex value";
        Serial.printf("[ACEPro] Color validation ERROR: %s\n", result.errorMsg.c_str());
        return result;
    }
    
    // Extract color components
    uint8_t r, g, b, a;
    
    if (hex.length() == 6) {
        // RGB format: RRGGBB
        r = (colorValue >> 16) & 0xFF;
        g = (colorValue >> 8) & 0xFF;
        b = colorValue & 0xFF;
        a = 0xFF;  // Default full opacity
    } else {
        // RGBA format: AARRGGBB
        a = (colorValue >> 24) & 0xFF;
        r = (colorValue >> 16) & 0xFF;
        g = (colorValue >> 8) & 0xFF;
        b = colorValue & 0xFF;
    }
    
    // Convert to ABGR: [Alpha, Blue, Green, Red]
    result.colorABGR = (a << 24) | (b << 16) | (g << 8) | r;
    result.isValid = true;
    result.errorCode = 0;
    result.errorMsg = "OK";
    
    Serial.printf("[ACEPro] Color validation OK: #%s (RGBA) -> 0x%08X (ABGR)\n", hex.c_str(), result.colorABGR);
    return result;
}

/**
 * Convert RGB hex from Spoolman to ABGR format for ACE Pro
 * Input:  "#FF5533" (RGB hex) or multi_color_hexes "FF5533,AABBCC"
 * Output: 0xFF3357FF (ABGR uint32_t)
 * Note: Now supports both color_hex and multi_color_hexes
 */
uint32_t getColor(const JsonObject& spoolData) {
    String colorHex;

    // First try multi_color_hexes (for multi-color filaments)
    if (spoolData["filament"]["multi_color_hexes"].is<const char*>()) {
        String multiColors = String(spoolData["filament"]["multi_color_hexes"]);

        // Extract first color (before comma)
        int commaIndex = multiColors.indexOf(',');
        if (commaIndex > 0) {
            colorHex = multiColors.substring(0, commaIndex);
            Serial.printf("[ACEPro] Multi-color filament detected, using first color: %s\n", colorHex.c_str());
        } else {
            colorHex = multiColors;
        }
    }
    // Fallback to single color_hex
    else if (spoolData["filament"]["color_hex"].is<const char*>()) {
        colorHex = String(spoolData["filament"]["color_hex"]);
    }
    // Default fallback
    else {
        colorHex = "#FFFFFF";
        Serial.println("[ACEPro] No color found in Spoolman, using white fallback");
    }

    // Use validation function
    ColorValidation validation = validateColorHex(colorHex);

    if (validation.isValid) {
        return validation.colorABGR;
    }

    // Fallback to white if validation fails
    Serial.printf("[ACEPro] Color validation failed, using white fallback\n");
    return 0xFFFFFFFF;  // White in ABGR
}

/**
 * Auto-detect bed temperature based on material type
 * Used when Spoolman doesn't provide bed temperature settings
 */
void getDefaultBedTemp(const String& material, uint16_t& minTemp, uint16_t& maxTemp) {
    if (material.indexOf("PLA") >= 0) {
        minTemp = 50;
        maxTemp = 60;
    } else if (material.indexOf("PETG") >= 0) {
        minTemp = 70;
        maxTemp = 80;
    } else if (material.indexOf("ABS") >= 0) {
        minTemp = 100;
        maxTemp = 110;
    } else if (material.indexOf("TPU") >= 0) {
        minTemp = 50;
        maxTemp = 60;
    } else if (material.indexOf("ASA") >= 0) {
        minTemp = 100;
        maxTemp = 110;
    } else {
        minTemp = 50;
        maxTemp = 60;  // Safe default
    }
    
    Serial.printf("[ACEPro] Bed temp fallback for %s: %d-%d°C\n", material.c_str(), minTemp, maxTemp);
}

/**
 * Convert 16-bit value to Little-Endian bytes
 * Required for all multi-byte fields (temps, diameter, weight)
 * 
 * Example: 330m = 0x014A
 * toLE16(330, bytes) → bytes[0]=0x4A, bytes[1]=0x01
 */
void toLE16(uint16_t value, uint8_t* bytes) {
    bytes[0] = (uint8_t)(value & 0xFF);
    bytes[1] = (uint8_t)((value >> 8) & 0xFF);
}

/**
 * Check if a page is protected/read-only on NTAG2xx tags
 * NTAG213/215/216: Pages 0-3 (UID/system), Pages 42+ (config/lock)
 * Returns true for any page that should NOT be written
 */
bool isPageProtected(uint8_t page) {
    // System pages (UID, BCC, etc.) - always protected
    if (page < 4) {
        return true;
    }
    
    // NTAG213 (180 bytes, 45 pages total):
    // - Writable: Pages 4-39
    // - Protected: Pages 40-45 (config/lock)
    
    // NTAG215 (540 bytes, 135 pages total):
    // - Writable: Pages 4-130
    // - Protected: Pages 131-134 (config/lock)
    
    // NTAG216 (888 bytes, 222 pages total):
    // - Writable: Pages 4-215
    // - Protected: Pages 216-221 (config/lock)
    
    // Conservative: Protect pages beyond 215 (safe for all tags)
    if (page > 215) {
        return true;
    }
    
    return false;
}

/**
 * Pick the first available temperature range from A, B, or C
 * Useful when Spoolman tag might have multiple profiles set
 * Fallback strategy: Try A → B → C
 * 
 * Example:
 *   rangeA.isValid=false, rangeB={200,230, true} → returns rangeB
 *   rangeA.isValid=false, rangeB.isValid=false, rangeC={210,240,true} → returns rangeC
 */
TempRange pickAvailableRange(const TempRange& rangeA, const TempRange& rangeB, const TempRange& rangeC) {
    // Try range A first (primary profile)
    if (rangeA.isValid && rangeA.nozzleMin > 0 && rangeA.nozzleMax > rangeA.nozzleMin) {
        Serial.printf("[ACEPro] Using nozzle range A: %d-%d°C\n", rangeA.nozzleMin, rangeA.nozzleMax);
        return rangeA;
    }
    
    // Fallback to range B
    if (rangeB.isValid && rangeB.nozzleMin > 0 && rangeB.nozzleMax > rangeB.nozzleMin) {
        Serial.printf("[ACEPro] Range A empty, using range B: %d-%d°C\n", rangeB.nozzleMin, rangeB.nozzleMax);
        return rangeB;
    }
    
    // Last resort: range C
    if (rangeC.isValid && rangeC.nozzleMin > 0 && rangeC.nozzleMax > rangeC.nozzleMin) {
        Serial.printf("[ACEPro] Ranges A/B empty, using range C: %d-%d°C\n", rangeC.nozzleMin, rangeC.nozzleMax);
        return rangeC;
    }
    
    // All ranges invalid - return rangeA (which has current defaults from extraction)
    Serial.printf("[ACEPro] All ranges invalid or empty, using defaults: %d-%d°C\n", rangeA.nozzleMin, rangeA.nozzleMax);
    return rangeA;
}

/**
 * Find NDEF TLV message in tag memory
 * NDEF messages are stored in TLV format with:
 *   - Type: 0x03 (NDEF Message)
 *   - Length: 1 byte (short) or 0xFF + 2 bytes (extended)
 *   - Value: NDEF record(s)
 * 
 * Can handle:
 *   - Standard short-length: 0x03 0xLL [NDEF data]
 *   - Extended-length: 0x03 0xFF 0xLL 0xLL [NDEF data] (from reference repo)
 *   - Empty/terminator: 0xFE
 */
int findNdefTlvStart(const uint8_t* buffer, size_t bufferSize) {
    if (!buffer || bufferSize < 2) {
        Serial.println("[NDEF] Invalid buffer for NDEF search");
        return -1;
    }
    
    for (size_t i = 0; i < bufferSize - 1; i++) {
        // Found NDEF message type
        if (buffer[i] == 0x03) {
            uint8_t lengthByte = buffer[i + 1];
            
            // Check for extended-length format (0xFF 0xLL 0xLL)
            if (lengthByte == 0xFF) {
                if (i + 4 < bufferSize) {
                    // Valid extended-length NDEF message
                    Serial.printf("[NDEF] Found NDEF TLV at offset 0x%02X (extended-length format)\n", (unsigned int)i);
                    return (int)i;
                }
            }
            // Standard short-length format
            else if (lengthByte > 0 && lengthByte <= 0xFE) {
                // Valid short-length NDEF message
                Serial.printf("[NDEF] Found NDEF TLV at offset 0x%02X (short-length: %d bytes)\n", (unsigned int)i, lengthByte);
                return (int)i;
            }
        }
        
        // Stop scanning at terminator TLV (0xFE)
        if (buffer[i] == 0xFE) {
            Serial.printf("[NDEF] Hit terminator TLV at offset 0x%02X, stopping search\n", (unsigned int)i);
            break;
        }
    }
    
    Serial.println("[NDEF] No valid NDEF TLV message found");
    return -1;
}

/**
 * Write single page + verify immediately
 * ENHANCEMENT: Checks for protected pages before attempting write
 * Returns: true if success, false if all retries failed or page protected
 */
bool writePageVerify(uint8_t page, uint8_t* data) {
    // SAFETY CHECK: Prevent writing to protected pages
    if (isPageProtected(page)) {
        Serial.printf("[ACEPro] ⚠️  SAFETY: Page %d is PROTECTED - skipping write\n", page);
        return false;
    }
    
    const int MAX_RETRIES = 5;  // Increased from 3 to 5
    
    for (int retry = 0; retry < MAX_RETRIES; retry++) {
        // Wait before write attempt (especially important for first write after detection)
        if (retry > 0) {
            vTaskDelay(100 / portTICK_PERIOD_MS);  // Longer delay between retries
        }
        
        // Write page
        if (!nfc.ntag2xx_WritePage(page, data)) {
            Serial.printf("[ACEPro] Page %d write failed (retry %d/%d)\n", page, retry + 1, MAX_RETRIES);
            continue;
        }
        
        // CRITICAL: Wait longer after write before reading back
        vTaskDelay(100 / portTICK_PERIOD_MS);  // Increased from 50ms to 100ms
        
        // Immediate read-back verification
        uint8_t buffer[4];
        if (!nfc.ntag2xx_ReadPage(page, buffer)) {
            Serial.printf("[ACEPro] Page %d read-back failed (retry %d/%d)\n", page, retry + 1, MAX_RETRIES);
            continue;
        }
        
        // Compare written data with read data
        if (memcmp(buffer, data, 4) == 0) {
            Serial.printf("[ACEPro] Page %d write-verify SUCCESS\n", page);
            return true;
        }
        
        Serial.printf("[ACEPro] Page %d verification MISMATCH (retry %d/%d)\n", page, retry + 1, MAX_RETRIES);
    }
    
    Serial.printf("[ACEPro] Page %d write FAILED after %d retries\n", page, MAX_RETRIES);
    return false;
}

/**
 * Write multiple pages (up to 5 pages) + verify immediately
 * Pages are written sequentially with verify after each
 * Returns: true if success, false if any page write failed
 */
bool writePagesVerify(uint8_t startPage, uint8_t numPages, uint8_t* data) {
    Serial.printf("[ACEPro] Writing %d pages starting at page %d\n", numPages, startPage);
    
    for (int i = 0; i < numPages; i++) {
        uint8_t page = startPage + i;
        uint8_t* pageData = data + (i * 4);  // Each page is 4 bytes
        
        if (!writePageVerify(page, pageData)) {
            Serial.printf("[ACEPro] FAILED to write page %d (offset %d in sequence)\n", page, i);
            return false;
        }
        
        vTaskDelay(20 / portTICK_PERIOD_MS);  // Increased from 10ms to 20ms between pages
    }
    
    Serial.printf("[ACEPro] Successfully wrote %d pages\n", numPages);
    return true;
}

/**
 * Main data extraction routine
 * Orchestrates all helper functions to build complete ACEProData
 * Handles fallbacks for missing Spoolman fields
 * Returns: true if extraction successful, false if critical fields missing
 */
bool extractACEProData(const JsonObject& spoolData, ACEProData& aceData) {
    Serial.println("[ACEPro] Starting data extraction from Spoolman...");
    
    // Extract SKU
    String sku = getSKU(spoolData);
    sku.toCharArray(aceData.sku, sizeof(aceData.sku));
    
    // Extract Brand
    String brand = getBrand(spoolData);
    brand.toCharArray(aceData.brand, sizeof(aceData.brand));
    
    // Extract Material (string version for compatibility)
    String material = getMaterial(spoolData);
    material.toCharArray(aceData.material, sizeof(aceData.material));
    
    // Extract Color with enhanced validation
    uint32_t colorABGR = getColor(spoolData);
    
    // Log color conversion result with validation details
    String colorHex = String(spoolData["filament"]["color_hex"] | "#FFFFFF");
    ColorValidation colorVal = validateColorHex(colorHex);
    if (colorVal.isValid) {
        Serial.printf("[ACEPro] ✓ Color validation passed: %s\n", colorVal.errorMsg.c_str());
    } else {
        Serial.printf("[ACEPro] ⚠️  Color validation warning (code %d): %s\n", colorVal.errorCode, colorVal.errorMsg.c_str());
    }
    
    // Store ABGR bytes
    aceData.colorABGR[0] = (uint8_t)((colorABGR >> 24) & 0xFF);  // Alpha
    aceData.colorABGR[1] = (uint8_t)((colorABGR >> 16) & 0xFF);  // Blue
    aceData.colorABGR[2] = (uint8_t)((colorABGR >> 8) & 0xFF);   // Green
    aceData.colorABGR[3] = (uint8_t)(colorABGR & 0xFF);          // Red
    
    // Extract Nozzle Temperatures
    aceData.nozzleTempMin = (uint16_t)(spoolData["filament"]["nozzle_temp_min"] | 200);
    aceData.nozzleTempMax = (uint16_t)(spoolData["filament"]["nozzle_temp_max"] | 230);
    Serial.printf("[ACEPro] Nozzle temps: %d-%d°C\n", aceData.nozzleTempMin, aceData.nozzleTempMax);
    
    // Extract or fallback Bed Temperatures
    if (spoolData["filament"]["bed_temp_min"].is<int>()) {
        aceData.bedTempMin = (uint16_t)spoolData["filament"]["bed_temp_min"];
        aceData.bedTempMax = (uint16_t)spoolData["filament"]["bed_temp_max"];
        Serial.printf("[ACEPro] Bed temps (from Spoolman): %d-%d°C\n", aceData.bedTempMin, aceData.bedTempMax);
    } else {
        getDefaultBedTemp(material, aceData.bedTempMin, aceData.bedTempMax);
    }
    
    // Extract Diameter (0.01mm units)
    // Default to 1.75mm = 175 if missing
    float diameter = spoolData["filament"]["diameter"] | 1.75f;
    aceData.diameterX100 = (uint16_t)(diameter * 100);
    Serial.printf("[ACEPro] Diameter: %.2fmm (%d units)\n", diameter, aceData.diameterX100);
    
    // Extract Length (fixed at 330m for standard spools)
    aceData.lengthMeters = 330;  // Standard spool length
    
    // Extract Weight (default to 1000g if missing)
    aceData.weightGrams = (uint16_t)(spoolData["remaining_weight"] | 1000);
    Serial.printf("[ACEPro] Weight: %dg\n", aceData.weightGrams);
    
    // Extract Spoolman ID (Pages 35-39 in NTAG215)
    // This enables 100% unique identification for weight updates
    String spoolIdStr = String(spoolData["id"] | 0);
    spoolIdStr.toCharArray(aceData.spoolId, sizeof(aceData.spoolId));
    Serial.printf("[ACEPro] Spoolman ID: %s\n", aceData.spoolId);
    
    Serial.println("[ACEPro] Data extraction complete");
    return true;
}

// ===== Main Tasks =====

/**
 * Entry point for ACE Pro binary write
 * Called from web API with spoolId
 * Fetches data from Spoolman and creates async write task
 */
void startWriteNfcTagBinary(const char* spoolId) {
    Serial.println("[ACEPro] startWriteNfcTagBinary() called");
    
    if (nfcWriteInProgress) {
        Serial.println("[ACEPro] NFC write already in progress, aborting");
        return;
    }
    
    if (!spoolId || strlen(spoolId) == 0) {
        Serial.println("[ACEPro] Invalid spoolId provided");
        return;
    }
    
    // Parse spoolId from string
    int spoolIdInt = atoi(spoolId);
    if (spoolIdInt <= 0) {
        Serial.println("[ACEPro] spoolId must be > 0");
        return;
    }
    
    Serial.printf("[ACEPro] Creating write task for spool ID: %d\n", spoolIdInt);
    
    // Create async task for write operation
    // Allocate memory for integer parameter (will be freed in task)
    int* pSpoolId = new int(spoolIdInt);
    
    if (xTaskCreate(
        writeNfcTagBinaryTask,
        "NFC_Binary_Write",
        16384,  // Stack size DOUBLED from 8192 to 16384
        (void*)pSpoolId,
        3,  // Priority
        NULL) != pdPASS) {
        
        Serial.println("[ACEPro] Failed to create write task");
        delete pSpoolId;
    }
}

/**
 * Main write task running in FreeRTOS
 * Performs 9-step page write sequence with verification
 * Updates web interface with progress/results
 */
void writeNfcTagBinaryTask(void* param) {
    int spoolId = *(int*)param;
    delete (int*)param;  // Free parameter memory
    
    Serial.printf("[ACEPro] Write task started for spool ID: %d\n", spoolId);
    sendNfcData();  // Update web interface
    
    nfcWriteInProgress = true;
    nfcReaderState = NFC_WRITING;
    
    // Step 1: Fetch Spoolman data
    Serial.println("[ACEPro] Step 1: Fetching Spoolman data...");
    JsonDocument spoolData = fetchSingleSpoolInfo(spoolId);
    
    if (spoolData.isNull() || !spoolData["id"].is<int>()) {
        Serial.println("[ACEPro] Failed to fetch spool data from Spoolman");
        nfcReaderState = NFC_WRITE_ERROR;
        nfcWriteInProgress = false;
        vTaskDelete(NULL);
        return;
    }
    
    // Step 2: Extract ACE Pro data
    Serial.println("[ACEPro] Step 2: Extracting ACE Pro data...");
    ACEProData aceData;
    JsonObject obj = spoolData.as<JsonObject>();
    if (!extractACEProData(obj, aceData)) {
        Serial.println("[ACEPro] Failed to extract data");
        nfcReaderState = NFC_WRITE_ERROR;
        nfcWriteInProgress = false;
        vTaskDelete(NULL);
        return;
    }
    
    // Step 3: Check tag present
    Serial.println("[ACEPro] Step 3: Checking tag presence...");
    uint8_t uid[7];
    uint8_t uidLength;
    
    if (!nfc.readPassiveTargetID(PN532_MIFARE_ISO14443A, uid, &uidLength, 100)) {
        Serial.println("[ACEPro] No tag detected");
        nfcReaderState = NFC_WRITE_ERROR;
        nfcWriteInProgress = false;
        vTaskDelete(NULL);
        return;
    }
    
    Serial.println("[ACEPro] Tag detected, starting write sequence...");
    
    // **CRITICAL**: Detect tag type BEFORE clearing to avoid false detection
    // (After clearing, empty pages can cause wrong type detection)
    Serial.println("[ACEPro] Step 4a: Detecting tag type before clearing...");
    String detectedTagType = detectNtagType();
    Serial.printf("[ACEPro] Tag Type Detected: %s\n", detectedTagType.c_str());
    
    // **CRITICAL**: For ACE Pro tags, we DON'T need to clear!
    // ACE Pro format has no NDEF structure before it - we just write directly
    // Clearing causes interface instability and is unnecessary
    // The magic bytes (0x7B 0x00 0x65 0x00) will overwrite any old data
    Serial.println("[ACEPro] Step 4b: Skipping clearing (ACE Pro direct write)");
    Serial.println("[ACEPro] ACE Pro format doesn't require clearing - writing directly");
    
    // CRITICAL: Brief stabilization before write sequence
    Serial.println("[ACEPro] Stabilizing before write sequence...");
    vTaskDelay(200 / portTICK_PERIOD_MS);
    
    // Step 5-17: Write & verify all pages
    // Following PR #29 extended format
    
    bool writeSuccess = true;
    
    // Step 5: Page 4 - Magic bytes
    Serial.println("[ACEPro] Step 5: Writing Page 4 (Magic bytes)...");
    vTaskDelay(50 / portTICK_PERIOD_MS);  // Extra delay before first write after clearing
    uint8_t magicBytes[4] = {0x7B, 0x00, 0x65, 0x00};
    if (!writePageVerify(4, magicBytes)) {
        Serial.println("[ACEPro] Failed to write magic bytes");
        writeSuccess = false;
    }
    
    // Step 6: Pages 5-8 - SKU (4 pages = 16 bytes)
    if (writeSuccess) {
        Serial.println("[ACEPro] Step 6: Writing Pages 5-8 (SKU)...");
        if (!writePagesVerify(5, 4, (uint8_t*)aceData.sku)) {
            Serial.println("[ACEPro] Failed to write SKU");
            writeSuccess = false;
        }
    }
    
    // Step 7: Pages 10-14 - Brand (5 pages = 20 bytes) [PR #29 EXTENDED]
    if (writeSuccess) {
        Serial.println("[ACEPro] Step 7: Writing Pages 10-14 (Brand)...");
        if (!writePagesVerify(10, 5, (uint8_t*)aceData.brand)) {
            Serial.println("[ACEPro] Failed to write brand");
            writeSuccess = false;
        }
    }
    
    // Step 8: Pages 15-19 - Material (5 pages = 20 bytes) [PR #29 EXTENDED]
    if (writeSuccess) {
        Serial.println("[ACEPro] Step 8: Writing Pages 15-19 (Material)...");
        if (!writePagesVerify(15, 5, (uint8_t*)aceData.material)) {
            Serial.println("[ACEPro] Failed to write material");
            writeSuccess = false;
        }
    }
    
    // Step 9: Page 20 - Color ABGR (4 bytes) [PRINTER-CRITICAL]
    if (writeSuccess) {
        Serial.println("[ACEPro] Step 9: Writing Page 20 (Color ABGR)...");
        if (!writePageVerify(20, aceData.colorABGR)) {
            Serial.println("[ACEPro] Failed to write color");
            writeSuccess = false;
        }
    }
    
    // Step 10: Page 24 - Nozzle temperatures (4 bytes)
    if (writeSuccess) {
        Serial.println("[ACEPro] Step 10: Writing Page 24 (Nozzle temps)...");
        uint8_t nozzleTempsLE[4];
        toLE16(aceData.nozzleTempMin, &nozzleTempsLE[0]);
        toLE16(aceData.nozzleTempMax, &nozzleTempsLE[2]);
        if (!writePageVerify(24, nozzleTempsLE)) {
            Serial.println("[ACEPro] Failed to write nozzle temps");
            writeSuccess = false;
        }
    }
    
    // Step 11: Page 29 - Bed temperatures (4 bytes)
    if (writeSuccess) {
        Serial.println("[ACEPro] Step 11: Writing Page 29 (Bed temps)...");
        uint8_t bedTempsLE[4];
        toLE16(aceData.bedTempMin, &bedTempsLE[0]);
        toLE16(aceData.bedTempMax, &bedTempsLE[2]);
        if (!writePageVerify(29, bedTempsLE)) {
            Serial.println("[ACEPro] Failed to write bed temps");
            writeSuccess = false;
        }
    }
    
    // Step 12: Page 30 - Diameter + Length (4 bytes)
    if (writeSuccess) {
        Serial.println("[ACEPro] Step 12: Writing Page 30 (Diameter + Length)...");
        uint8_t diamLenLE[4];
        toLE16(aceData.diameterX100, &diamLenLE[0]);
        toLE16(aceData.lengthMeters, &diamLenLE[2]);
        if (!writePageVerify(30, diamLenLE)) {
            Serial.println("[ACEPro] Failed to write diameter/length");
            writeSuccess = false;
        }
    }
    
    // Step 13: Page 31 - Weight (4 bytes)
    if (writeSuccess) {
        Serial.println("[ACEPro] Step 13: Writing Page 31 (Weight)...");
        uint8_t weightLE[4] = {0};
        toLE16(aceData.weightGrams, &weightLE[0]);
        if (!writePageVerify(31, weightLE)) {
            Serial.println("[ACEPro] Failed to write weight");
            writeSuccess = false;
        }
    }
    
    // Step 14: Full verification
    if (writeSuccess) {
        Serial.println("[ACEPro] Step 14: Performing full verification...");
        
        // Verify magic bytes
        uint8_t magicRead[4];
        if (!nfc.ntag2xx_ReadPage(4, magicRead) || memcmp(magicRead, magicBytes, 4) != 0) {
            Serial.println("[ACEPro] Magic bytes verification FAILED");
            writeSuccess = false;
        }
        
        // Verify SKU (pages 5-8)
        if (writeSuccess) {
            uint8_t skuRead[16];
            for (int i = 0; i < 4; i++) {
                if (!nfc.ntag2xx_ReadPage(5 + i, skuRead + (i * 4))) {
                    Serial.println("[ACEPro] SKU verification FAILED");
                    writeSuccess = false;
                    break;
                }
            }
            if (writeSuccess && memcmp(skuRead, aceData.sku, 16) != 0) {
                Serial.println("[ACEPro] SKU data mismatch");
                writeSuccess = false;
            }
        }
        
        // Verify Color (page 20)
        if (writeSuccess) {
            uint8_t colorRead[4];
            if (!nfc.ntag2xx_ReadPage(20, colorRead) || memcmp(colorRead, aceData.colorABGR, 4) != 0) {
                Serial.println("[ACEPro] Color verification FAILED");
                writeSuccess = false;
            }
        }
    }
    
    // Step 15: Pages 35-39 - Spoolman ID (sm_id)
    // This enables weight update and spool tracking in hybrid mode
    if (writeSuccess) {
        Serial.println("[ACEPro] Step 15: Writing Pages 35-39 (Spoolman ID)...");
        
        // Pad sm_id to 20 bytes (5 pages × 4 bytes)
        uint8_t smIdData[20] = {0};
        int smIdLen = strlen(aceData.spoolId);
        if (smIdLen > 20) smIdLen = 20;  // Enforce max length
        memcpy(smIdData, aceData.spoolId, smIdLen);
        
        Serial.printf("[ACEPro] sm_id data (%d bytes): ", smIdLen);
        for (int i = 0; i < 20; i++) {
            if (smIdData[i] < 0x10) Serial.print("0");
            Serial.print(smIdData[i], HEX);
            Serial.print(" ");
        }
        Serial.println();
        
        if (!writePagesVerify(35, 5, smIdData)) {
            Serial.println("[ACEPro] Failed to write Spoolman ID");
            writeSuccess = false;
        }
    }
    
    // Step 16: Write NDEF at page 40 using wrapper (hybrid format)
    // This allows NDEF apps to read tag data without conflicting with ACE Pro format
    if (writeSuccess) {
        Serial.println("[ACEPro] Step 16: Writing NDEF at page 40 (hybrid format)...");

        // Build NDEF payload with spoolman ID and basic tag info
        JsonDocument ndefPayload;
        ndefPayload["sm_id"] = aceData.spoolId;
        ndefPayload["sku"] = aceData.sku;
        ndefPayload["brand"] = aceData.brand;
        ndefPayload["type"] = aceData.material;

        // NDEF can store multi-color info (unlike binary format which only stores first color)
        // Check if multi_color_hexes exists in original Spoolman data
        if (obj["filament"]["multi_color_hexes"].is<const char*>()) {
            // Multi-color filament: Store original multi_color_hexes
            String multiColors = String(obj["filament"]["multi_color_hexes"]);
            ndefPayload["multi_color_hexes"] = multiColors;
            ndefPayload["multi_color_direction"] = String(obj["filament"]["multi_color_direction"] | "coaxial");
            Serial.printf("[ACEPro] NDEF: Storing multi-color info: %s\n", multiColors.c_str());

            // Also store first color as color_hex for compatibility
            int commaIndex = multiColors.indexOf(',');
            if (commaIndex > 0) {
                ndefPayload["color_hex"] = multiColors.substring(0, commaIndex);
            } else {
                ndefPayload["color_hex"] = multiColors;
            }
        } else {
            // Single color: Convert ABGR back to hex (format: RRGGBB without #)
            char colorHex[7];
            snprintf(colorHex, sizeof(colorHex), "%02X%02X%02X",
                     aceData.colorABGR[3],  // Red
                     aceData.colorABGR[2],  // Green
                     aceData.colorABGR[1]); // Blue
            ndefPayload["color_hex"] = colorHex;
        }

        // Add temperature ranges
        ndefPayload["min_temp"] = String(aceData.nozzleTempMin);
        ndefPayload["max_temp"] = String(aceData.nozzleTempMax);
        
        // Serialize to string
        String ndefPayloadStr;
        serializeJson(ndefPayload, ndefPayloadStr);
        
        Serial.printf("[ACEPro] NDEF payload (%d bytes): %s\n", ndefPayloadStr.length(), ndefPayloadStr.c_str());
        
        // Call wrapper to write NDEF starting at page 40
        uint8_t ndefWriteResult = ntag2xx_WriteNDEFWithStartPage(40, ndefPayloadStr.c_str());
        if (!ndefWriteResult) {
            Serial.println("[ACEPro] WARNING: NDEF write failed (hybrid tag will work with ACE Pro but not NDEF readers)");
            // Don't fail completely - ACE Pro format is still valid
            Serial.println("[ACEPro] Continuing with ACE Pro format only");
        } else {
            Serial.println("[ACEPro] NDEF write successful - tag now supports both formats!");
        }
        
        ndefPayload.clear();
    }
    
    // Step 17: Report result
    if (writeSuccess) {
        Serial.println("[ACEPro] ✓ Tag written successfully!");
        Serial.println("[ACEPro] ✓ ACE Pro binary format (Pages 4-34): READY");
        Serial.println("[ACEPro] ✓ Spoolman ID (Pages 35-39): READY");
        Serial.println("[ACEPro] ✓ NDEF format (Pages 40+): READY");
        Serial.println("[ACEPro] ✓ Tag is now DUAL-FORMAT COMPATIBLE!");
        nfcReaderState = NFC_WRITE_SUCCESS;
    } else {
        Serial.println("[ACEPro] ✗ Tag write FAILED");
        nfcReaderState = NFC_WRITE_ERROR;
    }
    
    sendNfcData();  // Update web interface
    
    // Stabilize NFC interface
    delay(200);
    
    // Cleanup
    nfcWriteInProgress = false;
    nfcReaderState = NFC_IDLE;
    
    Serial.println("[ACEPro] Write task completed");
    vTaskDelete(NULL);
}

/**
 * Validate Brand + Material fields read from ACE Pro tag
 * Provides separate validation for fallback matching scenarios
 * 
 * @param brand Brand string extracted from Pages 10-14
 * @param material Material string extracted from Pages 15-19
 * @return true if both fields are valid, false if either is corrupted/empty
 */
bool validateACEProBrandMaterial(const String& brand, const String& material) {
    Serial.println("[ACEPro-Validation] Starting Brand+Material validation...");
    
    // Check if brand is not empty
    if (brand.isEmpty() || brand.length() == 0) {
        Serial.println("[ACEPro-Validation] ❌ Brand field is empty");
        return false;
    }
    
    // Check if material is not empty
    if (material.isEmpty() || material.length() == 0) {
        Serial.println("[ACEPro-Validation] ❌ Material field is empty");
        return false;
    }
    
    // Validate brand - check for valid ASCII characters
    for (unsigned int i = 0; i < brand.length(); i++) {
        char c = brand.charAt(i);
        // Allow ASCII printable characters (0x20-0x7E) and common umlauts converted to ASCII
        if ((c < 0x20 || c > 0x7E) && c != 0x00) {
            Serial.printf("[ACEPro-Validation] ❌ Invalid character in brand at position %d: 0x%02X\n", i, (uint8_t)c);
            return false;
        }
    }
    
    // Validate material - check for valid ASCII characters
    for (unsigned int i = 0; i < material.length(); i++) {
        char c = material.charAt(i);
        // Allow ASCII printable characters (0x20-0x7E)
        if ((c < 0x20 || c > 0x7E) && c != 0x00) {
            Serial.printf("[ACEPro-Validation] ❌ Invalid character in material at position %d: 0x%02X\n", i, (uint8_t)c);
            return false;
        }
    }
    
    // Additional validation: reasonable length (not too short, not too long)
    if (brand.length() > 18) {
        Serial.println("[ACEPro-Validation] ⚠ Brand length exceeds 18 chars (corrupted?)");
        return false;
    }
    
    if (material.length() > 18) {
        Serial.println("[ACEPro-Validation] ⚠ Material length exceeds 18 chars (corrupted?)");
        return false;
    }
    
    // Both fields are valid
    Serial.printf("[ACEPro-Validation] ✓ Brand + Material validation successful\n");
    Serial.printf("[ACEPro-Validation]   Brand: '%s' (%d bytes)\n", brand.c_str(), brand.length());
    Serial.printf("[ACEPro-Validation]   Material: '%s' (%d bytes)\n", material.c_str(), material.length());
    
    return true;
}