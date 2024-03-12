
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
static inline uint32_t acolel(uint32_t acol, uint8_t bit, uint8_t a) {
    return ((((acol >> bit) & 0xff) * a / 0xff) & 0xff) << bit;
}
static uint32_t acolor(uint32_t acol) {
    uint8_t a = (acol >> 24) & 0xff;
    if (a == 0xff)
        return acol & 0x00ffffff;
    return
        acolel(acol, 16, a) |
        acolel(acol,  8, a) |
        acolel(acol,  0, a);
}

static TaskHandle_t _scenhnd = NULL;
static SemaphoreHandle_t _mutex = xSemaphoreCreateMutex();

static void proc_scen( void * param ) {
    auto beg = tmill();
    uint32_t tm = 0;
    CONSOLE("running on core %d", xPortGetCoreID());
    CONSOLE("beg: %lld", beg);

    auto pixall = reinterpret_cast<Adafruit_NeoPixel *>(param);
    auto pix = pixall;

    for (uint8_t x = 0; x < 4; x++, pix++) {
        xSemaphoreTake(_mutex, portMAX_DELAY);
        pix->clear();
        for (uint8_t n=0; n < NUMPIXELS; n++)
            pix->setPixelColor(n, 0x000000);
        pix->show();
        xSemaphoreGive(_mutex);
    }
    pix = NULL;

    bool chg = false;
#define pixredraw() \
        if (chg || (pix != NULL)) { \
            xSemaphoreTake(_mutex, portMAX_DELAY); \
            pix->show(); \
            xSemaphoreGive(_mutex); \
            chg = false; \
        }
        // второй pix->show(); был убран, это сократило отставание от реального времени
        // на сценарии test - с 12 сек до 6 за цикл
        // надо думать, как сделать стабильную прорисовку без мерцаний за максимально короткое время

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
                    pixredraw();
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
                            xSemaphoreTake(_mutex, portMAX_DELAY);
                            pix->setPixelColor(c.num-1, acolor(c.color));
                            xSemaphoreGive(_mutex);
                            chg = true;
                        }
                        //CONSOLE("color: %d = 0x%08x", c.num, c.color);
                    }
                    break;
                
                case LedFmt::LOOP: {
                        pixredraw();
                        auto l = r.d<LedFmt::loop_t>();
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
                    pixredraw();
                    goto theEnd;
            }
        }
    }

    theEnd:
    CONSOLE("finish");
    LedRead::reset();

    vTaskDelete( NULL );
}

static bool runscen(bool run) {
    char nam[16];
    strncpy_P(nam, PSTR("scen"), sizeof(nam));

    _scenhnd = xTaskGetHandle(nam);

    bool isscen = _scenhnd != NULL;

    if (!isscen && run)
        xTaskCreateUniversal(//PinnedToCore(
            proc_scen,      /* Task function. */
            nam,            /* name of task. */
            10240,          /* Stack size of task */
            pixels,         /* parameter of the task */
            0,              /* priority of the task */
            &_scenhnd,      /* Task handle to keep track of created task */
            1               /* pin task to core 0 */
        );

    return isscen;
}

void LedLight::scen() {
    runscen(true);
}

bool LedLight::isscen() {
    return runscen(false);
}

/* ------------------------------------------------ */

void LedLight::chcolor(uint8_t chan, uint32_t col) {
    uint8_t ch = 1;
    static uint32_t colall[4] = { 0, 0, 0, 0 };
    uint32_t *c = colall;

    bool isscen = runscen(false);
    if (isscen) {
        xSemaphoreTake(_mutex, portMAX_DELAY);
        vTaskDelete( _scenhnd );
        _scenhnd = NULL;
        xSemaphoreGive(_mutex);
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

    if (isscen)
        LedRead::reset();

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
