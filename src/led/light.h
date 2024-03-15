/*
    Led light: by scenario + by handle
*/

#ifndef _led_light_H
#define _led_light_H

#include <stdint.h>

#define LEDLIGHT_NUMPIXELS      80

namespace LedLight {
    void start();
    void stop();
    bool isrun();
};

#endif // _led_light_H
