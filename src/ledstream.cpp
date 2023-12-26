
#include "ledstream.h"
#include "log.h"

#include <stdlib.h>
#include <cstring>

#include "vfs_api.h"
#include <sys/unistd.h>
#include <sys/dirent.h>
#include "esp_err.h"
#include "esp_spiffs.h"

static FILE* fstr = NULL;
static const auto mnt = PSTR("");
static const auto nstr = PSTR("/strm.led");
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

    fstr = open_P(nstr);
    
    initok = true;

    const char dir[] = {'/', '\0'};
    listdir();
    
    size_t total = 0, used = 0;
    ESPDO(esp_spiffs_info(NULL, &total, &used), return false);
    CONSOLE("Partition size: total: %d, used: %d", total, used);
    
    return true;
}

bool lsformat() {
    if (fstr != NULL) {
        fclose(fstr);
        fstr = NULL;
    }
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

ls_info_t lsinfo() {
    size_t total = 0, used = 0;
    ESPDO(esp_spiffs_info(NULL, &total, &used), return { 0 });

    PFNAME(nstr);
    struct stat st;
    if (stat(fname, &st) != 0)
        st.st_size=-1;

    return { st.st_size, used, total };
}

/* ------------------------------------------------------------------------------------------- *
 *  основной файл потока
 * ------------------------------------------------------------------------------------------- */

bool lsopened() {
    return fstr != NULL;
}

static uint8_t buf[1100];
static uint16_t bc = 0, bl = 0;
bool _bufread(uint8_t *data, size_t sz) {
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
static FILE* ftmp = NULL;
static const auto ntmp = PSTR("/.tmp");

bool lstmp() {
    if (!initok) {
        CONSOLE("fs not inited");
        return false;
    }

    if (ftmp != NULL) {
        CONSOLE("ftmp already opened");
        return false;
    }
    
    PFNAME(ntmp);

    struct stat st;
    if (stat(fname, &st) == 0)
        unlink(fname);

    ftmp = open_P(fname, 'w');
    if (ftmp == NULL)
        CONSOLE("tmp file open fail");

    return ftmp != NULL;
}

bool lsadd(ls_type_t type, const uint8_t *data, size_t sz) {
    ls_rhead_t h;
    h.type  = type;
    h.sz    = sz;

    if (ftmp == NULL) {
        CONSOLE("lstmp not opened");
        return false;
    }

    auto sz1 = fwrite(&h, 1, sizeof(h), ftmp);
    if (sz1 != sizeof(h)) {
        CONSOLE("lstmp hdr-write fail: %d", sz1);
        return false;
    }

    if ((sz > 0) && ((sz1 = fwrite(data, 1, sz, ftmp)) != sz)) {
        CONSOLE("lstmp data-write fail: %d/%d", sz1, sz);
        return false;
    }

    return true;
}

int lsfindtm(uint32_t tm) {
    auto orig = ftmp != NULL ? ftell(ftmp) : 0;
    CONSOLE("ftmp pos: %d", orig);
    if (ftmp != NULL)
        fclose(ftmp);
    
    PFNAME(ntmp);
    ftmp = open_P(fname);
    if (ftmp == NULL) {
        CONSOLE("lstmp not opened for read");
        return -1;
    }

    int pos = -1;
    while (true) {
        auto p = ftell(ftmp);
        ls_rhead_t h;
        if (fread(&h, 1, sizeof(h), ftmp) != sizeof(h)) {
            CONSOLE("[%d]: hdr read", p);
            break;
        }
        if (h.m != LSHDR) {
            CONSOLE("[%d]: hdr fail", p);
            break;
        }
        if (h.type != LSTIME) {
            if (fseek(ftmp, h.sz, SEEK_CUR) != 0)
                break;
            continue;
        }
        if (h.sz < 4) {
            CONSOLE("[%d]: tm sz: %d", p, h.sz);
            break;
        }
        uint32_t tm1 = 0;
        if (fread(&tm1, 1, sizeof(tm1), ftmp) != sizeof(tm1))
            break;
        CONSOLE("find tm[%d]: %d", p, tm1);
        if (tm1 >= tm) {
            pos = p;
            break;
        }
    }

    fclose(ftmp);
    ftmp = open_P(fname, 'a');
    if (ftmp == NULL)
        CONSOLE("reopen fail: %s", fname);
    CONSOLE("seek aft reopen to orig: %d", orig);
    fseek(ftmp, orig, SEEK_SET);

    return pos;
}

bool lsfin() {
    if (ftmp != NULL) {
        fclose(ftmp);
        ftmp = NULL;
        CONSOLE("tmp closed");
    }

    if (fstr != NULL) {
        fclose(fstr);
        fstr = NULL;
    }

    char fsrc[32], fname[32];
    strncpy_P(fsrc, ntmp, sizeof(fsrc));
    strncpy_P(fname, nstr, sizeof(fname));
    
    struct stat st;
    if (stat(fname, &st) == 0)
        unlink(fname);
    if (rename(fsrc, fname) == 0)
        CONSOLE("renamed: %s -> %s", fsrc, fname);
    else {
        CONSOLE("rename fail: %s -> %s", fsrc, fname);
        return false;
    }

    fstr = open_P(nstr);

    return true;
}
