/*
    Led: pixel driver
*/

#ifndef _led_ws2812_H
#define _led_ws2812_H

#include <stddef.h>
#include <stdint.h>

namespace LedDriver {
    void init(uint8_t chan, uint8_t pin);
    void write(uint8_t chan, uint8_t *data, size_t sz);
    void wait(uint8_t chan);
    void done(uint8_t chan, uint8_t pin);
    void make(uint8_t pin, uint8_t *data, size_t sz);
}

#endif // _led_ws2812_H
