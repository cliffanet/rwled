/*
    WiFi direct-link functions
*/

#include "wifidirect.h"
#include "core/worker.h"
#include "core/clock.h"
#include "core/log.h"
#include "jump.h"
#include "wifiserver.h" // wifiNetIfInit();
#include "led/read.h"

#include <esp_err.h>
#include <esp_wifi.h>
#include <esp_now.h>

#include <functional>

#include <pgmspace.h>   // PSTR()
#include <cstring>
#include <map>

/* ------------------------------------------------------------------------------------------- *
 *  Запуск / остановка
 * ------------------------------------------------------------------------------------------- */
static bool event_loop = false;

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

    esp_now_peer_info_t peer = { 0 };
    peer.channel = CONFIG_ESPNOW_CHANNEL;
    peer.ifidx = ESPNOW_WIFI_IF;
    peer.encrypt = false;
    memset(peer.peer_addr, 0xFF, sizeof(peer.peer_addr));
    ESP(esp_now_add_peer(&peer));

    uint8_t mac[6];
    esp_wifi_get_mac(ESPNOW_WIFI_IF, mac);
    CONSOLE("mac: %02x:%02x:%02x:%02x:%02x:%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    
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
    int64_t _tmsnd = 0;
    std::map<uint16_t, wifi_rcv_t> _rcv;

    static void _recv(const uint8_t *mac, const uint8_t *data, int sz);
    void recv(const uint8_t *mac, const uint8_t *data, int sz) {
        //CONSOLE("from: %02x.%02x.%02x.%02x.%02x.%02x",
        //    mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
        
        auto d = BinRecv(data, sz);
        if (!d) {
            CONSOLE("data not valid: %d", d.state());
            return;
        }

        auto r = _rcv.find(d.cmd());
        if (r == _rcv.end()) {
            CONSOLE("unknown: sz=%d, cmd=0x%02x, len=%d, state=%d", sz, d.cmd(), d.len(), d.state());
            return;
        }

        r->second(d);
    }

public:
    _wnowWrk() {
        esp_now_register_recv_cb(_recv);
    }
    ~_wnowWrk() {
        esp_now_unregister_recv_cb();
        CONSOLE("(0x%08x) destroy", this);
    }

    bool rcvempty() const {
        return _rcv.empty();
    }

    bool add(uint16_t cmd, wifi_rcv_t hnd) {
        CONSOLE("[%d]: 0x%08x", _rcv.size(), hnd);
        const auto f = _rcv.find(cmd);
        if (f != _rcv.end())
            return false;
        _rcv[cmd] = hnd;
        return true;
    }
    /*
    bool add(uint16_t cmd, WiFiRecvBase *r) {
        CONSOLE("[%d]: 0x%08x", _rcv.size(), r);
        const auto f = _rcv.find(cmd);
        if (f != _rcv.end())
            return f->second == r;
        _rcv[cmd] = r;
        return true;
    }
    bool del(uint16_t cmd, WiFiRecvBase *r) {
        CONSOLE("[%d]: 0x%02x / 0x%08x", _rcv.size(), cmd, r);
        const auto f = _rcv.find(cmd);
        if (f == _rcv.end())
            return false;
        CONSOLE("found");
        if (f->second != r)
            return false;
        CONSOLE("equal");
        _rcv.erase(f);
        return true;
    }
    */

    state_t run() {
        auto tm = tmill();
        if (tm-_tmsnd >= 2000) {
            wifi_beacon_t r = { LedRead::mynum(), tm };
            wifiSend(0x01, r);
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

bool wifiBcast(const uint8_t *data, uint8_t len) {
    uint8_t bcaddr[] = { 0xff,0xff,0xff,0xff,0xff,0xff };
    auto err = esp_now_send(bcaddr, data, len);
    if (err != ESP_OK)
        CONSOLE("send fail (len: %d): %s", len, esp_err_to_name(err));
    return err == ESP_OK;
}

bool wifiSend(uint16_t cmd) {
    auto d = BinCmd(cmd);
    return wifiBcast(d.buf(), d.sz());
}

bool wifiRecvAdd(uint16_t cmd, wifi_rcv_t hnd) {
    if (!_wifi.isrun()) {
        if (!_init())
            return false;
        _wifi = wrkRun<_wnowWrk>();
    }
    return _wifi->add(cmd, hnd);
}

bool wifiRecvClear() {
    if (!_wifi.isrun())
        return false;
    _wifi.term();
    return true;
}
