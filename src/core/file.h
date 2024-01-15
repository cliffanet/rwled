/*
    File
*/

#ifndef _core_file_H
#define _core_file_H

#include <stdint.h>

#define PFNAME(s)   \
    char fname[32]; \
    strncpy_P(fname, s, sizeof(fname));

bool fileInit();
bool fileFormat();

typedef struct {
    uint32_t    used;
    uint32_t    total;
} fs_info_t;
fs_info_t fileInfo();

#endif // _core_file_H
