
#include "ledwork.h"
#include "worker.h"
#include "ledstream.h"
#include "log.h"

#include <esp_timer.h>

#include <Adafruit_NeoPixel.h>
#ifdef __AVR__
 #include <avr/power.h> // Required for 16 MHz Adafruit Trinket
#endif

#define NUMPIXELS 60 // Popular NeoPixel ring size

Adafruit_NeoPixel pixels[] = {
    Adafruit_NeoPixel(NUMPIXELS, 27, NEO_GRB + NEO_KHZ800),
    Adafruit_NeoPixel(NUMPIXELS, 25, NEO_GRB + NEO_KHZ800),
    Adafruit_NeoPixel(NUMPIXELS, 26, NEO_GRB + NEO_KHZ800),
    Adafruit_NeoPixel(NUMPIXELS, 32, NEO_GRB + NEO_KHZ800),
};

class _ledWrk : public Wrk {
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

    static int64_t tmill() {
        return esp_timer_get_time() / 1000LL;
    }

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
    }
#ifdef FWVER_DEBUG
    ~_ledWrk() {
        CONSOLE("(0x%08x) destroy", this);
    }
#endif

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
                    continue;
                
                case LSLED:
                    if (pix != NULL) {
                        pix->setPixelColor(d.led.num, acolor(d.led.color));// & 0x00ffffff);
                        lchg = true;
                    };
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
