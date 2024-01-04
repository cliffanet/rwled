/*
    Led test
*/

#ifndef _ledtest_H
#define _ledtest_H

#include <stdint.h>

bool ledTestStart();
bool ledTestStop();
void ledTestTgl();
void ledTestCorrect(uint32_t tm);

#endif // _ledtest_H
