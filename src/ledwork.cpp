
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
        bool lchg = false;
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
                    CONSOLE("start: %d", num);
                    return RUN;
                
                case LSTIME:
                    tm = d.n32;
                    //CONSOLE("tm: %d", tm);
                    return RUN;
                
                case LSCHAN:
                    pix = (d.n8 > 0) && (d.n8 <= 4) ?
                        pixels + d.n8 - 1 : NULL;
                    break;
                
                case LSLED:
                    if (pix != NULL) {
                        pix->setPixelColor(d.led.num, d.led.color);// & 0x00ffffff);
                        lchg = true;
                    };
                    break;
                
                case LSEND:
                    CONSOLE("end");
                    return END;
            }
        }

        if (lchg && (pix != NULL)) {
            pix->show();
            lchg = false;
        }

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
