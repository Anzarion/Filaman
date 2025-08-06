#include "api.h"
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "commonFS.h"
#include <Preferences.h>
#include "debug.h"

volatile spoolmanApiStateType spoolmanApiState = API_INIT;
//bool spoolman_connected = false;
String spoolmanUrl = "";
bool octoEnabled = false;
bool sendOctoUpdate = false;
String octoUrl = "";
String octoToken = "";
uint16_t remainingWeight = 0;
bool spoolmanConnected = false;

struct SendToApiParams {
    SpoolmanApiRequestType requestType;
    String httpType;
    String spoolsUrl;
    String updatePayload;
    String octoToken;
};

JsonDocument fetchSingleSpoolInfo(int spoolId) {
    HTTPClient http;
    String spoolsUrl = spoolmanUrl + apiUrl + "/spool/" + spoolId;

    Serial.print("Rufe Spool-Daten von: ");
    Serial.println(spoolsUrl);

    http.begin(spoolsUrl);
    int httpCode = http.GET();

    JsonDocument filteredDoc;
    if (httpCode == HTTP_CODE_OK) {
        String payload = http.getString();
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, payload);
        if (error) {
            Serial.print("Fehler beim Parsen der JSON-Antwort: ");
            Serial.println(error.c_str());
        } else {
            String filamentType = doc["filament"]["material"].as<String>();
            String filamentBrand = doc["filament"]["vendor"]["name"].as<String>();

            int nozzle_temp_min = 0;
            int nozzle_temp_max = 0;
            if (doc["filament"]["extra"]["nozzle_temperature"].is<String>()) {
                String tempString = doc["filament"]["extra"]["nozzle_temperature"].as<String>();
                tempString.replace("[", "");
                tempString.replace("]", "");
                int commaIndex = tempString.indexOf(',');
                
                if (commaIndex != -1) {
                    nozzle_temp_min = tempString.substring(0, commaIndex).toInt();
                    nozzle_temp_max = tempString.substring(commaIndex + 1).toInt();
                }
            } 

            String filamentColor = doc["filament"]["color_hex"].as<String>();
            filamentColor.toUpperCase();

            String tray_info_idx = doc["filament"]["extra"]["bambu_idx"].as<String>();
            tray_info_idx.replace("\"", "");
            
            String cali_idx = doc["filament"]["extra"]["bambu_cali_id"].as<String>(); // "\"153\""
            cali_idx.replace("\"", "");
            
            String bambu_setting_id = doc["filament"]["extra"]["bambu_setting_id"].as<String>(); // "\"PFUSf40e9953b40d3d\""
            bambu_setting_id.replace("\"", "");

            doc.clear();

            filteredDoc["color"] = filamentColor;
            filteredDoc["type"] = filamentType;
            filteredDoc["nozzle_temp_min"] = nozzle_temp_min;
            filteredDoc["nozzle_temp_max"] = nozzle_temp_max;
            filteredDoc["brand"] = filamentBrand;
            filteredDoc["tray_info_idx"] = tray_info_idx;
            filteredDoc["cali_idx"] = cali_idx;
            filteredDoc["bambu_setting_id"] = bambu_setting_id;
        }
    } else {
        Serial.print("Fehler beim Abrufen der Spool-Daten. HTTP-Code: ");
        Serial.println(httpCode);
    }

    http.end();
    return filteredDoc;
}

