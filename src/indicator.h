/*
    Indicator leds ctrl
*/

#ifndef _indicator_H
#define _indicator_H

#include <functional>

#define PINRED  16
#define PINGRN  17

class Indicator {
    public:
        typedef std::function<bool (uint16_t t)> hnd_t;
        const hnd_t red, grn;
        const int64_t tm;
        const uint16_t interval;

        Indicator(hnd_t red, hnd_t grn, uint16_t interval = 10000);
        ~Indicator();
};


#endif // _indicator_H
