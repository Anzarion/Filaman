# ACE Pro NFC Tag Implementation Plan

**Version:** 3.0 (Updated with mrRobot62 + ACE-RFID PR #29)  
**Status:** ✅ READY FOR CODE IMPLEMENTATION  
**Last Updated:** Post-Research + PR #29 Integration  
**Architecture:** Coexistence Pattern (FilaMan JSON + ACE Pro Binary)

---

## CRITICAL INSIGHTS FROM mrRobot62 + PR #29

### Finding 1: Printer Usage
The Anycubic ACE Pro printer **only reads SKU and Color** from the NFC tag. All other data (temperatures, material, weight) is stored for user reference but NOT used by the printer.

### Finding 2: Extended Format (PR #29)
ACE-RFID PR #29 revealed **extended page allocations:**
- Brand: **4 bytes → 20 bytes** (Pages 10-14) - Supports full vendor names
- Material: **4 bytes → 20 bytes** (Pages 15-19) - Supports complex material designations
- **Confirmed working with Anycubic Slicer Next**

### Implementation Strategy:
1. **Use PR #29 extended format** - Future-proof, supports any brand/material
2. Focus: Ensure SKU and Color encoding are 100% correct
3. Simplification: Other fields can use safe defaults
4. Compatibility: Confirmed format against real tag dumps + Java implementation

---

## ✅ QUICK START IMPLEMENTATION CHECKLIST

### Phase 1: Create New Files
- [ ] Create `src/nfc_acepro.h` with ACEProData struct + helper function declarations
- [ ] Create `src/nfc_acepro.cpp` with all helper functions + main write task
- [ ] Update `src/nfc.h` to export `startWriteNfcTagBinary()` function

### Phase 2: Implement Helper Functions (in nfc_acepro.cpp)
- [ ] `getSKU()` - Generate from Spoolman (vendor-material-id format)
- [ ] `getBrand()` - Extract vendor name with umlaut conversion
- [ ] `getMaterial()` - Extract material type
- [ ] `getColor()` - Convert RGB hex to ABGR uint32
- [ ] `extractACEProData()` - Main extraction routine
- [ ] `getDefaultBedTemp()` - Material-based fallback temps
- [ ] `toLE16()` - Little-endian conversion helper
- [ ] `writePageVerify()` - Write + verify single page
- [ ] `writePagesVerify()` - Write + verify multiple pages

### Phase 3: Implement Main Tasks (in nfc_acepro.cpp)
- [ ] `startWriteNfcTagBinary()` - Entry point (API call → fetch data → create task)
- [ ] `writeNfcTagBinaryTask()` - Main write sequence (9-step page writes + verification)

### Phase 4: Integration
- [ ] Add `startWriteNfcTagBinary()` export to `src/nfc.h`
- [ ] Test data extraction with real Spoolman JSON
- [ ] Test page writes with real NTAG213 tags
- [ ] Verify backward compatibility with old 4-byte format tags

---

## FILE STRUCTURE

```
src/
├── nfc.h                    [MODIFIED: +1 export]
├── nfc.cpp                  [UNCHANGED]
├── nfc_acepro.h             [NEW]
└── nfc_acepro.cpp           [NEW]
```

### Changes Required

#### File: nfc.h
**Action:** Add ONE function export (around line 32-35)
```cpp
// Existing exports stay the same
// Add this:
void startWriteNfcTagBinary(const char* spoolId);
```

---

## DEPENDENCY MAP

### External Dependencies (Already Available)
- ✅ `Adafruit_PN532` - NFC hardware interface
- ✅ `FreeRTOS` - Task scheduling
- ✅ `Arduino.h` - Core functions
- ✅ `ArduinoJson.h` - JSON parsing
- ✅ `config.h` - Debug macros

### Internal Dependencies (Reuse Existing)
- ✅ `api.cpp:fetchSingleSpoolInfo()` - Get filament data from Spoolman
- ✅ `api.h` - API declarations
- ✅ `nfc.cpp:extern Adafruit_PN532` - NFC hardware instance
- ✅ `nfc.cpp:nfcWriteInProgress` - State flag
- ✅ `nfc.cpp:nfcReaderState` - Reader state
- ✅ `website.h:updateWebInterface()` - UI notifications

### What We Do NOT Need to Change
- 🔒 `nfc.cpp` - Keep FilaMan JSON write logic untouched
- 🔒 `nfc.h` - Only add 1 export, don't modify existing code
- 🔒 `api.cpp` - Just call existing functions
- 🔒 `main.cpp` - No changes required
- 🔒 All other files - Untouched

---

## DATA STRUCTURES

### Main Data Container
```cpp
// In nfc_acepro.h

struct ACEProData {
    // Pages 5-8: SKU (16 bytes, 4 pages total)
    // Format: "VENDOR-MATERIAL-CODE" (max 15 chars + null terminator)
    // Example: "SUNL-PLA-123" or "BAMB-PETG-456" or "AHPLPDB-106"
    // NOTE: Printer uses this to identify filament profile
    String sku;
    
    // Pages 10-14: Brand/Manufacturer (20 bytes, 5 pages total)
    // Format: Full vendor name (max 18 chars + null terminator)
    // Examples: "Anycubic", "eSUN", "Bambu Lab", "Prusament", "SUNLU"
    // PR #29 EXTENDED: 4 bytes → 20 bytes (supports full brand names)
    // NOTE: Printer ignores this; for user reference
    String brand;
    
    // Pages 15-19: Material type (20 bytes, 5 pages total)
    // Format: Material designation (max 18 chars + null terminator)
    // Examples: "PLA", "PLA+", "PETG", "ASA Carbon", "ABS-GF", "TPU"
    // PR #29 EXTENDED: 4 bytes → 20 bytes (supports complex material names)
    // NOTE: Printer ignores this; for user reference
    String material;
    
    // Page 20: Color in ABGR format (4 bytes: Alpha, Blue, Green, Red)
    // Format: [0xFF, Blue, Green, Red]
    // Example: "#FF5533" (RGB) → [0xFF, 0x33, 0x55, 0xFF] (ABGR)
    // NOTE: Printer reads this value and displays on UI
    uint8_t colorABGR[4];
    
    // Page 24: Nozzle Temperature (4 bytes, 2×uint16 Little-Endian)
    // Format: Min temp (bytes 0-1), Max temp (bytes 2-3) in °C
    // Example: 200°C min, 230°C max
    // NOTE: Printer ignores; for user reference
    uint16_t nozzleTempMin;  // °C
    uint16_t nozzleTempMax;  // °C
    
    // Page 29: Bed Temperature (4 bytes, 2×uint16 Little-Endian)
    // Format: Min temp (bytes 0-1), Max temp (bytes 2-3) in °C
    // Fallback if not in Spoolman: Material-dependent (see getDefaultBedTemp)
    // NOTE: Printer ignores; for user reference
    uint16_t bedTempMin;     // °C
    uint16_t bedTempMax;     // °C
    
    // Page 30 (bytes 0-1): Filament Diameter (uint16 Little-Endian)
    // Unit: 0.01mm (so 175 = 1.75mm)
    // Page 30 (bytes 2-3): Spool Length (uint16 Little-Endian)
    // Unit: Meters (always 330 for standard spool)
    // NOTE: Printer ignores; for user reference
    uint16_t diameterX100;   // Diameter * 100 (e.g., 175 for 1.75mm)
    uint16_t lengthMeters;   // Spool length (always 330m)
    
    // Page 31: Weight (4 bytes, uint16 Little-Endian)
    // Unit: grams (remaining filament weight)
    // NOTE: Printer ignores; for user reference
    uint16_t weightGrams;    // Remaining weight in grams
};
```

---

## FUNCTION ARCHITECTURE

### Entry Point: startWriteNfcTagBinary()

**Signature:**
```cpp
void startWriteNfcTagBinary(const char* spoolId);
```

**Behavior:**
1. Parse spoolId (integer)
2. Call `fetchSingleSpoolInfo(spoolId)` to get filament data
3. Validate: Check if Spoolman API returned valid data
4. If failed: Send error notification to web interface
5. If success: Create ACEProData structure
6. Create FreeRTOS task: `writeNfcTagBinaryTask()`
7. Return immediately (task runs in background)

**Error Handling:**
```
✗ spoolId invalid → Return early + notify
✗ Spoolman unreachable → Return early + notify
✗ No filament data → Return early + notify
✗ Missing required fields → Use fallbacks, continue
```

---

### Main Task: writeNfcTagBinaryTask()

**Signature:**
```cpp
void writeNfcTagBinaryTask(void* param);
```

**Workflow:**
```
1. Set nfcWriteInProgress = true
2. Notify web UI: "NFC write in progress..."
3. Fetch Spoolman data via fetchSingleSpoolInfo()
4. Extract ACE Pro data via extractACEProData()
5. Check tag present (read page 0)
6. Write & verify sequence (PR #29 format):
   - Page 4:      Magic bytes (4 bytes) → writePageVerify()
   - Pages 5-8:   SKU (16 bytes, 4 pages) → writePagesVerify()
   - Pages 10-14: Brand (20 bytes, 5 pages) → writePagesVerify() [PR #29 EXTENDED]
   - Pages 15-19: Material (20 bytes, 5 pages) → writePagesVerify() [PR #29 EXTENDED]
   - Page 20:     Color ABGR (4 bytes) → writePageVerify() [PRINTER-CRITICAL]
   - Page 24:     Nozzle temps (4 bytes) → writePageVerify()
   - Page 29:     Bed temps (4 bytes) → writePageVerify()
   - Page 30:     Diameter + Length (4 bytes) → writePageVerify()
   - Page 31:     Weight (4 bytes) → writePageVerify()
7. Full verification (read all critical pages):
   - Read page 4 (magic) vs expected [0x7B, 0x00, 0x65, 0x00]
   - Read pages 5-8 (SKU) vs expected data
   - Read page 20 (color) vs expected ABGR
   - Compare byte-by-byte
8. Report result:
   - Success: "Tag written successfully"
   - Failure: "Write failed on page X" + abort
9. Interface update:
   - Stabilize NFC interface (same as JSON write)
   - Send success/error notification to web UI
10. Set nfcWriteInProgress = false
11. Task cleanup
```

**Error Recovery:**
```
If page write fails:
  → Retry up to 3 times
  → If all retries fail:
     → Abort entire operation
     → Report error to user
     → Set interface to error state
```

---

### Helper Functions

#### 1. SKU Generation: getSKU()
```cpp
// Generate SKU from Spoolman filament data
// Format: VENDOR(4 chars max)-MATERIAL-SPOOLID
// Example: "SUNL-PLA-123" or "BAMB-PETG-456"
// Output: Unique SKU per spool (max 15 chars, including null terminator)

String getSKU(const JsonObject& spoolData) {
    String vendor = String(spoolData["filament"]["vendor"]["name"] | "GENERIC");
    String material = String(spoolData["filament"]["material"] | "PLA");
    String spoolId = String(spoolData["id"] | 0);
    
    // Vendor uppercase & shorten to 4 chars for readability
    vendor.toUpperCase();
    if (vendor.length() > 4) vendor = vendor.substring(0, 4);
    
    // Format: VENDOR-MATERIAL-SPOOLID
    String sku = vendor + "-" + material + "-" + spoolId;
    if (sku.length() > 15) sku = sku.substring(0, 15);
    
    return sku;
}
```

#### 2. Brand Name: getBrand()
```cpp
// Extract and validate brand name from Spoolman
// Handles UTF-8 umlauts by converting to ASCII
// Output: ASCII brand name (max 18 chars + null terminator)

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
    if (brand.length() > 18) brand = brand.substring(0, 18);
    
    return brand;
}
```

#### 3. Material Name: getMaterial()
```cpp
// Extract and validate material name from Spoolman
// Output: ASCII material designation (max 18 chars)

String getMaterial(const JsonObject& spoolData) {
    String material = String(spoolData["filament"]["material"] | "PLA");
    material.toUpperCase();
    
    // Enforce length limit (18 chars + null terminator for Pages 15-19)
    if (material.length() > 18) material = material.substring(0, 18);
    
    return material;
}
```

#### 4. Color Conversion: getColor()
```cpp
// Convert RGB hex from Spoolman to ABGR format for ACE Pro
// Input:  "#FF5533" (RGB hex)
// Output: 0xFF3357FF (ABGR uint32_t)
// Note: Alpha channel always 0xFF (fully opaque)

uint32_t getColor(const JsonObject& spoolData) {
    String colorHex = String(spoolData["filament"]["color_hex"] | "#FFFFFF");
    
    // Parse "#RRGGBB" format
    uint32_t rgb = strtol(colorHex.substring(1).c_str(), NULL, 16);
    uint8_t r = (rgb >> 16) & 0xFF;
    uint8_t g = (rgb >> 8) & 0xFF;
    uint8_t b = rgb & 0xFF;
    
    // Return as ABGR: [Alpha=0xFF, Blue, Green, Red]
    return 0xFF000000 | (b << 16) | (g << 8) | r;
}
```

#### 5. Fallback Temperature Generator: getDefaultBedTemp()
```cpp
// Auto-detect bed temperature based on material type
// Used when Spoolman doesn't provide bed temperature settings

void getDefaultBedTemp(const String& material, 
                       uint16_t& minTemp, uint16_t& maxTemp) {
    if (material.indexOf("PLA") >= 0) {
        minTemp = 50; maxTemp = 60;
    } else if (material.indexOf("PETG") >= 0) {
        minTemp = 70; maxTemp = 80;
    } else if (material.indexOf("ABS") >= 0) {
        minTemp = 100; maxTemp = 110;
    } else if (material.indexOf("TPU") >= 0) {
        minTemp = 50; maxTemp = 60;
    } else {
        minTemp = 50; maxTemp = 60;  // Safe default
    }
}
```

#### 6. Little-Endian Conversion: toLE16()
```cpp
// Convert 16-bit value to Little-Endian bytes
// Required for all multi-byte fields (temps, diameter, weight)

void toLE16(uint16_t value, uint8_t* bytes) {
    bytes[0] = (uint8_t)(value & 0xFF);
    bytes[1] = (uint8_t)((value >> 8) & 0xFF);
}

// Example: 330m = 0x014A
// toLE16(330, bytes) → bytes[0]=0x4A, bytes[1]=0x01
```

#### 7. Page Write with Verification: writePageVerify()
```cpp
// Write single page + verify immediately
// Returns: true if success, false if all retries failed

bool writePageVerify(uint8_t page, uint8_t* data) {
    const int MAX_RETRIES = 3;
    
    for (int retry = 0; retry < MAX_RETRIES; retry++) {
        // Write page
        if (!nfc.write(page, data)) {
            continue;  // Retry on write failure
        }
        
        // Immediate read-back verification
        uint8_t buffer[4];
        if (nfc.read(page, buffer) && memcmp(buffer, data, 4) == 0) {
            return true;  // Success
        }
    }
    
    return false;  // All retries failed
}
```

#### 8. Multi-Page Write: writePagesVerify()
```cpp
// Write multiple consecutive pages with verification
// Calls writePageVerify() for each page
// PR #29: Used for 5-page strings (Brand Pages 10-14, Material Pages 15-19)

bool writePagesVerify(uint8_t startPage, uint8_t endPage, 
                      const uint8_t* data) {
    for (uint8_t page = startPage; page <= endPage; page++) {
        const uint8_t* pageData = data + (page - startPage) * 4;
        if (!writePageVerify(page, (uint8_t*)pageData)) {
            return false;  // Abort on first failure
        }
    }
    return true;
}

// Example usage for Brand (Pages 10-14, 20 bytes):
// String brand = "Anycubic";
// uint8_t brandBytes[20] = {0};
// strncpy((char*)brandBytes, brand.c_str(), 18);
// writePagesVerify(10, 14, brandBytes);  // 5 pages × 4 bytes
```

#### 9. Complete Data Extraction: extractACEProData()
```cpp
// Extract and validate all filament data from Spoolman JSON
// This is called once per write operation
// Returns: Fully populated ACEProData structure ready for writing

ACEProData extractACEProData(const JsonObject& spoolData) {
    ACEProData data;
    
    // === STRING FIELDS (from Spoolman) ===
    data.sku = getSKU(spoolData);
    data.brand = getBrand(spoolData);
    data.material = getMaterial(spoolData);
    
    // === COLOR FIELD (from Spoolman) ===
    uint32_t colorValue = getColor(spoolData);
    data.colorABGR[0] = (colorValue >> 24) & 0xFF;  // Alpha
    data.colorABGR[1] = (colorValue >> 16) & 0xFF;  // Blue
    data.colorABGR[2] = (colorValue >> 8) & 0xFF;   // Green
    data.colorABGR[3] = colorValue & 0xFF;          // Red
    
    // === NOZZLE TEMPERATURE (from Spoolman or defaults) ===
    JsonObject settings = spoolData["filament"]["settings_extruder"];
    if (!settings.isNull() && settings.containsKey("temperature")) {
        data.nozzleTempMin = settings["temperature"] | 200;
        data.nozzleTempMax = data.nozzleTempMin + 30;  // +30°C range
    } else {
        data.nozzleTempMin = 200;
        data.nozzleTempMax = 230;
    }
    
    // === BED TEMPERATURE (from Spoolman or fallback) ===
    if (!settings.isNull() && settings.containsKey("bed_temperature")) {
        data.bedTempMin = settings["bed_temperature"] | 50;
        data.bedTempMax = data.bedTempMin + 10;
    } else {
        getDefaultBedTemp(data.material, data.bedTempMin, data.bedTempMax);
    }
    
    // === FILAMENT DIAMETER ===
    data.diameterX100 = (uint16_t)(spoolData["filament"]["diameter"] | 1.75) * 100;
    
    // === SPOOL LENGTH ===
    data.lengthMeters = 330;  // Standard spool length (fixed)
    
    // === REMAINING WEIGHT ===
    data.weightGrams = spoolData["remaining_weight"] | 1000;
    
    return data;
}
```

---

## SPOOLMAN API FIELD MAPPING

### JSON Structure Expected from Spoolman
```json
{
  "id": 123,
  "filament": {
    "vendor": {
      "name": "Bambu Lab"  // or null
    },
    "material": "PLA",     // or null
    "color_hex": "#FF5733", // or null
    "diameter": 1.75,      // or null
    "settings_extruder": {
      "temperature": 200,  // optional
      "bed_temperature": 60 // optional
    }
  },
  "remaining_weight": 950
}
```

### Field Mapping & Fallbacks

| ACE Pro Field | Spoolman Source | Type | Fallback | Validation |
|---|---|---|---|---|
| **SKU** | id + vendor.name + material | String | "GENERIC-UNKNOWN-0" | Max 15 chars |
| **Brand** | filament.vendor.name | String | "Anycubic" | ASCII, max 18 chars |
| **Material** | filament.material | String | "PLA" | ASCII, max 18 chars |
| **Color** | filament.color_hex | String | "#FFFFFF" | Must be #RRGGBB format |
| **Nozzle Min** | settings_extruder.temperature | uint16 | 200°C | Valid range: 150-300°C |
| **Nozzle Max** | settings_extruder.temperature + 30 | uint16 | Min + 30°C | Valid range: 150-300°C |
| **Bed Min** | settings_extruder.bed_temperature | uint16 | Material-dependent | Valid range: 20-130°C |
| **Bed Max** | settings_extruder.bed_temperature + 10 | uint16 | Fallback + 10°C | Valid range: 20-130°C |
| **Diameter** | filament.diameter * 100 | uint16 | 175 (1.75mm) | Valid range: 100-400 (1.0-4.0mm) |
| **Length** | Fixed | uint16 | 330 | Always 330m (standard spool) |
| **Weight** | remaining_weight | uint16 | 1000g | Valid range: 0-5000g |

### Material-Based Fallback Temperatures

Used when `settings_extruder.bed_temperature` is NOT available:

| Material | Bed Min | Bed Max |
|----------|---------|---------|
| PLA, TPU | 50°C | 60°C |
| PETG | 70°C | 80°C |
| ABS | 100°C | 110°C |
| Others | 50°C | 60°C |

---

## PAGE WRITE SEQUENCE (DETAILED) — PR #29 EXTENDED FORMAT

Each write is independent with verification:

| Step | Page(s) | Content | Bytes | Format | Notes |
|------|---------|---------|-------|--------|---|
| 1 | 4 | Magic bytes | 4 | `7B 00 65 00` | Fixed; printer uses for tag recognition |
| 2 | 5-8 | SKU (15 chars max) | 16 | ASCII + null + padding | Pages 5-8 (4 pages) |
| **3** | **10-14** | **Brand (18 chars max)** | **20** | **ASCII + null + padding (PR #29)** | **Pages 10-14 (5 pages); printer ignores** |
| **4** | **15-19** | **Material (18 chars max)** | **20** | **ASCII + null + padding (PR #29)** | **Pages 15-19 (5 pages); printer ignores** |
| 5 | 20 | Color (ABGR) | 4 | `[0xFF, Blue, Green, Red]` | **Printer reads this for UI display** |
| 6 | 24 | Nozzle Temps | 4 | `[Min_LE(2B), Max_LE(2B)]` | Bytes 0-3: Min=0-1, Max=2-3 (Little-Endian) |
| 7 | 29 | Bed Temps | 4 | `[Min_LE(2B), Max_LE(2B)]` | Bytes 0-3: Min=0-1, Max=2-3 (Little-Endian) |
| 8 | 30 | Diameter + Length | 4 | `[Diam_LE(2B), Len_LE(2B)]` | Bytes 0-1: Diameter×100, Bytes 2-3: Length (m) |
| 9 | 31 | Weight | 4 | `[Weight_LE(2B), 0x00, 0x00]` | Bytes 0-1: Weight (grams), Bytes 2-3: Reserved |

**All pages must write successfully before reporting success.**

**PR #29 Changes:** Steps 3 & 4 now use 5 pages each (instead of 4) for Brand and Material fields.

---

## ERROR HANDLING STRATEGY

### Level 1: Spoolman API Errors
```cpp
if (!fetchSingleSpoolInfo(spoolId)) {
    updateWebInterface("error", "Cannot reach Spoolman API");
    return;
}
```

### Level 2: Invalid/Missing Data
```cpp
if (data.sku.length() == 0) {
    data.sku = "GENERIC-UNKNOWN";  // Fallback
}
if (data.diameterX100 == 0) {
    data.diameterX100 = 175;  // 1.75mm
}
```

### Level 3: NFC Write Failures
```cpp
if (!writePageVerify(page, pageData)) {
    updateWebInterface("error", "NFC write failed on page " + String(page));
    nfcWriteInProgress = false;
    return;
}
```

### Level 4: Verification Failures
```cpp
// After all writes complete, do full verification
if (!verifyAllPages(acData)) {
    updateWebInterface("error", "Verification failed after write");
    nfcWriteInProgress = false;
    return;
}
```

---

## FUTURE ENHANCEMENTS (NOT IN SCOPE)

### Phase 2: Dual-Tag Support
```cpp
// Currently: Manual (write first tag, place second tag manually)
// Future: Auto-loop after successful first write
for (int tagNum = 0; tagNum < 2; tagNum++) {
    // Wait for user to place next tag
    // Repeat write process
}
```

### Phase 3: Web UI Integration
```
// Currently: Uses internal test functions
// Future: rfid.js adds format selector:
//   [RadioButton] FilaMan JSON
//   [RadioButton] ACE Pro Binary
//   [Write Button]
```

### Phase 4: Extended Ranges (Pages 25-28)
```cpp
// Currently: Not implemented
// Future: Could store temperature ranges for specific nozzles
// Structure: TBD by Anycubic spec update
```

---

## REUSE OF EXISTING INFRASTRUCTURE

### No Changes Needed:
- ✅ NFC hardware interface (`Adafruit_PN532`) - use as-is
- ✅ FreeRTOS task handling - use existing pattern
- ✅ State flags (`nfcWriteInProgress`) - reuse
- ✅ API integration (`fetchSingleSpoolInfo`) - just call
- ✅ Web notifications - use existing function
- ✅ Error handling patterns - mirror existing code

### Leverage Existing Patterns:
- ✅ Page-by-page write with immediate verification (from FilaMan JSON write)
- ✅ Retry logic (up to 3 attempts per page)
- ✅ Task cleanup and interface stabilization
- ✅ Error notification to web UI

---

## TESTING STRATEGY (MANUAL)

1. **Prepare Test Environment:**
   - Have real Anycubic filament NFC tag available
   - Have blank NTAG213 for writing
   - Have NFC reader connected to ESP32

2. **Test Sequence:**
   ```
   a) Write filament data using startWriteNfcTagBinary("123")
   b) Verify page 4 has magic bytes
   c) Verify page 5-8 has correct SKU
   d) Verify page 20 has correct ABGR color
   e) Compare with real Anycubic tag dump format
   ```

3. **Verification:**
   - Use existing NFC reader to dump tag
   - Compare byte-for-byte with spec
   - Test with actual ACE Pro printer (if available)

---

## IMPLEMENTATION CHECKLIST

- [ ] Create `nfc_acepro.h` header file
- [ ] Create `nfc_acepro.cpp` implementation
- [ ] Add export to `nfc.h`
- [ ] Implement `startWriteNfcTagBinary()`
- [ ] Implement `writeNfcTagBinaryTask()`
- [ ] Implement color conversion (hexToABGR)
- [ ] Implement SKU generation
- [ ] Implement page write/verify functions
- [ ] Implement fallback values for missing data
- [ ] Test with sample filament data
- [ ] Verify byte-for-byte against spec
- [ ] Document any deviations from plan
- [ ] Code review with user

---

## SUMMARY

This implementation:
- ✅ Creates **zero impact** on existing FilaMan JSON code
- ✅ Reuses **all existing infrastructure** (NFC, FreeRTOS, API, etc.)
- ✅ Follows **existing code patterns** (page write, verify, error handling)
- ✅ Stores data **formatted correctly** per real tag dumps
- ✅ Focuses on **SKU + Color** (what printer actually uses)
- ✅ Provides **safe fallbacks** for all optional fields
- ✅ Ready for **immediate coding** without further analysis

**Next Step:** Code implementation in `nfc_acepro.h` and `nfc_acepro.cpp`