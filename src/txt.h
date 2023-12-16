/*
    text functions
*/

#ifndef _txt_H
#define _txt_H

#include <stddef.h>
#include <stdint.h>

#define ISLNEND(c)      ((c == '\r') || (c == '\n'))
#define ISSPACE(c)      ((c == ' ') || (c == '\t'))

// txtparam - читает текст до появления пробельного символа
// все пробелы будут пропущены, но если наткнёмся на "перевод строки", он не будет пропущен
int txtparam(char *dst, size_t sz, const char *src, char splt = '=');

// txtline - читает строку до обнаружения "перевода строки",
// при этом сам символ "перевода строки" будет пропущен,
// чтобы следующий вызов read_param/read_line начался уже со следующей строки
int txtline(char *dst, size_t sz, const char *src);

class BufTail {
    char *_buf = NULL;
    size_t _sz=0;
    size_t _len=0;
public:
    ~BufTail();

    size_t sz()     { return _sz; }
    size_t len()    { return _len; }

    bool save(const char *buf, size_t sz);
    size_t restore(char *buf);
    void clear();
    void reset();
};

#endif // _txt_H
