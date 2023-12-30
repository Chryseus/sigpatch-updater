#include <switch.h>
#include <stdarg.h>
#include <stdio.h>

#include "util.h"

void consolePrint(const char* s, ...) {
    va_list v;
    va_start(v, s);
    vprintf(s, v);
    consoleUpdate(NULL);
    va_end(v);
}

bool stillRunning(void) {
    static Mutex mutex = {0};
    static bool running = true;
    mutexLock(&mutex);
    if (!appletMainLoop()) {
        running = false;
    }
    const bool result = running;
    mutexUnlock(&mutex);
    return result;
}