void sendToApi(void *parameter) {
    HEAP_DEBUG_MESSAGE("sendToApi begin");

    spoolmanApiState = API_TRANSMITTING;
    SendToApiParams* params = (SendToApiParams*)parameter;

    // Extrahiere die Werte
    SpoolmanApiRequestType requestType = params->requestType;
    String httpType = params->httpType;
    String spoolsUrl = params->spoolsUrl;
    String updatePayload = params->updatePayload;
    String octoToken = params->octoToken;    

    HTTPClient http;
    http.setReuse(false);

    http.begin(spoolsUrl);
    http.addHeader("Content-Type", "application/json");
    if (octoEnabled && octoToken != "") http.addHeader("X-Api-Key", octoToken);

    int httpCode;
    if (httpType == "PATCH") httpCode = http.PATCH(updatePayload);
    else if (httpType == "POST") httpCode = http.POST(updatePayload);
    else httpCode = http.PUT(updatePayload);

    if (httpCode == HTTP_CODE_OK) {
        Serial.println("Spoolman erfolgreich aktualisiert");

        // Restgewicht der Spule auslesen
        String payload = http.getString();
        JsonDocument doc;
        DeserializationError error = deserializeJson(doc, payload);
        if (error) {
            Serial.print("Fehler beim Parsen der JSON-Antwort: ");
            Serial.println(error.c_str());
        } else {
            switch(requestType){
            case API_REQUEST_SPOOL_WEIGHT_UPDATE:
                remainingWeight = doc["remaining_weight"].as<uint16_t>();
                Serial.print("Aktuelles Gewicht: ");
                Serial.println(remainingWeight);
                //oledShowMessage("Remaining: " + String(remaining_weight) + "g");
                if(!octoEnabled){
                    // TBD: Do not use Strings...
                    oledShowProgressBar(1, 1, "Spool Tag", ("Done: " + String(remainingWeight) + " g remain").c_str());
                    remainingWeight = 0;
                }else{
                    // ocoto is enabled, trigger octo update
                    sendOctoUpdate = true;
                }
                break;
            case API_REQUEST_SPOOL_LOCATION_UPDATE:
                oledShowProgressBar(1, 1, "Loc. Tag", "Done!");
                break;
            case API_REQUEST_SPOOL_TAG_ID_UPDATE:
                oledShowProgressBar(1, 1, "Write Tag", "Done!");
                break;
            case API_REQUEST_OCTO_SPOOL_UPDATE:
                // TBD: Do not use Strings...
                oledShowProgressBar(5, 5, "Spool Tag", ("Done: " + String(remainingWeight) + " g remain").c_str());
                remainingWeight = 0;
                break;
            }
        }
        doc.clear();
    } else {
        switch(requestType){
        case API_REQUEST_SPOOL_WEIGHT_UPDATE:
        case API_REQUEST_SPOOL_LOCATION_UPDATE:
        case API_REQUEST_SPOOL_TAG_ID_UPDATE:
            oledShowProgressBar(1, 1, "Failure!", "Spoolman update");
            break;
        case API_REQUEST_OCTO_SPOOL_UPDATE:
            oledShowProgressBar(1, 1, "Failure!", "Octoprint update");
            break;
        case API_REQUEST_BAMBU_UPDATE:
            oledShowProgressBar(1, 1, "Failure!", "Bambu update");
            break;
        }
        Serial.println("Fehler beim Senden an Spoolman! HTTP Code: " + String(httpCode));

        // TBD: really required?
        vTaskDelay(2000 / portTICK_PERIOD_MS);
    }

    http.end();
    vTaskDelay(50 / portTICK_PERIOD_MS);

    // Speicher freigeben
    delete params;
    HEAP_DEBUG_MESSAGE("sendToApi end");
    spoolmanApiState = API_IDLE;
    vTaskDelete(NULL);
}

