
#include "txt.h"
#include <stdlib.h>
#include <cstring>

/* ------------------------------------------------------------------------------------------- *
 *  работа с текстовыми файлами
 * ------------------------------------------------------------------------------------------- */

int txtparam(char *dst, size_t sz, const char *src, char splt) {
    size_t di = 0;
    int len = 0;

    while (ISSPACE(*src) || ISLNEND(*src)) {
        src++;
        len++;
    }
    
    while ((*src != splt) && (*src != '\0')) {
        if ((sz > 0) && (di < (sz-1)) && (dst != NULL)) {
            dst[di] = *src;
            di ++;
        }
        
        src++;
        len++;
    }

    if ((sz > 0) && (dst != NULL)) {
        while ((di > 0) && ISSPACE(dst[di]))
            // убираем все пробелы в конце найденного параметра
            di --;
        dst[di] = '\0';
    }
    
    if (*src == '\0') return len;

    // стравливаем все пробелы после параметра
    src++;
    len++;
    while (ISSPACE(*src)) {
        src++;
        len++;
    }
    
    return len;
}

int txtline(char *dst, size_t sz, const char *src) {
    size_t di = 0;
    int len = 0;
    
    while (ISSPACE(*src)) {
        src++;
        len++;
    }
    while (!ISLNEND(*src) && (*src != '\0')) {
        if ((sz > 0) && (di < (sz-1)) && (dst != NULL)) {
            dst[di] = *src;
            di ++;
        }
        
        src++;
        len++;
    }

    if ((sz > 0) && (dst != NULL)) {
        while ((di > 0) && ISSPACE(dst[di]))
            // убираем все пробелы в конце найденного параметра
            di --;
        dst[di] = '\0';
    }
    
    if (*src == '\0') return len;

    // стравливаем все переходы на след строку
    while (ISLNEND(*src)) {
        src++;
        len++;
    }
    
    return len;
}

/* ------------------------------------------------------------------------------------------- *
 *  работа с хвостом буфера
 * ------------------------------------------------------------------------------------------- */

BufTail::~BufTail() {
    if (_buf != NULL)
        free(_buf);
}

bool BufTail::save(const char *buf, size_t sz) {
    if (sz > _sz) {
        if (_buf != NULL)
            free(_buf);
        _buf = reinterpret_cast<char *>(malloc(sz));
        if (_buf == NULL) {
            _sz = 0;
            _len = 0;
            return false;
        }

        _sz = sz;
    }

    if (sz > 0)
        memcpy(_buf, buf, sz);
    _len = sz;

    return true;
}

size_t BufTail::restore(char *buf) {
    if ((_buf != NULL) && (_len > 0))
        memcpy(buf, _buf, _len);
    return _len;
}

void BufTail::clear() {
    _len = 0;
}

void BufTail::reset() {
    if (_buf != NULL)
        free(_buf);
    _buf    = NULL;
    _sz     = 0;
    _len    = 0;
}
