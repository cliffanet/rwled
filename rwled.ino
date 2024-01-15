//#include <Arduino.h>

#include "src/core/log.h"
#include "src/core/clock.h"
#include "src/core/worker.h"
#include "src/core/file.h"
#include "src/console.h"
#include "src/wifiserver.h"
#include "src/power.h"

#include "src/jump.h"
#include "src/led/work.h"

void setup() {
    Serial.setRxBufferSize(4096);
    Serial.begin(115200);
    powerStart(true);

    // временно прямое включение для отладки
    //pwrinit();
}

void pwrinit() {
    CONSOLE("Firmware " FWVER_FILENAME "; Build Date: " __DATE__);
    fileInit();
    //initConsoleReader(Serial);
    wifiSrvStart();
    
    CONSOLE("init finish");
}

void loop() {
    uint64_t u = utick();
    wrkProcess(30);
    u = utm_diff(u);
    if (u < 29000)
        delay((30000-u) / 1000);
}
