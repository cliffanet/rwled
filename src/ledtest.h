/*
    Led test
*/

#ifndef _ledtest_H
#define _ledtest_H

#include <stdint.h>

void ledStart();

typedef enum {
    LED_FF,
    LED_CNP,
    LED_GND
} led_jmp_t;
void ledByJump(led_jmp_t mode);

#endif // _ledtest_H
