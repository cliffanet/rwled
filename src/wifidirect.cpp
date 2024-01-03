/*
    WiFi direct-link functions
*/

#include "wifidirect.h"
#include "core/worker.h"
#include "core/clock.h"
#include "core/log.h"
#include "ledstream.h"
#include "wifiserver.h" // wifiNetIfInit();

#include <esp_err.h>
#include <esp_wifi.h>
#include <esp_now.h>

#include <functional>

#include <pgmspace.h>   // PSTR()
#include <cstring>

/* ------------------------------------------------------------------------------------------- *
 *  Запуск / остановка
 * ------------------------------------------------------------------------------------------- */
static bool event_loop = false;

void _cb(const uint8_t *mac, const uint8_t *data, int sz) {
                char s[sz+1];
                memcpy(s, data, sz);
                s[sz] = '\0';
                CONSOLE("recv from: %02x.%02x.%02x.%02x.%02x.%02x: %s",
                    mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], s);
            }

static bool _init() {
#define ESP(func)   ESPDO(func, return false)

    // event loop
    if (!event_loop) {
        ESP(esp_event_loop_create_default());
        event_loop = true;
    }

    // netif
    if (!wifiNetIfInit())
        return false;
    
    // init
    wifi_init_config_t init_cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP(esp_wifi_init(&init_cfg));
    ESP(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    
    // mode
    ESP(esp_wifi_set_mode(ESPNOW_WIFI_MODE));

    // start
    ESP(esp_wifi_start());
    //ESP(esp_wifi_set_max_tx_power(60));

    // channel
    ESP(esp_wifi_set_channel(CONFIG_ESPNOW_CHANNEL, WIFI_SECOND_CHAN_NONE));
#if CONFIG_ESPNOW_ENABLE_LONG_RANGE
    ESP(esp_wifi_set_protocol(ESPNOW_WIFI_IF, WIFI_PROTOCOL_11B|WIFI_PROTOCOL_11G|WIFI_PROTOCOL_11N|WIFI_PROTOCOL_LR));
#endif
    
    ESP(esp_now_init());
    //ESP(esp_now_set_self_role(ESP_NOW_ROLE_COMBO));

    ESP(esp_now_register_recv_cb(_cb));

    esp_now_peer_info_t peer = { 0 };
    peer.channel = CONFIG_ESPNOW_CHANNEL;
    peer.ifidx = ESPNOW_WIFI_IF;
    peer.encrypt = false;
    memset(peer.peer_addr, 0xFF, sizeof(peer.peer_addr));
    ESP(esp_now_add_peer(&peer));
    
    CONSOLE("wifi started");

#undef ESP
    
    return true;
}

static bool _stop() {
    bool ret = true;
#define ESP(func)   ESPDO(func, ret = false)

    ESP(esp_now_deinit());
    
    ESP(esp_wifi_stop());
    ESP(esp_wifi_deinit());
    
    // Из описания: Note: Deinitialization is not supported yet
    // ESP(esp_netif_deinit());

    ESP(esp_event_loop_delete_default());
    event_loop = false;
    
    CONSOLE("wifi stopped: %d", ret);
    
#undef ESP
    
    return ret;
}

/* ------------------------------------------------------------------------------------------- *
 *  процессинг
 * ------------------------------------------------------------------------------------------- */
class _wnowWrk : public Wrk {
    const int8_t _num = lsnum();
    int64_t _tmsnd = 0;

    static void bcast(uint8_t *data, uint8_t len) {
        uint8_t bcaddr[] = { 0xff,0xff,0xff,0xff,0xff,0xff };
        auto err = esp_now_send(bcaddr, data, len);
        if (err != ESP_OK)
            CONSOLE("send fail (len: %d): %s", len, esp_err_to_name(err));
    }

    static void _recv(const uint8_t *mac, const uint8_t *data, int sz);
    void recv(const uint8_t *mac, const uint8_t *data, int sz) {
        char s[sz+1];
        memcpy(s, data, sz);
        s[sz] = '\0';
        CONSOLE("from: %02x.%02x.%02x.%02x.%02x.%02x: %s",
            mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], s);
    }

public:
    _wnowWrk() {
        //esp_now_register_recv_cb(
        //);
    }
    ~_wnowWrk() {
        esp_now_unregister_recv_cb();
        CONSOLE("(0x%08x) destroy", this);
    }

    state_t run() {
        auto tm = tmill();
        if (tm-_tmsnd >= 2000) {
            char s[64];
            auto l = snprintf_P(s, sizeof(s), PSTR("rwled#%d"), _num);
            bcast(reinterpret_cast<uint8_t*>(s), l);
            _tmsnd = tm;
        }

        return DLY;
    }

    void end() {
        _stop();
    }
};
static WrkProc<_wnowWrk> _wifi;

void _wnowWrk::_recv(const uint8_t *mac, const uint8_t *data, int sz) {
    if (!_wifi.isrun())
        return;
    _wifi->recv(mac, data, sz);
}

bool wifiDirectStart() {
    if (!_init()) {
        _stop();
        return false;
    }

    if (!_wifi.isrun())
        _wifi = wrkRun<_wnowWrk>();
    
    return true;
}

bool wifiDirectStop() {
    if (!_wifi.isrun())
        return false;
    
    _wifi.term();

    return true;
}