bool updateSpoolTagId(String uidString, const char* payload) {
    oledShowProgressBar(2, 3, "Write Tag", "Update Spoolman");

    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payload);
    
    if (error) {
        Serial.print("Fehler beim JSON-Parsing: ");
        Serial.println(error.c_str());
        return false;
    }
    
    // Überprüfe, ob die erforderlichen Felder vorhanden sind
    if (!doc["sm_id"].is<String>() || doc["sm_id"].as<String>() == "") {
        Serial.println("Keine Spoolman-ID gefunden.");
        return false;
    }

    String spoolsUrl = spoolmanUrl + apiUrl + "/spool/" + doc["sm_id"].as<String>();
    Serial.print("Update Spule mit URL: ");
    Serial.println(spoolsUrl);
    
    doc.clear();

    // Update Payload erstellen
    JsonDocument updateDoc;
    updateDoc["extra"]["nfc_id"] = "\""+uidString+"\"";
    
    String updatePayload;
    serializeJson(updateDoc, updatePayload);
    Serial.print("Update Payload: ");
    Serial.println(updatePayload);

    SendToApiParams* params = new SendToApiParams();  
    if (params == nullptr) {
        Serial.println("Fehler: Kann Speicher für Task-Parameter nicht allokieren.");
        return false;
    }
    params->requestType = API_REQUEST_SPOOL_TAG_ID_UPDATE;
    params->httpType = "PATCH";
    params->spoolsUrl = spoolsUrl;
    params->updatePayload = updatePayload;

    // Erstelle die Task
    BaseType_t result = xTaskCreate(
        sendToApi,                // Task-Funktion
        "SendToApiTask",          // Task-Name
        6144,                     // Stackgröße in Bytes
        (void*)params,            // Parameter
        0,                        // Priorität
        NULL                      // Task-Handle (nicht benötigt)
    );

    updateDoc.clear();

    // Update Spool weight
    //TBD: how to handle this with spool and locatin tags? Also potential parallel access again
    //if (weight > 10) updateSpoolWeight(doc["sm_id"].as<String>(), weight);

    return true;
}

uint8_t updateSpoolWeight(String spoolId, uint16_t weight) {
    HEAP_DEBUG_MESSAGE("updateSpoolWeight begin");
    oledShowProgressBar(3, octoEnabled?5:4, "Spool Tag", "Spoolman update");
    String spoolsUrl = spoolmanUrl + apiUrl + "/spool/" + spoolId + "/measure";
    Serial.print("Update Spule mit URL: ");
    Serial.println(spoolsUrl);

    // Update Payload erstellen
    JsonDocument updateDoc;
    updateDoc["weight"] = weight;
    
    String updatePayload;
    serializeJson(updateDoc, updatePayload);
    Serial.print("Update Payload: ");
    Serial.println(updatePayload);

    SendToApiParams* params = new SendToApiParams();
    if (params == nullptr) {
        // TBD: reset ESP instead of showing a message
        Serial.println("Fehler: Kann Speicher für Task-Parameter nicht allokieren.");
        return 0;
    }
    params->requestType = API_REQUEST_SPOOL_WEIGHT_UPDATE;
    params->httpType = "PUT";
    params->spoolsUrl = spoolsUrl;
    params->updatePayload = updatePayload;

    // Erstelle die Task
    BaseType_t result = xTaskCreate(
        sendToApi,                // Task-Funktion
        "SendToApiTask",          // Task-Name
        6144,                     // Stackgröße in Bytes
        (void*)params,            // Parameter
        0,                        // Priorität
        NULL                      // Task-Handle (nicht benötigt)
    );

    updateDoc.clear();
    HEAP_DEBUG_MESSAGE("updateSpoolWeight end");

    return 1;
}

uint8_t updateSpoolLocation(String spoolId, String location){
    HEAP_DEBUG_MESSAGE("updateSpoolLocation begin");

    oledShowProgressBar(3, octoEnabled?5:4, "Loc. Tag", "Spoolman update");

    String spoolsUrl = spoolmanUrl + apiUrl + "/spool/" + spoolId;
    Serial.print("Update Spule mit URL: ");
    Serial.println(spoolsUrl);

    // Update Payload erstellen
    JsonDocument updateDoc;
    updateDoc["location"] = location;
    
    String updatePayload;
    serializeJson(updateDoc, updatePayload);
    Serial.print("Update Payload: ");
    Serial.println(updatePayload);

    SendToApiParams* params = new SendToApiParams();
    if (params == nullptr) {
        Serial.println("Fehler: Kann Speicher für Task-Parameter nicht allokieren.");
        return 0;
    }
    params->requestType = API_REQUEST_SPOOL_LOCATION_UPDATE;
    params->httpType = "PATCH";
    params->spoolsUrl = spoolsUrl;
    params->updatePayload = updatePayload;

    if(spoolmanApiState == API_IDLE){
    // Erstelle die Task
    BaseType_t result = xTaskCreate(
        sendToApi,                // Task-Funktion
        "SendToApiTask",          // Task-Name
        6144,                     // Stackgröße in Bytes
        (void*)params,            // Parameter
        0,                        // Priorität
        NULL                      // Task-Handle (nicht benötigt)
    );

    }else{
        Serial.println("Not spawning new task, API still active!");
    }

    updateDoc.clear();

    HEAP_DEBUG_MESSAGE("updateSpoolLocation end");
    return 1;
}

