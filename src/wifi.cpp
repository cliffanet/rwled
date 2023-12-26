/*
    WiFi functions
*/

#include "wifi.h"
#include "worker.h"
#include "dataparser.h"
#include "ledstream.h"
#include "btn.h"
#include "indicator.h"
#include "log.h"

#include "ledwork.h"

#include <esp_err.h>
#include <esp_wifi.h>

#include <DNSServer.h>
#include <WebServer.h>

static esp_netif_t *_netif = NULL;
static bool event_loop = false;
bool _wifiStop();

/* ------------------------------------------------------------------------------------------- *
 *  webserver
 * ------------------------------------------------------------------------------------------- */
const char html_ok[] PROGMEM = R"rawliteral(
<p>Uploaded</p>
)rawliteral";

const char html_index[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML>
<html lang="en">
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <meta charset="UTF-8">
</head>
<body>
  <p><h1>LED Upload</h1></p>
  <form method="POST" enctype="multipart/form-data" id="formup">
    <input type="file" name="data" id="data" onchange="uploadFile()"/>
    <span>
      <input type="checkbox" name="fmt" id="fmt" />
      format spiffs
    </span>
    <br />
    <progress id="prog" value="0" max="100" style="width:100%%;"></progress>
    <h3 id="status"></h3>
    <br />
    <p>exist file: %d b</p>
    <p>SPIFFS used: %d b</p>
    <p>SPIFFS full: %d b</p>
  </form>
  <script>
    function _(el) {
      return document.getElementById(el);
    }
    function h(el, html) {
      _(el).innerHTML = html;
    }
    function uploadFile() {
      var file = _("data").files[0];
      var f = new FormData();
      if (_("fmt").checked) // before file!
        f.append("fmt", "on");
      f.append("data", file);
      var ajax = new XMLHttpRequest();
      ajax.upload.addEventListener("progress", prog, false);
      ajax.addEventListener("load", cmpl, false);
      ajax.addEventListener("error", err, false);
      ajax.addEventListener("abort", abrt, false);
      ajax.open("POST", "/upload");
      ajax.send(f);
    }
    function prog(e) {
      var p = Math.round((e.loaded / e.total) * 100);
      _("prog").value = p;
      h("status", p + "%% ("+e.loaded+" of "+e.total+" b)");
    }
    function cmpl(e) {
      h("status", e.target.responseText?"OK: "+e.target.responseText:"OK");
    }
    function err(e) {
      h("status", "FAIL");
    }
    function abrt(e) {
      h("status", "Aborted");
    }
  </script>
</body>
</html>
)rawliteral";

/* ------------------------------------------------------------------------------------------- *
 *  процессинг
 * ------------------------------------------------------------------------------------------- */
class _wifiWrk : public Wrk {
    DNSServer dns;
    WebServer web;
    DataBufParser _prs;
    bool fin = false;
    uint32_t _to = 1;

    void upload_reply() {
        web.sendHeader("Connection", "close");
    }
    void upload_data() {
        if (fin) return;
        HTTPUpload &upload = web.upload();
        switch (upload.status) {
            case UPLOAD_FILE_START:
                CONSOLE("Upload: (%d bytes) %s", upload.totalSize, upload.filename.c_str());
                _prs.clear();
                _to = 0;
                {
                    auto f = web.arg(String("fmt"));
                if (f == "on")
                    lsformat();
                else CONSOLE("no fmt: %s", f.c_str());
                }
                break;
            case UPLOAD_FILE_WRITE:
                CONSOLE("read bytes: %d", upload.currentSize);
                {
                    auto ok = _prs.read(upload.currentSize, [this, upload](char *buf, size_t sz) {
                        memcpy(buf, upload.buf, sz);
                        return sz;
                    });
                    if (!ok) {
                        fin = true;
                        web.client().stop();
                        web.client().flush();
                        web.close();
                    }
                }
                _to = 1;
                break;
            case UPLOAD_FILE_END:
                CONSOLE("upload end");
                fin = true;
                web.send_P(200, "text/html", html_ok);
                break;
            case UPLOAD_FILE_ABORTED:
                CONSOLE("upload aborted");
                fin = true;
                break;
        }
    }
    void web_index() {
        char ctype[64];
        strncpy_P(ctype, PSTR("text/html"), sizeof(ctype));

        char s[strlen_P(html_index)+64];
        auto i = lsinfo();
        snprintf_P(s, sizeof(s), html_index, i.fsize, i.used, i.total);

        web.send(200, ctype, (String)s);
    }

public:
    _wifiWrk() :
        web(80)
    {
  delay(100);
        esp_netif_ip_info_t info = { 0 };
        if ((_netif != NULL) && (esp_netif_get_ip_info(_netif, &info) == ESP_OK)) {
            auto ip = IPAddress(info.ip.addr);
            if (dns.start(53, "*", ip))
                CONSOLE("dns ok: %s", ip.toString().c_str());
            else
                CONSOLE("dns bind fail (%s)", ip.toString().c_str());
        }
        else
            CONSOLE("Can\'t get self-IP");

        web.on(
            "/upload", HTTP_POST,
            [this]() { upload_reply(); },
            [this]() { upload_data(); }
        );
        web.onNotFound([this]() { web_index(); });
        web.begin();

        btnMode(BTNWIFI);
    }
    ~_wifiWrk() {
        CONSOLE("(0x%08x) destroy", this);
    }

