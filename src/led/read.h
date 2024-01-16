/*
    Led stream: reader
*/

#ifndef _led_read_H
#define _led_read_H

#include <stddef.h>
#include <stdint.h>
#include "fmt.h"

namespace LedRead {
    const char *fname();
    bool open();
    bool close();
    size_t fsize();
    uint8_t mynum();

    bool opened();
    LedFmt::type_t get(uint8_t *data, size_t sz);
    template <typename T>
    LedFmt::type_t get(T &data) {
        return get(reinterpret_cast<uint8_t *>(&data), sizeof(T));
    }
    bool seek(size_t pos);
}

#endif // _led_read_H
