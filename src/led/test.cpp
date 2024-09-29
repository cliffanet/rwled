
#include "test.h"
#include "light.h"
#include "../core/worker.h"
#include "../core/log.h"




class _ltestWrk : public Wrk {
    uint8_t c = 0;

public:
    _ltestWrk(int64_t tm = -1) {
        CONSOLE("(0x%08x) create", this);
    }
    ~_ltestWrk() {
        CONSOLE("(0x%08x) destroy", this);
    }

    void ledcol(uint32_t col) {
        CONSOLE("color: 0x%06x", col);
        LedLight::fixcolor(LED_CHANA | LED_CHANB | LED_CHANC | LED_CHAND, col);
    }
    void ledchan(uint8_t nmask) {
        CONSOLE("chan: 0x%x", nmask);
        LedLight::fixcolor(LED_CHANA | LED_CHANB | LED_CHANC | LED_CHAND, 0x000000);
        LedLight::fixcolor(nmask, 0x888888);
    }

    bool wait() {
        c++;
        if (c < 50)
            return true;
        
        c = 0;
        return false;
    }

    state_t run() {
    WPROC
        ledcol(0xFF0000);
    WPRC_BREAK
        if (wait())
            return DLY;
        
        ledcol(0x00FF00);
    WPRC_BREAK
        if (wait())
            return DLY;

        ledcol(0x0000FF);
    WPRC_BREAK
        if (wait())
            return DLY;

        ledcol(0xFFFFFF);
    WPRC_BREAK
        if (wait())
            return DLY;
        
        ledcol(0x000000);
    WPRC_BREAK
        if (wait())
            return DLY;
        
        ledchan(LED_CHANA);
    WPRC_BREAK
        if (wait())
            return DLY;
        
        ledchan(LED_CHANB);
    WPRC_BREAK
        if (wait())
            return DLY;
        
        ledchan(LED_CHANC);
    WPRC_BREAK
        if (wait())
            return DLY;
        
        ledchan(LED_CHAND);
    WPRC_BREAK
        if (wait())
            return DLY;
        
        ledcol(0x000000);

    WPRC(END);
    }
};

static WrkProc<_ltestWrk> _ltest;

void LedTest::start() {
    if (!_ltest.isrun())
        _ltest = wrkRun<_ltestWrk>();
}

void LedTest::stop() {
    if (_ltest.isrun())
        _ltest.term();
}