bool updateSpoolOcto(int spoolId) {
    oledShowProgressBar(4, octoEnabled?5:4, "Spool Tag", "Octoprint update");

    String spoolsUrl = octoUrl + "/plugin/Spoolman/selectSpool";
    Serial.print("Update Spule in Octoprint mit URL: ");
    Serial.println(spoolsUrl);

    JsonDocument updateDoc;
    updateDoc["spool_id"] = spoolId;
    updateDoc["tool"] = "tool0";

    String updatePayload;
    serializeJson(updateDoc, updatePayload);
    Serial.print("Update Payload: ");
    Serial.println(updatePayload);

    SendToApiParams* params = new SendToApiParams();
    if (params == nullptr) {
        Serial.println("Fehler: Kann Speicher für Task-Parameter nicht allokieren.");
        return false;
    }
    params->requestType = API_REQUEST_OCTO_SPOOL_UPDATE;
    params->httpType = "POST";
    params->spoolsUrl = spoolsUrl;
    params->updatePayload = updatePayload;
    params->octoToken = octoToken;

    // Erstelle die Task
    BaseType_t result = xTaskCreate(
        sendToApi,                // Task-Funktion
        "SendToApiTask",          // Task-Name
        6144,                     // Stackgröße in Bytes
        (void*)params,            // Parameter
        0,                        // Priorität
        NULL                      // Task-Handle (nicht benötigt)
    );

    updateDoc.clear();

    return true;
}

bool updateSpoolBambuData(String payload) {
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, payload);
    if (error) {
        Serial.print("Fehler beim JSON-Parsing: ");
        Serial.println(error.c_str());
        return false;
    }

    String spoolsUrl = spoolmanUrl + apiUrl + "/filament/" + doc["filament_id"].as<String>();
    Serial.print("Update Spule mit URL: ");
    Serial.println(spoolsUrl);

    JsonDocument updateDoc;
    updateDoc["extra"]["bambu_setting_id"] = "\"" + doc["setting_id"].as<String>() + "\"";
    updateDoc["extra"]["bambu_cali_id"] = "\"" + doc["cali_idx"].as<String>() + "\"";
    updateDoc["extra"]["bambu_idx"] = "\"" + doc["tray_info_idx"].as<String>() + "\"";
    updateDoc["extra"]["nozzle_temperature"] = "[" + doc["temp_min"].as<String>() + "," + doc["temp_max"].as<String>() + "]";

    String updatePayload;
    serializeJson(updateDoc, updatePayload);

    doc.clear();
    updateDoc.clear();

    Serial.print("Update Payload: ");
    Serial.println(updatePayload);

    SendToApiParams* params = new SendToApiParams();
    if (params == nullptr) {
        Serial.println("Fehler: Kann Speicher für Task-Parameter nicht allokieren.");
        return false;
    }
    params->requestType = API_REQUEST_BAMBU_UPDATE;
    params->httpType = "PATCH";
    params->spoolsUrl = spoolsUrl;
    params->updatePayload = updatePayload;

    // Erstelle die Task
    BaseType_t result = xTaskCreate(
        sendToApi,                // Task-Funktion
        "SendToApiTask",          // Task-Name
        6144,                     // Stackgröße in Bytes
        (void*)params,            // Parameter
        0,                        // Priorität
        NULL                      // Task-Handle (nicht benötigt)
    );

    return true;
}

