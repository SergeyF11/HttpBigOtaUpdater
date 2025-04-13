#include <ESP8266WiFi.h>
#include <EspHttpsBigOTA.h>

const char* ssid = "your_sid";
const char* password = "your_pass";
const char* firmwareUrl = "https://example.com/firmware.bin";
const char* updaterUrl = "https://example.com/updater.bin";

EspHttpsBigOTA otaUpdater;

void setup() {
    
    Serial.begin(115200);
    while(1){
        delay(500);
        if(Serial){
            Serial.flush();
            Serial.println();
            break;
        }
    }

    WiFi.persistent(true); 
    WiFi.begin(ssid, password);
    
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    
    Serial.println("\nConnected to WiFi");

    auto res = ESP.getResetInfoPtr();
    if( res->reason  ) {
        Serial.println("Not hardware reset. Stop");
        while(1){
            delay(1000);
        }
    };


    // Работа через прошивку-загрузчик
    /* 
    загрузит загрузчик минимального размера (см. примеры updaterHttps и updaterHttp )
    необходимо заранее скомпилировать загрузчик для вашего проекта с такой же 
    конфигурацией esp8266:
        размер и распределение FLASH-памяти,
    и разместить прошивку-загрузчик по адресу из updaterUrl
     */
    otaUpdater.setUpdaterMode();
    otaUpdater.setUpdaterUrl( updaterUrl );

    // Обычный режим работы
    //otaUpdater.setNormalMode();
    // по умолчанию апдейтер сам определяет режим работы по наличию свободной памяти
    
    otaUpdater.begin(firmwareUrl,
        []() { // onStart
            Serial.println("Starting firmware update");
        },
        [](size_t progress, size_t total) { // onProgress
            //Serial.printf("Progress: %d%%\r", (progress * 100) / total);
        },
        []() { // onSuccess
            Serial.println("\nUpdate completed successfully!");
        },
        [](const String& error) { // onError
            Serial.println("Update error: " + error);
        }
    );
}

void loop() {
 
    // Ваш основной код
    
}
