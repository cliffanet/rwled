/*
    WiFi direct-link functions
*/

#ifndef _wifi_direct_H
#define _wifi_direct_H

#include "../def.h"
#include "core/bindata.h"

#include <stdint.h>
#include <functional>


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

typedef std::function<void(BinRecv &d)> wifi_rcv_t;

bool wifiRecvAdd(uint16_t cmd, wifi_rcv_t hnd);
template <typename T>
bool wifiRecv(uint16_t cmd, std::function<void (const T &d)> hnd) {
    return wifiRecvAdd(cmd, [hnd](BinRecv &d) {
        if (hnd != NULL)
            hnd(d.data<T>());
    });
}
bool wifiRecvClear();

#endif // _wifi_direct_H
