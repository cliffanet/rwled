/*
    Led stream: light-generator
*/

#ifndef _led_light_H
#define _led_light_H

#include <stddef.h>
#include <stdint.h>

void ledInit();
void ledTerm();

#define LED_CHANA       0x1
#define LED_CHANB       0x2
#define LED_CHANC       0x4
#define LED_CHAND       0x8
#define LED_CHAN_ALL    0xf

namespace LedLight {
    void scen();
    bool isscen();
    void chcolor(uint8_t chan, uint32_t col);
};


#endif // _led_light_H
