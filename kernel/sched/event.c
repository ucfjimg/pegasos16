#include "taskp.h"

void ev_init(event_t *event)
{
    UNREFERENCED(event);
}

int ev_wait(int nev, event_t *evs[])
{
    UNREFERENCED(nev);
    UNREFERENCED(evs);
    return 0;
}

void ev_signal(event_t *event)
{
    UNREFERENCED(event);
}

int ev_signaled(event_t *event)
{
    UNREFERENCED(event);
    return 0;
}

