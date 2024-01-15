/*
    Led stream: saver
*/

#ifndef _led_save_H
#define _led_save_H

#include "../core/txt.h"

#include <functional>

#define DATALENMAX      3600000

class LedSaver {
    enum {
        HEAD,
        DATA,
        FIN,
        END
    } _state;

public:
    LedSaver();
    void clear();
    bool open();
    bool addstr(const char *s);
};

class LedSaverBuf : public LedSaver {
    BufTail _tail;
    uint32_t _ln = 1;

public:
    void clear();

    typedef std::function<size_t (char *, size_t)> read_t;
    bool read(size_t sz, read_t hnd);
};

#endif // _led_save_H
