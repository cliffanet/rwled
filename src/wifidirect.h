/*
    WiFi direct-link functions
*/

#ifndef _wifi_direct_H
#define _wifi_direct_H

#include "../def.h"
#include "core/bindata.h"

#include <stdint.h>


/* ESPNOW can work in both station and softap mode. It is configured in menuconfig. */

#define CONFIG_ESPNOW_WIFI_MODE_STATION 1
#if CONFIG_ESPNOW_WIFI_MODE_STATION
#define ESPNOW_WIFI_MODE WIFI_MODE_STA
#define ESPNOW_WIFI_IF   WIFI_IF_STA
#else
#define ESPNOW_WIFI_MODE WIFI_MODE_AP
#define ESPNOW_WIFI_IF   WIFI_IF_AP
#endif

#define CONFIG_ESPNOW_CHANNEL 1

bool wifiDirectStart();
bool wifiDirectStop();

bool wifiBcast(const uint8_t *data, uint8_t len);

template <typename T>
bool wifiSend(uint16_t cmd, const T &data) {
    auto d = BinData(cmd, data);
    return wifiBcast(d.buf(), d.sz());
}
bool wifiSend(uint16_t cmd);

typedef struct __attribute__((__packed__)) {
    uint8_t num;
    int64_t tm;
} wifi_beacon_t;

#endif // _wifi_direct_H
