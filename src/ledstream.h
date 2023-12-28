/*
    Led stream
*/

#ifndef _ledstream_H
#define _ledstream_H

#include <stdint.h>
#include <stddef.h>

#define LSHDR   '#'

typedef enum {
    LSFAIL = -1,
    LSUNKNOWN,
    LSSTART,
    LSTIME,
    LSCHAN,
    LSLED,
    LSLOOP,
    LSEND
} ls_type_t;

typedef struct __attribute__((__packed__)) {
    uint8_t     m       = LSHDR;
    uint8_t     type;
    uint8_t     sz;
} ls_rhead_t;

typedef struct __attribute__((__packed__)) {
    uint8_t     num;
    uint32_t    color;
} ls_led_t;

typedef struct __attribute__((__packed__)) {
    uint32_t    tm;
    uint32_t    len;
    uint32_t    fpos;
} ls_loop_t;

typedef struct {
    uint32_t    fsize;
    uint32_t    used;
    uint32_t    total;
} ls_info_t;

bool lsbegin();
bool lsformat();
ls_info_t lsinfo();

bool lsopen();
bool lsclose();
uint8_t lsnum();
bool lsopened();
ls_type_t lsget(uint8_t *data, size_t sz);
template <typename T>
ls_type_t lsget(T &data) {
    return lsget(reinterpret_cast<uint8_t *>(&data), sizeof(T));
}
bool lsseek(size_t pos);


bool lstmp();
bool lsadd(ls_type_t type, const uint8_t *data, size_t sz);
inline
bool lsadd(ls_type_t type) {
    return lsadd(type, NULL, 0);
}
template <typename T>
bool lsadd(ls_type_t type, const T &data) {
    return lsadd(type, reinterpret_cast<const uint8_t *>(&data), sizeof(T));
}
int  lsfindtm(uint32_t tm);
bool lsfin();

#endif // _ledstream_H