    state_t run() {
        dns.processNextRequest();
        web.handleClient();
        if (fin) return END;
        if (_to > 0) {
            _to ++;
            if (_to > 2000) {
                CONSOLE("upload timeout");
                return END;
            }
        }

        indicator(IGRN, [](uint16_t v){ return (v == 35) || (v == 37); }, 5000);

        return DLY;
    }

    void end() {
        dns.stop();
        web.stop();
        _wifiStop();
        indClear();
        ledStart();
    }
};
static WrkProc<_wifiWrk> _wifi;

/* ------------------------------------------------------------------------------------------- *
 *  Запуск / остановка
 * ------------------------------------------------------------------------------------------- */
static bool _wifiStart() {
    esp_err_t err;

#define ERR(txt, ...)   { CONSOLE(txt, ##__VA_ARGS__); return false; }
#define ESPRUN(func)    { CONSOLE(TOSTRING(func)); if ((err = func) != ESP_OK) ERR(TOSTRING(func) ": [%d] %s", err, esp_err_to_name(err)); }
    
    // event loop
    if (!event_loop) {
        ESPRUN(esp_event_loop_create_default());
        event_loop = true;
    }

    // netif
    static bool netif_init = false;
    if (!netif_init) {
#if CONFIG_IDF_TARGET_ESP32
        uint8_t mac[8];
        if (esp_efuse_mac_get_default(mac) == ESP_OK)
            esp_base_mac_addr_set(mac);
#endif
        
        ESPRUN(esp_netif_init());
        netif_init = true;
    }
    
    if (_netif == NULL) {
        CONSOLE("esp_netif_create_default_wifi_ap");
        _netif = esp_netif_create_default_wifi_ap();
        if (_netif == NULL)
            ERR("esp_netif_create_default_wifi_ap fail");
        
        /*
        esp_netif_dhcp_status_t status = ESP_NETIF_DHCP_INIT;
        */
        esp_netif_ip_info_t info = { 0 };
        IP4_ADDR(&info.ip, 192, 168, 4, 22);
        info.gw = info.ip;
        IP4_ADDR(&info.netmask, 255, 255, 255, 0);

        ESPRUN(esp_netif_dhcps_stop(_netif));
        ESPRUN(esp_netif_set_ip_info(_netif, &info));
        auto mask = info.netmask.addr;
        ESPRUN(esp_netif_dhcps_option(_netif, ESP_NETIF_OP_SET, ESP_NETIF_SUBNET_MASK, (void*)&mask, sizeof(mask)));
        dhcps_lease_t lease = { 0 };
        lease.enable = true;
        IP4_ADDR(&lease.start_ip, 192, 168, 4, 101);
        IP4_ADDR(&lease.end_ip, 192, 168, 4, 111);
        ESPRUN(esp_netif_dhcps_option(_netif, ESP_NETIF_OP_SET, ESP_NETIF_REQUESTED_IP_ADDRESS, (void*)&lease, sizeof(lease)));
        ESPRUN(esp_netif_dhcps_start(_netif));
    }
    
    // init
    wifi_init_config_t init_cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESPRUN(esp_wifi_init(&init_cfg));
    ESPRUN(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    
    // mode
    ESPRUN(esp_wifi_set_mode(WIFI_MODE_AP));

    // config
    wifi_config_t cfg = { 0 };
    cfg.ap.channel          = 6;
    cfg.ap.max_connection   = 4;
    cfg.ap.beacon_interval  = 100;
    cfg.ap.authmode         = WIFI_AUTH_OPEN;

    uint16_t n = esp_random();
    cfg.ap.ssid_len =
        snprintf_P(
            reinterpret_cast<char*>(cfg.ap.ssid),
            sizeof(cfg.ap.ssid),
            PSTR("rwled-%04x"), n
        );
    ESPRUN(esp_wifi_set_config(WIFI_IF_AP, &cfg));

    // start
    ESPRUN(esp_wifi_start());
    //ESPRUN(esp_wifi_set_max_tx_power(60));
    
    CONSOLE("wifi started");

#undef ERR
#undef ESPRUN
    
    return true;
}

bool wifiStart() {
    if (!_wifiStart()) {
        wifiStop();
        return false;
    }

    if (!_wifi.isrun())
        _wifi = wrkRun<_wifiWrk>();
    
    return true;
}

bool _wifiStop() {
    esp_err_t err;
    bool ret = true;
    
#define ERR(txt, ...)   { CONSOLE(txt, ##__VA_ARGS__); ret = false; }
#define ESPRUN(func)    { CONSOLE(TOSTRING(func)); if ((err = func) != ESP_OK) ERR(TOSTRING(func) ": [%d] %s", err, esp_err_to_name(err)); }
    
    ESPRUN(esp_wifi_stop());
    ESPRUN(esp_wifi_deinit());
    
    if (_netif != NULL) {
        ESPRUN(esp_netif_dhcps_stop(_netif));
        esp_netif_destroy_default_wifi(_netif);
        _netif = NULL;
    }
    
    // Из описания: Note: Deinitialization is not supported yet
    // ESPRUN(esp_netif_deinit());

    ESPRUN(esp_event_loop_delete_default());
    event_loop = false;
    
    CONSOLE("wifi stopped");
    
#undef ERR
#undef ESPRUN
    
    return ret;
}

bool wifiStop() {
    if (_wifi.isrun())
        _wifi.term();
    else {
        _wifi.reset();
        return _wifiStop();
    }
    return true;
}
