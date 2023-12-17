/*
    Data parser
*/

#ifndef _dataparser_H
#define _dataparser_H

#include "txt.h"

#include <functional>

#define DATALENMAX      3600000

class DataParser {
    enum {
        HEAD,
        DATA,
        FIN,
        END
    } _state;

public:
    DataParser();
    void clear();
    bool addstr(const char *s);
};

class DataBufParser : public DataParser {
    BufTail _tail;
    uint32_t _ln = 1;

public:
    void clear();

    typedef std::function<size_t (char *, size_t)> read_t;
    bool read(size_t sz, read_t hnd);
};

#endif // _dataparser_H
