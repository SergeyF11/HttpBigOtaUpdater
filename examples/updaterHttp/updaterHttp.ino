
#include <ESP8266WiFi.h>
#include <ESP8266httpUpdate.h>
#include <ESP8266HTTPClient.h>
#include "HttpsBigOtaRtcStruct.h"

/*
Внимание !!!!!

При работе по http настоятельно рекомендуется использовать подписанные прошивки основной программы и загрузчика
во избежании взлома процесса обновления и замены прошивки в устройстве на несанкционированные.

*/

#define MINIMAL_SIZE
#ifdef MINIMAL_SIZE
    #define Begin(p, bps) 
    #define Serial_println( a ) 
    #define Serial_print( a )
    #define Serial_printf(fmt, ...) 
#else
    #define Begin(p, bps) { p.begin(bps); while(1) {delay(500); if(p) { p.flush(); break; } }; }
    #define Serial_println( a ) Serial.println(a)
    #define Serial_print( a ) Serial.print(a)
    #define Serial_printf(fmt, ...) Serial.printf( fmt, ## __VA_ARGS__)
#endif

void setup() {
    Begin(Serial, 115200);
    Serial_println();
    // Загрузка данных из RTC памяти
    RTCMemoryData rtcData;
    ESP.rtcUserMemoryRead(rtcStructAddress, (uint32_t*)&rtcData, sizeof(rtcData));
    if ( ! isValid( rtcData ) ) {
        Serial_println("Wrong RTC data. Stop update fw.");
        return;
    }

    Serial_println("Connecting to WiFi with saved config...");
    
    // if ( WiFi.getPersistent() && WiFi.getAutoConnect() ) WiFi.begin();
    // else  {

    WiFi.begin(rtcData.ssid, rtcData.psk, rtcData.channel, rtcData.bssid);

    //WiFi.config(localIP, gateway, subnet, dns1, dns2);
    // }

    Serial_println("Connecting to WiFi...");
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        Serial_print(".");
        attempts++;
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        Serial_println("\nConnected to WiFi");
        
        // Выполнение OTA обновления
        ESPhttpUpdate.setLedPin(LED_BUILTIN, LOW);

        HTTPClient client;

        t_httpUpdate_return ret = ESPhttpUpdate.update(client, rtcData.firmwareUrl);
        
        switch(ret) {
            case HTTP_UPDATE_FAILED:
                Serial_printf("Update failed. Error: %s\n", ESPhttpUpdate.getLastErrorString().c_str());
                break;
            case HTTP_UPDATE_NO_UPDATES:
                Serial_println("No updates available");
                break;
            case HTTP_UPDATE_OK:
                Serial_println("Update successful. Restart...");
                ESP.restart();
                //break;
        }
    } else {
        Serial_println("\nFailed to connect to WiFi");
        return;
    }
    
}

void loop() {
    // Не используется
}