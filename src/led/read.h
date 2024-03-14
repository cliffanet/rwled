/*
    Led stream: reader
*/

#ifndef _led_read_H
#define _led_read_H

#include <stddef.h>
#include <stdint.h>
#include "fmt.h"

#define LEDREAD_NUMPIXELS 80
#define LEDLIGHT_PINENABLE  14

#define LED_CHANA       0x1
#define LED_CHANB       0x2
#define LED_CHANC       0x4
#define LED_CHAND       0x8
#define LED_CHAN_ALL    0xf

namespace LedRead {
    void on();
    void off();

    const char *fname();
    bool open();
    bool close();
    size_t fsize();
    uint8_t mynum();

    bool opened();

    void start();
    void stop();
    bool isrun();

    void fixcolor(uint8_t nmask, uint32_t col, bool force = false);
}

#endif // _led_read_H
