
#include "ledstream.h"
#include "core/log.h"

#include <stdlib.h>
#include <cstring>

#include "vfs_api.h"
#include <sys/unistd.h>
#include <sys/dirent.h>
#include "esp_err.h"
#include "esp_spiffs.h"

static const auto mnt = PSTR("");
#define PFNAME(s)   \
    char fname[32]; \
    strncpy_P(fname, s, sizeof(fname));

void listdir(const char * dname = "") {
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

static inline FILE* open_P(const char *name, char m = 'r') {
    PFNAME(name);
    char mode[2] = { m };
    FILE* f = fopen(fname, mode);
    if (f)
        CONSOLE("opened[%s] ok: %s", mode, fname);
    else
        CONSOLE("can't open[%s] %s", mode, fname);
    
    return f;
}

/* ------------------------------------------------------------------------------------------- *
 *  инициализация
 * ------------------------------------------------------------------------------------------- */
static bool initok = false;
bool lsbegin() {
    PFNAME(mnt);
    esp_vfs_spiffs_conf_t conf = {
      .base_path = fname,
      .partition_label = NULL,
      .max_files = 5,
      .format_if_mount_failed = true
    };
    ESPDO(esp_vfs_spiffs_register(&conf), return false);
    
    initok = true;
    lsopen();

    const char dir[] = {'/', '\0'};
    listdir();
    
    size_t total = 0, used = 0;
    ESPDO(esp_spiffs_info(NULL, &total, &used), return false);
    CONSOLE("Partition size: total: %d, used: %d", total, used);
    
    return true;
}

bool lsformat() {
    lsclose();
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

/* ------------------------------------------------------------------------------------------- *
 *  основной файл потока
 * ------------------------------------------------------------------------------------------- */
static FILE* fstr = NULL;
static const auto nstr = PSTR("/strm.led");
static uint8_t _mynum = 0;
static uint8_t buf[1100];
static uint16_t bc = 0, bl = 0;

ls_info_t lsinfo() {
    size_t total = 0, used = 0;
    ESPDO(esp_spiffs_info(NULL, &total, &used), return { 0 });

    PFNAME(nstr);
    struct stat st;
    if (stat(fname, &st) != 0)
        st.st_size=-1;

    return { st.st_size, used, total };
}

bool lsopen() {
    if (fstr != NULL)
        return false;
    
    fstr = open_P(nstr);
    if (fstr == NULL) return false;

    if (lsget(_mynum) != LSSTART) {
        CONSOLE("file format fail");
        _mynum = 0;
        lsseek(0);
    }
    else
        CONSOLE("mynum: %d", _mynum);
    
    return fstr != NULL;
}

bool lsclose() {
    if (fstr == NULL)
        return false;
    
    fclose(fstr);
    fstr = NULL;
    _mynum = 0;
    bc = 0;
    bl = 0;

    return true;
}

uint8_t lsnum() {
    return _mynum;
}

bool lsopened() {
    return fstr != NULL;
}

static bool _bufread(uint8_t *data, size_t sz) {
    if (sz > bl) {
        if (fstr == NULL) {
            CONSOLE("led stream not opened");
            return LSFAIL;
        }
        if (bc > 0) {
            for (int i = 0; i < bl; i++)
                buf[i] = buf[bc+i];
            bc = 0;
        }
        size_t l = 1024;
        if (l+bl > sizeof(buf))
            l = sizeof(buf) - bl;
        l = fread(buf+bl, 1, l, fstr);
        //CONSOLE("buf readed: %d / %d", l, ftell(fstr));
        if (l < 1) {
            CONSOLE("[%d]: can\'t file read", ftell(fstr));
            return false;
        }
        bl += l;
    }

    if (sz > bl) {
        CONSOLE("[%d]: no buf[%d] data(%d)", ftell(fstr), sz, bl);
        return false;
    }

    if (data != NULL)
        memcpy(data, buf+bc, sz);
    bc += sz;
    bl -= sz;

    return true;
}

ls_type_t lsget(uint8_t *data, size_t sz) {
    ls_rhead_t h;
    if (!_bufread(reinterpret_cast<uint8_t *>(&h), sizeof(h))) {
        CONSOLE("[%d]: hdr read", ftell(fstr));
        return LSFAIL;
    }
    if (h.m != LSHDR) {
        CONSOLE("[%d]: hdr fail", ftell(fstr));
        return LSFAIL;
    }

    size_t l = h.sz < sz ? h.sz : sz;
    if (l < 0) l = 0;
    if ((l > 0) && !_bufread(data, l)) {
        CONSOLE("[%d]: data read", ftell(fstr));
        return LSFAIL;
    }
    if ((h.sz > l) && !_bufread(NULL, h.sz-l)) {
        CONSOLE("[%d]: skip empty", ftell(fstr));
        return LSFAIL;
    }
    
    return static_cast<ls_type_t>(h.type);
}

bool lsseek(size_t pos) {
    CONSOLE("pos: %d -> %d", ftell(fstr), pos);
    bc = 0;
    bl = 0;
    return fseek(fstr, pos, SEEK_SET) == 0;
}


/* ------------------------------------------------------------------------------------------- *
 *  временный файл
 * ------------------------------------------------------------------------------------------- */