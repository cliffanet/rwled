
#include "console.h"
#include "worker.h"
#include "dataparser.h"
#include "log.h"

#include "txt.h"

//#include <Arduino.h> // ESP.getFreeHeap()
//#include <pgmspace.h>   // PSTR()

class _consoleReader : public Wrk {
    Stream &_fh;
    DataBufParser _prs;
    
    void clear() {
        _prs.clear();
    }

public:
    _consoleReader(Stream &fh) :
        _fh(fh)
    { }
#ifdef FWVER_DEBUG
    ~_consoleReader() {
        CONSOLE("_consoleReader(0x%08x) destroy", this);
    }
#endif

    state_t run() {
        auto sz = _fh.available();
        if (sz <= 0) return DLY;

        auto ok = _prs.read(sz, [this](char *buf, size_t sz) {
            size_t rsz = this->_fh.readBytes(buf, sz);
            return rsz;
        });

        if (!ok) return END;

        return _fh.available() > 0 ? RUN : DLY;
    }
};

static WrkProc<_consoleReader> _reader;
void initConsoleReader(Stream &fh) {
    if (!_reader.isrun())
        _reader = wrkRun<_consoleReader>(fh);
}
