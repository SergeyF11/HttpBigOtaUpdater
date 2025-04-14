
#pragma GCC optimize ("Os") // Оптимизация по размеру
#define DEBUG_ESP_HTTP_UPDATE 0 // Отключаем debug-логи


#include "HttpsBigOtaRtcStruct.h"
#include <ESP8266WiFi.h>
#include <Updater.h>
// #include <ESP8266httpUpdate.h>
 #include <ESP8266HTTPClient.h>
#include <WiFiClientSecureBearSSL.h>

//#include <EspHttpsBigOTA.h>

// extern "C" {
// #include "user_interface.h"
// }
//EspHttpsBigOTA otaUpdater;

#define MINIMAL_SIZE
#define _MANUAL_ // 347848 bytes vs 354696

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
    
    // if ( WiFi.getPersistent() ) WiFi.begin();
    // else  {
        WiFi.begin(rtcData.ssid, rtcData.psk, rtcData.channel, rtcData.bssid);
    //WiFi.config(localIP, gateway, subnet, dns1, dns2);
    // }
    
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        Serial_print(".");
        attempts++;
    }

    if (WiFi.status() != WL_CONNECTED)  {
        Serial_println("\nFailed to connect to WiFi");
        return;
    }
    
    

    Serial_print("\nConnected to WiFi ");
    Serial_println( WiFi.SSID() );

    BearSSL::WiFiClientSecure *client = new BearSSL::WiFiClientSecure;
    client->setInsecure();

    // #ifdef MINIMAL_SIZE

    #ifdef _MANUAL_
    const int port = strncmp( rtcData.firmwareUrl, "https", sizeof("https")-1 ) == 0 ? 443 : 80;
    char *p = rtcData.firmwareUrl;
    p += ( port == 443 ) ? sizeof("https:/") : sizeof("http:/");
    
    // site - всё до первого '/'
    char *host = p;
    if((p = strchr(p, '/'))) {
        *p++ = '\0';
    }
    
    // uri - остаток строки
    char *uri = p;

    Serial_printf("Try to connect %s:%d %s\n", host, port, uri );

    if(!client->connect(host, port)) {
        Serial_println("Error: connection");    
        return;
    }

    pinMode( BUILTIN_LED, OUTPUT);
    // Формируем HTTP-запрос вручную
    client->print(String("GET /") + uri + " HTTP/1.1\r\n" +
                 "Host: " + host + "\r\n" +
                 "Accept: application/octet-stream\r\n\r\n");
    
    long size = -1;
    // Пропускаем заголовки
    while(client->connected()) {
      String line = client->readStringUntil('\n');
      if(line.startsWith("Content-Length: ")) {
        size = line.substring(16).toInt();
        
        Update.begin(size);
        // Update.onProgress([](size_t len, size_t size ){
        //     Serial_printf("upgrade progress %lu from %lu\r", len, size );
        //  });
      }
      if(line.startsWith("Content-Type:")) {
        // if(!line.substring(sizeof("Content-Type:")).startsWith("application/octet-stream")) {
          if(!line.endsWith("octet-stream")) {
            Serial_printf("Wrong type: %s\n", line.c_str());   
            return;
        }
      }
      if(line == "\r") break; // Конец заголовков
    }
  
    // Потоковая запись в Updater
    if(Update.isRunning()) {
      Serial_printf("Start update[%d]\n", size);
      uint8_t buffer[1024];
      size_t writed = 0;
      bool ledState = digitalRead(BUILTIN_LED);
      while(client->connected() && writed != size ) {
        unsigned int available = client->available();
        if( available ) {
            auto readed = client->readBytes(buffer, min( sizeof(buffer), available));
            writed += Update.write(buffer, readed);
            ledState = !ledState;
            digitalWrite(BUILTIN_LED, ledState);
            Serial_print("writed "); Serial_println(writed);
        }
        
        
        delay(1);
      };
      if(Update.end()) {
        Serial_println("Update done. Reboot..."); 
        ESP.restart();
      }
    }

    #else
  
    HTTPClient http;
    http.begin( *client, rtcData.firmwareUrl);
    http.addHeader("Accept", "application/octet-stream");
    
    int httpCode = http.GET();
    
    if (httpCode == HTTP_CODE_OK) {
      // Проверяем Content-Type
      if (http.header("Content-Type").equalsIgnoreCase("application/octet-stream")) {
        int len = http.getSize();
        
        // Инициализируем Updater
        if (Update.begin(len)) {
          Serial_printf("Firmware size: %d bytes\n", len);
          
          // Получаем поток данных и пишем в Updater
          WiFiClient *stream = http.getStreamPtr();
          size_t written = Update.writeStream(*stream);
          
          if (written == len) {
            Serial_println("Firmware written successfully");
            if (Update.end()) {
              Serial_println("Update complete! Rebooting...");
              ESP.restart();
            }
          } else {
            Serial_printf("Write failed. Expected %d, got %d\n", len, written);
          }
        } else {
          Serial_println("Not enough space for update");
        }
      } else {
        Serial_println("Invalid Content-Type");
      }
    } else {
      Serial_printf("HTTP error: %d\n", httpCode);
    }
    
    http.end();


    #endif

    // #else
    // // Выполнение OTA обновления
    // ESPhttpUpdate.setLedPin(LED_BUILTIN, LOW);

    

    // t_httpUpdate_return ret = ESPhttpUpdate.update(client, rtcData.firmwareUrl);
    
    // switch(ret) {
    //     case HTTP_UPDATE_FAILED:
    //         //Serial_printf("Update failed. Error: %s\n", ESPhttpUpdate.getLastErrorString().c_str());
    //         break;
    //     case HTTP_UPDATE_NO_UPDATES:
    //         //Serial_println("No updates available");
    //         break;
    //     case HTTP_UPDATE_OK:
    //         //Serial_println("Update successful");
    //         break;
    // }


    // // otaUpdater.setNormalMode();
    // // otaUpdater.begin(rtcData.firmwareUrl,
    // //     []() {
    // //         pinMode(BUILTIN_LED, OUTPUT);
    // //     }, 
    // //     [](size_t progress, size_t total) { // onProgress
    // //         Serial_printf("Progress: %d%%\r", (progress * 100) / total);
    // //         digitalWrite(BUILTIN_LED, !digitalRead(BUILTIN_LED));
    // //     },   
    // //     []() { // onSuccess
    // //         Serial_println("\nUpdate completed successfully!");
    // //         ESP.restart();
    // //     },
    // //     [](const String& error) { // onError
    // //         Serial_println("Update error: " + error);
    // //         delay(10000);
    // //         ESP.restart();
    // //     });
    // #endif
    
}

void loop() {
    delay(100);
    // Не используется
}