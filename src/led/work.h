/*
    Led test
*/

#ifndef _led_work_H
#define _led_work_H

#include "../../def.h"
#include <stdint.h>

void ledWork();

typedef enum {
    LED_FF,
    LED_FFEND,
    LED_CNP,
    LED_GND
} led_jmp_t;
void ledByJump(led_jmp_t mode);

#endif // _led_work_H
