#ifndef NFC_H
#define NFC_H

#include <Arduino.h>

typedef enum{
    NFC_IDLE,
    NFC_READING,
    NFC_READ_SUCCESS,
    NFC_READ_ERROR,
    NFC_WRITING,
    NFC_WRITE_SUCCESS,
    NFC_WRITE_ERROR
} nfcReaderStateType;

void startNfc();
void scanRfidTask(void * parameter);
void startWriteJsonToTag(const bool isSpoolTag, const char* payload);
void startWriteNfcTagBinary(const char* spoolId);  // ACE Pro binary format write
bool quickSpoolIdCheck(String uidString);
bool readCompleteJsonForFastPath(); // Read complete JSON data for fast-path web interface display
uint8_t ntag2xx_WriteNDEFWithStartPage(uint8_t startPage, const char *payload);  // NDEF write with custom start page
bool clearNtagUserData();  // Clear all user data (Pages 4-129 for NTAG215, etc.)
String detectNtagType();  // Detect NTAG type (NTAG213/215/216)

extern TaskHandle_t RfidReaderTask;
extern String nfcJsonData;
extern String activeSpoolId;
extern String lastSpoolId;
extern volatile nfcReaderStateType nfcReaderState;
extern volatile bool pauseBambuMqttTask;
extern volatile bool nfcWriteInProgress;
extern bool tagProcessed;



#endif