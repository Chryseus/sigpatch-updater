#ifndef _UTIL_H_
#define _UTIL_H_

#include <stdbool.h>

void consolePrint(const char* s, ...) __attribute__ ((format (printf, 1, 2)));

bool stillRunning(void);

#endif
