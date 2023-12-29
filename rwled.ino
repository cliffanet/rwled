//#include <Arduino.h>

#include "src/log.h"
#include "src/clock.h"
#include "src/worker.h"
#include "src/console.h"
#include "src/ledstream.h"
#include "src/wifi.h"
#include "src/power.h"

void setup() {
    Serial.setRxBufferSize(4096);
    Serial.begin(115200);
    powerStart(true);
}

void pwrinit() {
    lsbegin();
    //initConsoleReader(Serial);
    wifiStart();
    
    CONSOLE("init finish");
}

void loop() {
    uint64_t u = utick();
    wrkProcess(30);
    u = utm_diff(u);
    if (u < 29000)
        delay((30000-u) / 1000);
}
