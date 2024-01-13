
#include "ledtest.h"
#include "core/worker.h"
#include "core/clock.h"
#include "core/btn.h"
#include "core/indicator.h"
#include "core/log.h"
#include "ledwork.h"
#include "wifidirect.h"
#include "jump.h"
#include "power.h"

#include <Adafruit_NeoPixel.h>
#ifdef __AVR__
 #include <avr/power.h> // Required for 16 MHz Adafruit Trinket
#endif

 // Popular NeoPixel ring size
#define NUMPIXELS 5

static Adafruit_NeoPixel pixels[] = {
    Adafruit_NeoPixel(NUMPIXELS, 26, NEO_GRB + NEO_KHZ800), // NEO_KHZ400 / NEO_KHZ800
    Adafruit_NeoPixel(NUMPIXELS, 27, NEO_GRB + NEO_KHZ800),
    Adafruit_NeoPixel(NUMPIXELS, 25, NEO_GRB + NEO_KHZ800),
    Adafruit_NeoPixel(NUMPIXELS, 32, NEO_GRB + NEO_KHZ800),
};

class _ledtestWrk : public Wrk {
    typedef enum {
        WAIT,
        STOP,
        SYNC,
        TIMER,
        SCEN
    } mode_t;

    Btn _btn_stop = Btn(
        [this]() { wait(); }
    );
    Btn _btn_sync = Btn(
        [this]() {
            CONSOLE("to TIMER");
            _mode = TIMER;
            _tmr = 0;
        },
        [this]() { stop(); }
    );
    Btn _btn_scen = Btn(
        powerOff,
        [this]() { stop(); }
    );
    Btn _btn_idle = Btn(
        powerOff,
        [this]() { sync(); }
    );

    Indicator _ind_stop = Indicator(
        [this](uint16_t t) { return true; },
        NULL
    );
    Indicator _ind_sync = Indicator(
        [this](uint16_t t) { return (t % 4) < 2; },
        NULL
    );
    Indicator _ind_scen = Indicator(
        [](uint16_t t){ return t % 2 == 0; },
        NULL
    );

    int64_t tmsync = 0;
    uint32_t tmscen = 0;
    uint32_t col = 0, snd = 0;
    uint16_t _tmr = 0;
    mode_t _mode = WAIT;

    void noind() {
        _ind_stop.hide();
        _ind_sync.hide();
        _ind_scen.hide();
    }
    void wait() {
        CONSOLE("to WAIT");
        _mode = WAIT;
        _btn_idle.activate();
        noind();
        _tmr = 0;
        tmsync = 0;
        tmscen = 0;
    }
    void stop() {
        CONSOLE("to STOP");
        _mode = STOP;
        _btn_stop.activate();
        _ind_stop.activate();
        _tmr = 0;
        tmsync = 0;
        tmscen = 0;
    }
    void sync(int64_t b = tmill()) {
        CONSOLE("to SYNC (%lld)", b);
        _mode = SYNC;
        _btn_sync.activate();
        _ind_sync.activate();
        tmsync = b;
        _tmr = 0;
    }
    void scen(uint32_t b) {
        CONSOLE("to SCEN (%d)", b);
        _mode = SCEN;
        _btn_scen.activate();
        _ind_scen.activate();
        tmscen = b;
        _tmr = 0;
    }

public:
    _ledtestWrk() {
        CONSOLE("(0x%08x) create", this);
        for (auto &p: pixels) p.begin();
        ledOn();
        noind();

        wifiRecv<wifi_beacon_t>(0x01, [](const wifi_beacon_t &d) {
            //CONSOLE("beacon[%d]: %lld", d.num, d.tm);
        });

        // stop
        wifiRecv<uint32_t>(0x09, [this](const uint32_t &tm) {
            if (_mode > STOP)
                wait();
        });

        // sync
        wifiRecv<uint32_t>(0x11, [this](const uint32_t &tm) {
            if (_mode == WAIT)
                sync(tmill() - tm - 20);
            
            if (_mode != SYNC)
                return;
            
            auto t = tmill();
            uint32_t tc = t-tmsync;
            if ((tc < tm) || (tc-tm < 50))
                return;
            tmsync = t - tm - 20;
            CONSOLE("corrected: %lld", tmsync);
        });

        // scen
        wifiRecv<uint32_t>(0x21, [this](const uint32_t &tm) {
            if (_mode == SYNC) {
                scen(tm);
                return;
            }
            
            if (_mode != SCEN)
                return;

            if (tmscen <= tm)
                return;
            tmscen = tm;
            CONSOLE("corrected: %d", tmscen);
        });
    }
    ~_ledtestWrk() {
        CONSOLE("(0x%08x) destroy", this);
        ledOff();
        wifiRecvClear();
    }

    void byjump() {
        if (_mode == SCEN)
            return;
        scen(_mode == SYNC ? tmill()-tmsync : 0);
    }

    state_t run() {
        uint32_t tm = tmill() - tmsync - tmscen;
        uint32_t tc = tm % 12000; 
        uint32_t c = 0;

        switch (_mode) {
            case STOP:
                _tmr ++;
                _tmr %= 5;
                if (_tmr == 1)
                    wifiSend(0x09);
                break;
            
            case SYNC:
                _tmr ++;
                _tmr %= 5;
                if (_tmr == 1)
                    wifiSend<uint32_t>(0x11, tmill()-tmsync);
                c =
                    (tc % 6000 < 2000) &&
                    (tc % 1000 < 500) ?
                        0x555555 :
                        0x000000;
                break;

            case TIMER:
                _tmr ++;
                if (_tmr > 5*33) {
                    _tmr = 0;
                    scen(tmill()-tmsync);
                }
                c =
                    tc % 200 < 100 ?
                        0x555555 :
                        0x000000;
                break;
            
            case SCEN:
                if (tmscen > 0) {
                    _tmr ++;
                    _tmr %= 5;
                    if (_tmr == 1)
                        wifiSend<uint32_t>(0x21, tmscen);
                }
                c =
                    tc > 2100 ?
                        0x00ff00 :
                    tc % 700 < 300 ?
                        0xff0000 :
                        0x000000;
                break;
        }

        if (jumpIsCnp())
            c = 0x0000ff;
        
        if (c != col) {
            for (auto &p: pixels) {
                for (int n = 0; n < NUMPIXELS; n++)
                    p.setPixelColor(n, c);
                p.show();
            }
            for (auto &p: pixels)
                p.show();
            col = c;
        }

        return DLY;
    }

    void end() {
    }
};

static WrkProc<_ledtestWrk> _ltwrk;

void ledStart() {
    if (!_ltwrk.isrun())
        _ltwrk = wrkRun<_ledtestWrk>();
}

void ledByJump() {
    ledStart();
    _ltwrk->byjump();
}
