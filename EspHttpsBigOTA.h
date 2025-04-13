#ifndef ESP8266_SECURE_BIG_OTA_H
#define ESP8266_SECURE_BIG_OTA_H

#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266httpUpdate.h>
#include "HttpsBigOtaRtcStruct.h"

#define __Debug__ Serial

#ifdef __Debug__
#define Debug(fmt, ... ) { __Debug__.printf( "[%d] %s ", __LINE__,  __PRETTY_FUNCTION__); __Debug__.printf(fmt, ## __VA_ARGS__); }
#else 

#define Debug( fnt, ... )
#endif

#define MAX_UPDATE_URL_LENGTH 192


class EspHttpsBigOTA {
public:
    typedef std::function<void()> OTACallback;
    typedef std::function<void(size_t progress, size_t total)> ProgressCallback;
    typedef std::function<void(const String& error)> ErrorCallback;

    // Установка URL для прошивателя
    void setUpdaterUrl(const String& url) {
        _updaterUrl = url;
    }

    enum UpdaterMode {
        AUTO,
        NORMAL,
        FORCE,
    };
    
    bool isUpdaterMode() {
        return ( _updaterMode == FORCE ) ? true : ! _checkFreeSpace();
    }

    void setUpdaterMode(bool _set = true){
        _updaterMode = _set ? FORCE : AUTO;
    }
    void setNormalMode(bool _set = true){
        _updaterMode = _set ? NORMAL : AUTO;
    }

    void begin(const String& firmwareUrl, 
               OTACallback onStart = nullptr,
               ProgressCallback onProgress = nullptr,
               OTACallback onSuccess = nullptr,
               ErrorCallback onError = nullptr) {
        
        assert( firmwareUrl.length()< MAX_UPDATE_URL_LENGTH && "firmwareUrl must be < 192 ");
        assert( WiFi.isConnected() && "WiFi must be connected for run Updater");
        
        _firmwareUrl = firmwareUrl;
        _onStart = onStart;
        _onProgress = onProgress;
        _onSuccess = onSuccess;
        _onError = onError;

        // Проверяем, нужно ли загружать прошиватель
        if ( isUpdaterMode()){
            _prepareUpdaterUpdate();
        } else {
            _startNormalUpdate();
        }
    }


private:
    UpdaterMode _updaterMode = UpdaterMode::AUTO;
    String _firmwareUrl;
    String _updaterUrl; // = "https://example.com/updater.bin";
    
    OTACallback _onStart;
    ProgressCallback _onProgress;
    OTACallback _onSuccess;
    ErrorCallback _onError;

    bool inline _checkFreeSpace() {
        uint32_t maxNewSketchSize = ESP.getSketchSize() + ( 100UL * 1024UL );
        return ESP.getFreeSketchSpace() > maxNewSketchSize;
    }

    bool _prepareUpdaterUpdate() {
        if( !WiFi.getPersistent() || !WiFi.getAutoConnect() ){
            RTCMemoryData data;
            // data.ip = (uint32_t)WiFi.localIP();
            // data.gateway = (uint32_t)WiFi.gatewayIP();
            // data.subnet = (uint32_t)WiFi.subnetMask();
            // data.dns1 = (uint32_t)WiFi.dnsIP(0);
            // data.dns2 = (uint32_t)WiFi.dnsIP(1);
            //if ( WiFi.SSID().length() >= 32 ) return false;
            strcpy(data.ssid, WiFi.SSID().c_str() );
            strcpy(data.psk, WiFi.psk().c_str());
            data.channel = WiFi.channel();
            bssidCpy( data.bssid, WiFi.BSSID());
            strncpy(data.firmwareUrl, _firmwareUrl.c_str(), sizeof(data.firmwareUrl));
            data.crc = _calculateCRC32((uint8_t*)&data, sizeof(data)-sizeof(data.crc));
            if ( ! isValid(data) || 
            ESP.rtcUserMemoryWrite(rtcStructAddress, (uint32_t*)&data, sizeof(data)) ) return false;
        }
        _downloadUpdater();
        return true;
    }

    void _downloadUpdater() {
        WiFiClientSecure client;
        client.setInsecure();
        HTTPClient http;
        
        if(_onStart) _onStart();

        http.begin(client, _updaterUrl);
        int httpCode = http.GET();
        
        if(httpCode != HTTP_CODE_OK) {
            if(_onError) _onError("HTTP error: " + String(httpCode));
            return;
        }

        int len = http.getSize();
        if(len > 410000) {
            if(_onError) _onError("Updater too large");
            return;
        }

        if(!Update.begin(len, U_FLASH)) {
            if(_onError) _onError("Begin failed: " + String(Update.getError()));
            return;
        }

        WiFiClient* stream = http.getStreamPtr();
        uint8_t buffer[256];
        size_t received = 0;
        Update.onProgress( _onProgress );

        while(http.connected() && received < len) {
            received += Update.writeStream( *stream);

            // size_t avail = stream->available();
            // if(avail) {
            //     size_t read = stream->readBytes(buffer, min(avail, sizeof(buffer)));
            //     Update.write(buffer, read);
            //     received += read;
                
            //     if(_onProgress) _onProgress(received, len);
            // }
            //yield();
        }

        if(Update.end(true)) {
            if(_onSuccess) _onSuccess();
            ESP.restart();
            
        } else {
            if(_onError) _onError("Update failed: " + String(Update.getError()));
            
        }
    }

    void _startNormalUpdate() {
        WiFiClientSecure client;
        client.setInsecure();
        HTTPClient http;
        
        if(_onStart) _onStart();

        http.begin(client, _firmwareUrl);
        int httpCode = http.GET();
        
        if(httpCode != HTTP_CODE_OK) {
            if(_onError) _onError("HTTP error: " + String(httpCode));
            return;
        }

        int len = http.getSize();
        if(!_checkSpaceForUpdate(len)) {
            if(_onError) _onError("Not enough space");
            return;
        }

        if(!Update.begin(len, U_FLASH)) {
            if(_onError) _onError("Begin failed: " + String(Update.getError()));
            return;
        }

        WiFiClient* stream = http.getStreamPtr();
        uint8_t buffer[512];
        size_t received = 0;

        while(http.connected() && received < len) {
            size_t avail = stream->available();
            if(avail) {
                size_t read = stream->readBytes(buffer, min(avail, sizeof(buffer)));
                Update.write(buffer, read);
                received += read;
                
                if(_onProgress) _onProgress(received, len);
            }
            yield();
        }

        if(Update.end(true)) {
            if(_onSuccess) _onSuccess();
            ESP.restart();
        } else {
            if(_onError) _onError("Update failed: " + String(Update.getError()));
        }
    }

    bool _checkSpaceForUpdate(size_t required) {
        return ESP.getFreeSketchSpace() > required;
    }


};

#endif