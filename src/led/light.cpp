
#include "light.h"
#include "fmt.h"
#include "read.h"
#include "../core/file.h"
#include "../core/clock.h"
#include "../core/log.h"

#include <string.h>
#include "vfs_api.h"
#include <sys/unistd.h>

#include <Adafruit_NeoPixel.h>
#ifdef __AVR__
 #include <avr/power.h> // Required for 16 MHz Adafruit Trinket
#endif

 // Popular NeoPixel ring size
#define NUMPIXELS 80
#define PINENABLE 14

static FILE* fstr = NULL;
static const auto nstr = PSTR("/strm.led");

/* ------------------------------------------------ */

static Adafruit_NeoPixel pixels[] = {
    Adafruit_NeoPixel(NUMPIXELS, 26, NEO_GRB + NEO_KHZ800), // NEO_KHZ400 / NEO_KHZ800
    Adafruit_NeoPixel(NUMPIXELS, 27, NEO_GRB + NEO_KHZ800),
    Adafruit_NeoPixel(NUMPIXELS, 25, NEO_GRB + NEO_KHZ800),
    Adafruit_NeoPixel(NUMPIXELS, 32, NEO_GRB + NEO_KHZ800),
};

void ledInit() {
    for (auto &p: pixels) p.begin();

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

void ledTerm() {
    for (auto &pix: pixels) {
        for (uint8_t n=0; n < NUMPIXELS; n++)
            pix.setPixelColor(n, 0);
        pix.show();
    }
    digitalWrite(PINENABLE, LOW);
}

/* ------------------------------------------------ */

/* ------------------------------------------------ */
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

static TaskHandle_t _scenhnd = NULL;
static void proc_scen( void * param ) {
    auto beg = tmill();
    uint32_t tm = 0;
    CONSOLE("running on core %d", xPortGetCoreID());
    CONSOLE("beg: %lld", beg);

    auto pixall = reinterpret_cast<Adafruit_NeoPixel *>(param);
    auto pix = pixall;
    for (uint8_t x = 0; x < 4; x++, pix++) {
        for (uint8_t n=0; n < NUMPIXELS; n++)
            pix->setPixelColor(n, 0x000000);
        pix->show();
    }
    pix = NULL;

    bool chg = false;
#define pixredraw() \
        if (chg || (pix != NULL)) { \
            pix->show(); \
            pix->show(); \
            chg = false; \
        }

    while (LedRead::opened()) {
        while (tmill() - beg >= tm) {
            auto r = LedRead::get();
            switch (r.type) {
                case LedFmt::FAIL:
                    goto theEnd;
                
                case LedFmt::START:
                    CONSOLE("start: %d", r.d<uint8_t>());
                    break;
                
                case LedFmt::TIME:
                    tm = r.d<uint32_t>();
                    //CONSOLE("tm: %d", tm);
                    break;
                
                case LedFmt::CHAN: {
                        pixredraw();
                        auto n = r.d<uint8_t>();
                        pix = (n > 0) && (n <= 4) ?
                            pixall + n - 1 : NULL;
                        //CONSOLE("ch: %d", n);
                    }
                    break;
                
                case LedFmt::LED: {
                        auto c = r.d<LedFmt::col_t>();
                        if ((pix != NULL) && (c.num > 0) && (c.num <= NUMPIXELS)) {
                            pix->setPixelColor(c.num-1, acolor(c.color));// & 0x00ffffff);
                            chg = true;
                        }
                    }
                    break;
                
                case LedFmt::LOOP: {
                        pixredraw();
                        auto l = r.d<LedFmt::loop_t>();
                        if (!LedRead::seek(l.fpos)) {
                            CONSOLE("seek: %d fail", l.fpos);
                            goto theEnd;
                        }
                        CONSOLE("LOOP: curbeg=%lld, curtm=%lld/%d, tm=%d, len=%d", beg, tmill() - beg, tm, l.tm, l.len);
                        beg += l.len - l.tm;
                        //beg = tmill() +  - l.tm;
                        tm = l.tm;
                        pix = NULL;
                        CONSOLE("beg: %lld / %d", beg, tm);
                        CONSOLE("tmill: %lld", tmill());
                    }
                    break;
                
                case LedFmt::END:
                    CONSOLE("end");
                    goto theEnd;
            }
        }
    }

    theEnd:

    vTaskDelete( NULL );
}

void LedLight::scen() {
    if (_scenhnd == NULL)
        xTaskCreateUniversal(//PinnedToCore(
            proc_scen,      /* Task function. */
            "scen",         /* name of task. */
            10240,          /* Stack size of task */
            pixels,         /* parameter of the task */
            0,              /* priority of the task */
            &_scenhnd,      /* Task handle to keep track of created task */
            1               /* pin task to core 0 */
        );
}

/* ------------------------------------------------ */

void LedLight::chcolor(uint8_t chan, uint32_t col) {
    uint8_t ch = 1;
    static uint8_t colall[4] = { 0, 0, 0, 0 };
    uint8_t *c = colall;

    if (_scenhnd != NULL) {
        vTaskDelete( _scenhnd );
        _scenhnd = NULL;
    }

    for (auto &p: pixels) {
        if (((chan & ch) > 0) && (*c != col)) {
            for (int n = 0; n < NUMPIXELS; n++)
                p.setPixelColor(n, col);
            p.show();
        }
        ch = ch << 1;
        c ++;
    }

    ch = 1;
    c = colall;
    for (auto &p: pixels) {
        if (((chan & ch) > 0) && (*c != col)) {
            *c = col;
            p.show();
        }
        ch = ch << 1;
        c ++;
    }
}
