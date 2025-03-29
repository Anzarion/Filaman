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
void startWriteJsonToTag(const char* payload);

extern TaskHandle_t RfidReaderTask;
extern String nfcJsonData;
extern String spoolId;
extern volatile nfcReaderStateType nfcReaderState;
extern volatile bool pauseBambuMqttTask;



#endif