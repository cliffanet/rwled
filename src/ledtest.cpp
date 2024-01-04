
#include "ledtest.h"
#include "core/worker.h"
#include "core/clock.h"
#include "core/indicator.h"
#include "core/log.h"
#include "ledwork.h"
#include "wifidirect.h"
#include "jump.h"

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
    const Indicator _ind = Indicator(
        [](uint16_t t){ return t < 10; },
        NULL,
        2000
    );

    int64_t beg;
    uint32_t col = 0, snd = 0;

public:
    _ledtestWrk() {
        for (auto &p: pixels) p.begin();
        beg = tmill();
        ledOn();
        wifiSend(0x11);
    }
    ~_ledtestWrk() {
        CONSOLE("(0x%08x) destroy", this);
        ledOff();
        wifiSend(0x12);
    }

    void tmcorrect(uint32_t tm) {
        auto t = tmill();
        uint32_t tc = t-beg;
        CONSOLE("correct: tc=%d, recv tm=%d", tc, tm);
        if ((tc > tm) || (tm-tc < 100) || (tm-tc > 30000))
            return;
        beg = t - tm - 20;
        CONSOLE("corrected: %lld", beg);
    }

    state_t run() {
        uint32_t tm = tmill() - beg;
        
        uint32_t tc = tm % 12000; 
        uint32_t c =
            jumpIsCnp() ?
                0x0000ff :
            tc > 2100 ?
                0x00ff00 :
            tc % 700 < 300 ?
                0xff0000 :
                0x000000;
        
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

        if (tm-snd >= 1000) {
            wifiSend(0x10, tm);
            snd = tm;
        }

        return RUN;
    }

    void end() {
    }
};

static WrkProc<_ledtestWrk> _ltwrk;
bool ledTestStart() {
    if (_ltwrk.isrun())
        return true;
    
    _ltwrk = wrkRun<_ledtestWrk>();
}

bool ledTestStop() {
    if (!_ltwrk.isrun())
        return false;
    _ltwrk.term();
    return true;
}

void ledTestTgl() {
    if (_ltwrk.isrun()) {
        _ltwrk.term();
        wifiSend(0x13);
    }
    else
        ledTestStart();
}

void ledTestCorrect(uint32_t tm) {
    if (!_ltwrk.isrun()) {
        if (tm > 30000)
            return;
        ledTestStart();
        if (!_ltwrk.isrun())
            return;
    }
    _ltwrk->tmcorrect(tm);
}
