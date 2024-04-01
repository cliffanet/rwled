
#include "work.h"
#include "light.h"
#include "../core/worker.h"
#include "../core/clock.h"
#include "../core/btn.h"
#include "../core/display.h"
#include "../core/indicator.h"
#include "../core/log.h"
#include "../wifidirect.h"
#include "../power.h"

class _ledWrk : public Wrk {
    typedef enum {
        WAIT,
        CANOPY,
        STOP,
        SYNC,
        TIMER,
        SCEN
    } mode_t;

    Btn _btn_stop = Btn(
        [this]() { wait(); },
        [this]() { sync(); }
    );
    Btn _btn_sync = Btn(
        [this]() { timer(tmill()-tmsync+5000); },
        [this]() { stop(); }
    );
    Btn _btn_scen = Btn(
        powerOff,
        [this]() { stop(); }
        //[this]() { canopy(); } // для тестирования
    );
    Btn _btn_gnd = Btn(
        [this]() { wait(); }
    );
    Btn _btn_idle = Btn(
        powerOff,
        [this]() { sync(); }
        //[this]() { _mode == WAIT ? canopy() : wait(); } // для тестирования
    );

    Display _dspl = Display(
        [this](U8G2 &u8g2) {
            char s[30];

            u8g2.setFont(u8g2_font_bubble_tr);
            switch (_mode) {
                case CANOPY:
                    SCPY("CNP");
                    u8g2.drawStr(0, 40, s);
                    break;
                case STOP:
                    SCPY("STOP");
                    u8g2.drawStr(0, 40, s);
                    break;
                case SYNC:
                    SCPY("SYNC");
                    u8g2.drawStr(0, 40, s);
                    break;
                case TIMER:
                    SCPY("TMR");
                    u8g2.drawStr(0, 40, s);
                    break;
                case SCEN:
                    if ((_drawtmr % 10) < 5) {
                        SCPY("SHOW");
                        u8g2.drawStr(0, 40, s);
                    }
                    break;
            }

            u8g2.setFont(u8g2_font_tenstamps_mn);
            if ((_mode == SYNC) || (_mode == TIMER) || (_mode == SCEN)) {
                uint32_t tm =
                    _mode == SYNC   ?   tmill() - tmsync :
                    _mode == TIMER  ?   tmsync + tmscen - tmill() :
                    _mode == SCEN   ?   tmill() - tmsync - tmscen :
                                        0;
                
                tm /= 1000;
                SPRN("%d:%02d", tm / 60, tm % 60);
                u8g2.drawStr(72-SWIDTH, 60, s);
            }
        }
    );

#if HWVER < 2
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
#endif

    int64_t tmsync = 0;
    uint32_t tmscen = 0;
    std::function<void()> _sndhnd = NULL;
    uint8_t _sndtmr = 0, _drawtmr = 0;
    mode_t _mode = WAIT;

#if HWVER < 2
    void noind() {
        _ind_stop.hide();
        _ind_sync.hide();
        _ind_scen.hide();
    }
#else
#define noind()
#endif

    void snd(std::function<void()> _hnd = NULL) {
        _sndhnd = _hnd;
        _sndtmr = 0;
    }

