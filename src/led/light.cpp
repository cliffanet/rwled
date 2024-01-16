
#include "light.h"
#include "fmt.h"
#include "../core/file.h"
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

static TaskHandle_t _scenhnd = NULL;
static void proc_scen( void * param ) {
    const uint32_t col[2] = { 0xff0000, 0x00ff00 };
    uint8_t c = 0;
    CONSOLE("running on core %d", xPortGetCoreID());

    while (1) {
        auto pix = reinterpret_cast<Adafruit_NeoPixel *>(param);
        for (uint8_t x = 0; x < 4; x++, pix++) {
            for (uint8_t n=0; n < NUMPIXELS; n++)
                pix->setPixelColor(n, col[c]);
            pix->show();
        }
        c++;
        c %= 2;
        delay(1000);
    }

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
