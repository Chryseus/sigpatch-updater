#ifndef _DOWNLOAD_H_
#define _DOWNLOAD_H_

#define AMS_SIG_URL     "https://github.com/ITotalJustice/patches/releases/latest/download/SigPatches.zip"
#define APP_URL         "https://github.com/ITotalJustice/sigpatch-updater/releases/latest/download/sigpatch-updater.nro"
#define TEMP_FILE       "/switch/sigpatch-updater/temp"

#define ON              1
#define OFF             0


#include <stdbool.h>

//
bool downloadFile(const char *url, const char *output, int api);

#endif
