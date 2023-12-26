/*
    Indicator leds ctrl
*/

#ifndef _indicator_H
#define _indicator_H

#include <functional>

typedef enum {
    IRED = 0,
    IGRN
} indicator_col_t;

void indbegin();
void indClear();

typedef std::function<bool (uint16_t t)> indicator_val_t;
void indicator(indicator_col_t col, indicator_val_t val, uint16_t interval = 10000);

#endif // _indicator_H
