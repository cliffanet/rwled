
#include "file.h"
#include "../core/log.h"

#include <stdlib.h>
#include <cstring>

#include "vfs_api.h"
#include <sys/unistd.h>
#include <sys/dirent.h>
#include "esp_err.h"
#include "esp_spiffs.h"

static const auto mnt = PSTR("");

static void listdir(const char * dname = "") {
    CONSOLE("readdir: %s", dname);
    DIR* dh = opendir(dname);
    if (dh == NULL)
        return;

    while (true) {
        struct dirent* de = readdir(dh);
        if (!de)
            break;
        
        if (de->d_type == DT_DIR)
            CONSOLE("  dir: %s", de->d_name);
        else {
            char fname[64];
            snprintf_P(fname, sizeof(fname), PSTR("%s/%s"), dname, de->d_name);
            struct stat st;
            if (stat(fname, &st) != 0)
                st.st_size=-1;
            CONSOLE("  file: %s (%d)", de->d_name, st.st_size);
        }
    }

    closedir(dh);
}

/* ------------------------------------------------------------------------------------------- *
 *  инициализация
 * ------------------------------------------------------------------------------------------- */
bool fileInit() {
    PFNAME(mnt);
    esp_vfs_spiffs_conf_t conf = {
      .base_path = fname,
      .partition_label = NULL,
      .max_files = 5,
      .format_if_mount_failed = true
    };
    ESPDO(esp_vfs_spiffs_register(&conf), return false);
    
    listdir();
    
    auto inf = fileInfo();
    CONSOLE("Partition size: total: %d, used: %d", inf.total, inf.used);
    
    return true;
}

bool fileFormat() {
    PFNAME(mnt);
    if (esp_spiffs_mounted(NULL)) {
        ESPDO(esp_vfs_spiffs_unregister(NULL), return false);
    }
    ESPDO(esp_spiffs_format(NULL), return false);
    esp_vfs_spiffs_conf_t conf = {
      .base_path = fname,
      .partition_label = NULL,
      .max_files = 5,
      .format_if_mount_failed = false
    };
    ESPDO(esp_vfs_spiffs_register(&conf), return false);
    return true;
}

fs_info_t fileInfo() {
    size_t total = 0, used = 0;
    ESPDO(esp_spiffs_info(NULL, &total, &used), return { 0 });

    return { used, total };
}
