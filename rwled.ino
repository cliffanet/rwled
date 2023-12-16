//#include <Arduino.h>

#include "src/log.h"
#include "src/clock.h"
#include "src/worker.h"
#include "src/dataread.h"

void setup() {
    //Serial.setRxBufferSize(4096);
    Serial.begin(115200);
    initConsoleReader(Serial);

    CONSOLE("init finish");
}

void loop() {
    uint64_t u = utick();
    wrkProcess(30);
    u = utm_diff(u);
    if (u < 29000)
        delay((30000-u) / 1000);
}
