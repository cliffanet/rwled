/*
    Worker
*/

#include "worker.h"
#include "clock.h"
#include "log.h" // временно для отладки

#include <map>
#include <string.h> // strncpy

////////////////////////////////////////////////////////////////////////////////

typedef std::map<Wrk::key_t, Wrk::run_t> worker2_list_t;

static worker2_list_t wrkall;

bool _wrkAdd(Wrk::run_t ws, const char *tname_P) {
    if (!ws || (ws.get() == 0))
        return false;

    wrkall[ws.get()] = ws;
    ws->optset(Wrk::O_INLIST);
#ifdef FWVER_DEBUG
    char tname[128];
    strncpy_P(tname, tname_P, sizeof(tname));
    tname[sizeof(tname)-1]='\0';
    CONSOLE("Wrk(0x%08x) begin: %s", ws.get(), tname);
#endif

    return true;
}

////////////////////////////////////////////////////////////////////////////////

void wrkProcess(uint32_t tmmax)
{
    if (wrkall.size() == 0)
        return;
    
    for (auto &it : wrkall) {
        // сбрасываем флаг needwait
        if (it.second->opt(Wrk::O_NEEDWAIT))
            it.second->optdel(Wrk::O_NEEDWAIT);
        // timer
        it.second->timer();
    }
    
    auto beg = tmill();
    bool run = true;
    
    while (run) {
        run = false;
        for(auto it = wrkall.begin(), itnxt = it; it != wrkall.end(); it = itnxt) {
            itnxt++; // такие сложности, чтобы проще было удалить текущий элемент
            auto &ws = it->second;

            if ( ws->opt(Wrk::O_NEEDFINISH) ) {
                if (!ws->opt(Wrk::O_FINISHED))
                    ws->end();

                ws->optclr(Wrk::O_FINISHED);
                CONSOLE("Wrk(0x%08x) finished", it->first);
                wrkall.erase(it);
            }
            else
            if ( ! ws->opt(Wrk::O_NEEDWAIT) )
                switch ( ws->run() ) {
                    case Wrk::DLY:
                        ws->optset(Wrk::O_NEEDWAIT);
                        break;

                    case Wrk::RUN:
                        run = true;
                        break;

                    case Wrk::END:
                        ws->optset(Wrk::O_NEEDFINISH);
                        run = true;
                        break;
                }
        }
        
        if ((tmill()-beg) >= tmmax)
            return;
    }
}

bool wrkEmpty() {
    return wrkall.empty();
}
