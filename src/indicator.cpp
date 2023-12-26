
#include "indicator.h"
#include "clock.h"
#include "bitstring.h"
#include "log.h"

#include "esp32-hal-gpio.h"

#define PINRED  16
#define PINGRN  17

void indbegin() {
    pinMode(PINRED, OUTPUT);
    pinMode(PINGRN, OUTPUT);
}

static void indset(indicator_col_t col, uint8_t val) {
    switch (col) {
        case IRED:
            digitalWrite(PINRED, val);
            return;
        case IGRN:
            digitalWrite(PINGRN, val);
            return;
    }
}

void indClear() {
    digitalWrite(PINRED, LOW);
    digitalWrite(PINGRN, LOW);
}

void indicator(indicator_col_t col, indicator_val_t val, uint16_t interval) {
    uint16_t t = (tmill() % interval) / 100;
    bool v = val(t);

    static uint8_t p = 0;
    auto vc = bit_test(&p, col);
    if (v && !vc) {
        indset(col, HIGH);
        bit_set(&p, col);
    }
    else
    if (!v && vc) {
        indset(col, LOW);
        bit_clear(&p, col);
    }
}