    void wait() {
        CONSOLE("to WAIT");
        _mode   = WAIT;
        tmsync  = 0;
        tmscen  = 0;
        _drawtmr= 0;

        _btn_idle.activate();
        noind();
        snd(NULL);
    }
    void stop() {
        CONSOLE("to STOP");
        _mode   = STOP;
        tmsync  = 0;
        tmscen  = 0;
        _drawtmr= 0;

        _btn_stop.activate();
#if HWVER < 2
        _ind_stop.activate();
#endif
        snd([]() { wifiSend(0x09); });
    }
    void sync(int64_t b = tmill()) {
        CONSOLE("to SYNC (%lld)", b);
        _mode   = SYNC;
        tmsync  = b;
        _drawtmr= 0;

        _btn_sync.activate();
#if HWVER < 2
        _ind_sync.activate();
#endif
        snd([this]() { wifiSend<uint32_t>(0x11, tmill()-tmsync); });
    }
    void timer(uint32_t b) {
        CONSOLE("to TIMER (%d)", b);
        _mode   = TIMER;
        tmscen  = b;
        _drawtmr= 0;

        snd([this]() { wifiSend<uint32_t>(0x20, tmscen); });
    }
    void scen(uint32_t b) {
        CONSOLE("to SCEN (%d)", b);
        _mode   = SCEN;
        tmscen  = b;
        _drawtmr= 0;

        _btn_scen.activate();
#if HWVER < 2
        _ind_scen.activate();
#endif
        tmscen > 0 ?
            snd([this]() { wifiSend<uint32_t>(0x21, tmscen); }) :
            snd(NULL);
        
        LedLight::start(tmsync + tmscen);
    }
    void canopy() {
        CONSOLE("to CANOPY");
        _mode   = CANOPY;
        _drawtmr= 0;

        _btn_idle.activate();
        noind();
        snd(NULL);
    }

public:
    _ledWrk() {
        CONSOLE("(0x%08x) create", this);
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

        // timer
        wifiRecv<uint32_t>(0x20, [this](const uint32_t &tm) {
            if (_mode == SYNC) {
                timer(tm);
                return;
            }

            if (_mode != TIMER)
                return;

            if (tmscen >= tm)
            // более поздняя нажатая кнопка заного запускает таймер
                return;
            
            tmscen = tm;
            CONSOLE("corrected: %d", tmscen);
            LedLight::start(tmsync + tmscen);
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
    ~_ledWrk() {
        CONSOLE("(0x%08x) destroy", this);
        wifiRecvClear();
    }

    void byjump(led_jmp_t jmp) {
        switch (jmp) {
            case LED_FF:
                if (_mode != SCEN)
                    scen(_mode == SYNC ? tmill()-tmsync : 0);
                return;
            
            case LED_FFEND:
                canopy();
                return;
            
            case LED_GND:
                if (_mode == CANOPY)
                    _btn_gnd.activate();
                return;
        }
    }

    state_t run() {
        _drawtmr++;

        // переключение из TIMER в SCEN
        if ((_mode == TIMER) && (tmill() >= tmsync + tmscen))
            scen(tmscen);

        // Оповещение по wifi о текущем состоянии
        if (_sndhnd != NULL) {
            if (_sndtmr == 0)
                _sndhnd();
            _sndtmr ++;
            _sndtmr %= 5;
        }

        // Текущий цвет ленты
        uint32_t tm = tmill() - tmsync - tmscen;
        uint32_t tc = tm % 6000;

        switch (_mode) {
            case CANOPY:
                LedLight::fixcolor(
                    LED_CHANA | LED_CHANB,
                    (tm % 2000 > 300) &&
                    ((tm-500) % 2000 > 300) ?
                        0xFFFFFF : 0x000000
                );
                LedLight::fixcolor(LED_CHANC, 0x00ff00);
                LedLight::fixcolor(LED_CHAND, 0xff0000);
                break;

            case SYNC:
                LedLight::fixcolor(
                    LED_CHANA | LED_CHANB,
                    0x070707
                );
                LedLight::fixcolor(
                    LED_CHANC | LED_CHAND,
                    (tm % 6000 < 2000) &&
                    (tm % 1000 < 500) ?
                        0x070707 : 0x000000
                );
                break;

            case TIMER:
                LedLight::fixcolor(
                    LED_CHANA | LED_CHANB,
                    0x070707
                );
                LedLight::fixcolor(
                    LED_CHANC | LED_CHAND,
                    (tm % 500 >= 100) ?
                        0x080808 : 0x000000
                );
                break;

            case SCEN:
                if (!LedLight::isrun())
                    canopy();
                break;
            
            default:
                LedLight::fixcolor(
                    LED_CHAN_ALL, 0x000000
                );
        }

        return DLY;
    }
};

static WrkProc<_ledWrk> _ltwrk;

void ledWork() {
    if (!_ltwrk.isrun())
        _ltwrk = wrkRun<_ledWrk>();
}

void ledByJump(led_jmp_t mode) {
    ledWork();
    _ltwrk->byjump(mode);
}
