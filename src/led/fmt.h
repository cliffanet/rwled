/*
    Led stream: data format
*/

#ifndef _led_fmt_H
#define _led_fmt_H

#include <stdint.h>
#include <stddef.h>

#define LEDFMT_HDR      '#'

namespace LedFmt {

    typedef enum {
        FAIL = -1,
        UNKNOWN,
        START,
        TIME,
        CHAN,
        LED,
        LOOP,
        END
    } type_t;

    typedef struct __attribute__((__packed__)) {
        uint8_t     m       = LEDFMT_HDR;
        uint8_t     type;
        uint8_t     sz;
    } head_t;

    typedef struct __attribute__((__packed__)) {
        uint8_t     num;
        uint32_t    color;
    } col_t;

    typedef struct __attribute__((__packed__)) {
        uint32_t    tm;
        uint32_t    len;
        uint32_t    fpos;
    } loop_t;
};

#endif // _led_fmt_H
