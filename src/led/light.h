/*
    Led light: by scenario + by handle
*/

#ifndef _led_light_H
#define _led_light_H

#include "../../def.h"
#include <stdint.h>

#define LEDLIGHT_NUMPIXELS      128
#define LEDLIGHT_PINENABLE      14

#define LED_CHANA               0x1
#define LED_CHANB               0x2
#define LED_CHANC               0x4
#define LED_CHAND               0x8
#define LED_CHAN_ALL            0xf

namespace LedLight {
    void on();
    void off();

    void start(int64_t tm);
    void stop();
    bool isrun();

    void fixcolor(uint8_t nmask, uint32_t col, bool force = false);
};

#endif // _led_light_H
