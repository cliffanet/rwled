
#include "ledwork.h"
#include "worker.h"
#include "ledstream.h"
#include "clock.h"
#include "indicator.h"
#include "log.h"

#include <Adafruit_NeoPixel.h>
#ifdef __AVR__
 #include <avr/power.h> // Required for 16 MHz Adafruit Trinket
#endif

 // Popular NeoPixel ring size
#define NUMPIXELS 60
#define PINENABLE 14

Adafruit_NeoPixel pixels[] = {
    Adafruit_NeoPixel(NUMPIXELS, 26, NEO_GRB + NEO_KHZ800), // NEO_KHZ400 / NEO_KHZ800
    Adafruit_NeoPixel(NUMPIXELS, 27, NEO_GRB + NEO_KHZ800),
    Adafruit_NeoPixel(NUMPIXELS, 25, NEO_GRB + NEO_KHZ800),
    Adafruit_NeoPixel(NUMPIXELS, 32, NEO_GRB + NEO_KHZ800),
};

class _ledWrk : public Wrk {
    const Indicator _ind = Indicator(
        [](uint16_t t){ return t < 10; },
        NULL,
        2000
    );

    uint8_t  num = 0;
    Adafruit_NeoPixel *pix = NULL;
    bool lchg = false;
    uint32_t tm = 0;
    int64_t beg;
    union {
        uint32_t    n32;
        uint8_t     n8;
        ls_rhead_t  hdr;
        ls_led_t    led;
        ls_loop_t   loop;
    } d = {};

    static inline uint32_t acolel(uint32_t acol, uint32_t mask, uint8_t a) {
        return ((acol & mask) * 255 / a) & mask;
    }
    static uint32_t acolor(uint32_t acol) {
        uint8_t a = (acol >> 24) & 0xff;
        if (a == 0xff)
            return acol & 0x00ffffff;
        return
            acolel(acol, 0x00ff0000, a) &
            acolel(acol, 0x0000ff00, a) &
            acolel(acol, 0x000000ff, a);
    }

    inline void pixredraw() {
        if (!lchg || (pix == NULL))
            return;
        pix->show();
        lchg = false;
    }

public:
    _ledWrk() {
        for (auto &p: pixels) p.begin();
        beg = tmill();
        ledOn();
    }
    ~_ledWrk() {
        lsseek(0);
        CONSOLE("(0x%08x) destroy", this);
        ledOff();
    }

    state_t run() {
        if (!lsopened())
            return END;
        
        auto tc = tmill() - beg;
        while (tc > tm) {
            auto type = lsget(d);
            if (lchg && (type != LSLED) && (pix != NULL)) {
                pix->show();
                lchg = false;
            }

            switch (type) {
                case LSFAIL:
                    return END;
                
                case LSSTART:
                    num = d.n8;
                    CONSOLE("start: %d (tmill: %lld)", num, tc);
                    continue;
                
                case LSTIME:
                    tm = d.n32;
                    //CONSOLE("tm: %d", tm);
                    continue;
                
                case LSCHAN:
                    pixredraw();
                    pix = (d.n8 > 0) && (d.n8 <= 4) ?
                        pixels + d.n8 - 1 : NULL;
                    //CONSOLE("ch: %d", d.n8);
                    continue;
                
                case LSLED:
                    if ((pix != NULL) && (d.led.num > 0) && (d.led.num <= NUMPIXELS)) {
                        pix->setPixelColor(d.led.num-1, acolor(d.led.color));// & 0x00ffffff);
                        lchg = true;
                    }
                    continue;
                
                case LSLOOP:
                    pixredraw();
                    if (!lsseek(d.loop.fpos)) {
                        CONSOLE("seek: %d fail", d.loop.fpos);
                        return END;
                    }
                    CONSOLE("LOOP: curbeg=%lld, tm=%d, len=%d", beg, d.loop.tm, d.loop.len);
                    beg += d.loop.len - d.loop.tm;
                    //beg = tmill() +  - d.loop.tm;
                    tm = d.loop.tm;
                    num = 0;
                    pix = NULL;
                    CONSOLE("beg: %lld / %d", beg, tm);
                    CONSOLE("tmill: %lld", tmill());
                    return RUN;
                
                case LSEND:
                    CONSOLE("end");
                    return END;
            }
        }

        pixredraw();

        //CONSOLE("wait: %d/%d", tc, tm);

        return RUN;
    }

    void end() {
        auto tc = tmill() - beg;
        CONSOLE("interval: real=%d, scene=%d", static_cast<uint32_t>(tc), tm);
    }
};

static WrkProc<_ledWrk> _ledwrk;
bool ledStart() {
    if (_ledwrk.isrun())
        return true;
    if (!lsopened())
        return false;
    
    _ledwrk = wrkRun<_ledWrk>();
}

bool ledStop() {
    if (!_ledwrk.isrun())
        return false;
    _ledwrk.term();
    return true;
}

void ledTgl() {
    if (_ledwrk.isrun())
        _ledwrk.term();
    else
        ledStart();
}

void ledOn() {
    if (!digitalRead(PINENABLE)) {
        for (auto &pix: pixels) {
            for (uint8_t n=0; n < NUMPIXELS; n++)
                pix.setPixelColor(n, 0);
            pix.show();
        }
    }

    pinMode(PINENABLE, OUTPUT);
    digitalWrite(PINENABLE, HIGH);
}

void ledOff() {
    digitalWrite(PINENABLE, LOW);
}