// #### Spoolman init
bool checkSpoolmanExtraFields() {
    HTTPClient http;
    String checkUrls[] = {
        spoolmanUrl + apiUrl + "/field/spool",
        spoolmanUrl + apiUrl + "/field/filament"
    };

    String spoolExtra[] = {
        "nfc_id"
    };

    String filamentExtra[] = {
        "nozzle_temperature",
        "price_meter",
        "price_gramm",
        "bambu_setting_id",
        "bambu_cali_id",
        "bambu_idx",
        "bambu_k",
        "bambu_flow_ratio",
        "bambu_max_volspeed"
    };

    String spoolExtraFields[] = {
        "{\"name\": \"NFC ID\","
        "\"key\": \"nfc_id\","
        "\"field_type\": \"text\"}"
    };

    String filamentExtraFields[] = {
        "{\"name\": \"Nozzle Temp\","
        "\"unit\": \"°C\","
        "\"field_type\": \"integer_range\","
        "\"default_value\": \"[190,230]\","
        "\"key\": \"nozzle_temperature\"}",

        "{\"name\": \"Price/m\","
        "\"unit\": \"€\","
        "\"field_type\": \"float\","
        "\"key\": \"price_meter\"}",
        
        "{\"name\": \"Price/g\","
        "\"unit\": \"€\","
        "\"field_type\": \"float\","
        "\"key\": \"price_gramm\"}",

        "{\"name\": \"Bambu Setting ID\","
        "\"field_type\": \"text\","
        "\"key\": \"bambu_setting_id\"}",

        "{\"name\": \"Bambu Cali ID\","
        "\"field_type\": \"text\","
        "\"key\": \"bambu_cali_id\"}",

        "{\"name\": \"Bambu Filament IDX\","
        "\"field_type\": \"text\","
        "\"key\": \"bambu_idx\"}",

        "{\"name\": \"Bambu k\","
        "\"field_type\": \"float\","
        "\"key\": \"bambu_k\"}",

        "{\"name\": \"Bambu Flow Ratio\","
        "\"field_type\": \"float\","
        "\"key\": \"bambu_flow_ratio\"}",

        "{\"name\": \"Bambu Max Vol. Speed\","
        "\"unit\": \"mm3/s\","
        "\"field_type\": \"integer\","
        "\"default_value\": \"12\","
        "\"key\": \"bambu_max_volspeed\"}"
    };

    Serial.println("Überprüfe Extrafelder...");

    int urlLength = sizeof(checkUrls) / sizeof(checkUrls[0]);

    for (uint8_t i = 0; i < urlLength; i++) {
        Serial.println();
        Serial.println("-------- Prüfe Felder für "+checkUrls[i]+" --------");
        http.begin(checkUrls[i]);
        int httpCode = http.GET();
    
        if (httpCode == HTTP_CODE_OK) {
            String payload = http.getString();
            JsonDocument doc;
            DeserializationError error = deserializeJson(doc, payload);
            if (!error) {
                String* extraFields;
                String* extraFieldData;
                u16_t extraLength;

                if (i == 0) {
                    extraFields = spoolExtra;
                    extraFieldData = spoolExtraFields;
                    extraLength = sizeof(spoolExtra) / sizeof(spoolExtra[0]);
                } else {
                    extraFields = filamentExtra;
                    extraFieldData = filamentExtraFields;
                    extraLength = sizeof(filamentExtra) / sizeof(filamentExtra[0]);
                }

                for (uint8_t s = 0; s < extraLength; s++) {
                    bool found = false;
                    for (JsonObject field : doc.as<JsonArray>()) {
                        if (field["key"].is<String>() && field["key"] == extraFields[s]) {
                            Serial.println("Feld gefunden: " + extraFields[s]);
                            found = true;
                            break;
                        }
                    }
                    if (!found) {
                        Serial.println("Feld nicht gefunden: " + extraFields[s]);

                        // Extrafeld hinzufügen
                        http.begin(checkUrls[i] + "/" + extraFields[s]);
                        http.addHeader("Content-Type", "application/json");
                        int httpCode = http.POST(extraFieldData[s]);

                         if (httpCode > 0) {
                            // Antwortscode und -nachricht abrufen
                            String response = http.getString();
                            //Serial.println("HTTP-Code: " + String(httpCode));
                            //Serial.println("Antwort: " + response);
                            if (httpCode != HTTP_CODE_OK) {

                                return false;
                            }
                        } else {
                            // Fehler beim Senden der Anfrage
                            Serial.println("Fehler beim Senden der Anfrage: " + String(http.errorToString(httpCode)));
                            return false;
                        }
                        //http.end();
                    }
                    yield();
                    vTaskDelay(100 / portTICK_PERIOD_MS);
                }
            }
            doc.clear();
        }
    }
    
    Serial.println("-------- ENDE Prüfe Felder --------");
    Serial.println();

    http.end();

    return true;
}

