#pragma once
#include <Arduino.h>
#include <ESP8266WiFi.h>
#define MAX_SSID_LENGTH 31
#define MAX_PSK_LENGTH 31
#define MAX_FW_URL_LENGTH 179

static const int rtcStructAddress = 32;
static uint32_t _calculateCRC32(const uint8_t *data, size_t length) {
    uint32_t crc = 0xffffffff;
    while(length--) {
        uint8_t c = *data++;
        for(uint32_t i = 0x80; i > 0; i >>= 1) {
            bool bit = crc & 0x80000000;
            if(c & i) bit = !bit;
            crc <<= 1;
            if(bit) crc ^= 0x04c11db7;
        }
    }
    return crc;
}

using Bssid_t = uint8_t[6];
inline Bssid_t& bssidCpy(Bssid_t &  dest, const uint8_t * src){
    memcpy(dest, src, sizeof(Bssid_t));
    return dest;
};
inline Bssid_t& bssidCpy(Bssid_t&  dest, const Bssid_t& src){
    memcpy(dest, src, sizeof(Bssid_t));
    return dest;
};

struct RTCMemoryData {
    char ssid[MAX_SSID_LENGTH+1] = {0};
    char psk[MAX_PSK_LENGTH+1] = {0};
    uint8_t channel;
    Bssid_t bssid;
    //uint8_t bssid[6]; // MAC-адрес точки доступа
    // uint32_t ip;
    // uint32_t gateway;
    // uint32_t subnet;
    char firmwareUrl[MAX_FW_URL_LENGTH+1];
    uint32_t crc;
};

//constexpr size_t RTCMemoryDataSize = sizeof(RTCMemoryData);
bool inline _isValid(const char* s, uint8_t size) { return strlen(s) <=size; };

bool isValid ( const RTCMemoryData& data ){
    return ( _isValid( data.ssid, MAX_SSID_LENGTH) &&
             _isValid( data.psk, MAX_PSK_LENGTH ) &&
             _isValid (data.firmwareUrl, MAX_FW_URL_LENGTH ) &&
             _calculateCRC32((uint8_t*)&data, sizeof(data)-sizeof(data.crc)) == data.crc );
}