bool checkSpoolmanInstance(const String& url) {
    HTTPClient http;
    String healthUrl = url + apiUrl + "/health";

    Serial.print("Überprüfe Spoolman-Instanz unter: ");
    Serial.println(healthUrl);

    http.begin(healthUrl);
    int httpCode = http.GET();

    if (httpCode > 0) {
        if (httpCode == HTTP_CODE_OK) {
            String payload = http.getString();
            JsonDocument doc;
            DeserializationError error = deserializeJson(doc, payload);
            if (!error && doc["status"].is<String>()) {
                const char* status = doc["status"];
                http.end();

                if (!checkSpoolmanExtraFields()) {
                    Serial.println("Fehler beim Überprüfen der Extrafelder.");

                    // TBD
                    oledShowMessage("Spoolman Error creating Extrafields");
                    vTaskDelay(2000 / portTICK_PERIOD_MS);
                    
                    return false;
                }

                spoolmanApiState = API_IDLE;
                oledShowTopRow();
                spoolmanConnected = true;
                return strcmp(status, "healthy") == 0;
            }

            doc.clear();
        }
    } else {
        Serial.println("Error contacting spoolman instance! HTTP Code: " + String(httpCode));
    }
    http.end();
    return false;
}

bool saveSpoolmanUrl(const String& url, bool octoOn, const String& octo_url, const String& octoTk) {
    Preferences preferences;
    preferences.begin(NVS_NAMESPACE_API, false); // false = readwrite
    preferences.putString(NVS_KEY_SPOOLMAN_URL, url);
    preferences.putBool(NVS_KEY_OCTOPRINT_ENABLED, octoOn);
    preferences.putString(NVS_KEY_OCTOPRINT_URL, octo_url);
    preferences.putString(NVS_KEY_OCTOPRINT_TOKEN, octoTk);
    preferences.end();

    //TBD: This could be handled nicer in the future
    spoolmanUrl = url;
    octoEnabled = octoOn;
    octoUrl = octo_url;
    octoToken = octoTk;

    return true;
}

String loadSpoolmanUrl() {
    Preferences preferences;
    preferences.begin(NVS_NAMESPACE_API, true);
    String spoolmanUrl = preferences.getString(NVS_KEY_SPOOLMAN_URL, "");
    octoEnabled = preferences.getBool(NVS_KEY_OCTOPRINT_ENABLED, false);
    if(octoEnabled)
    {
        octoUrl = preferences.getString(NVS_KEY_OCTOPRINT_URL, "");
        octoToken = preferences.getString(NVS_KEY_OCTOPRINT_TOKEN, "");
    }
    preferences.end();
    return spoolmanUrl;
}

bool initSpoolman() {
    oledShowProgressBar(3, 7, DISPLAY_BOOT_TEXT, "Spoolman init");
    spoolmanUrl = loadSpoolmanUrl();
    spoolmanUrl.trim();
    if (spoolmanUrl == "") {
        Serial.println("Keine Spoolman-URL gefunden.");
        return false;
    }

    bool success = checkSpoolmanInstance(spoolmanUrl);
    if (!success) {
        Serial.println("Spoolman nicht erreichbar.");
        return false;
    }

    oledShowTopRow();
    return true;
}