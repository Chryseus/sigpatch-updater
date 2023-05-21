#ifndef _DOWNLOAD_H_
#define _DOWNLOAD_H_

#define AMS_SIG_URL     "https://sigmapatches.coomer.party/sigpatches.zip?latest"
#define APP_URL         "https://github.com/ITotalJustice/sigpatch-updater/releases/latest/download/sigpatch-updater.nro"
#define SYS_PATCH_URL   "https://github.com/ITotalJustice/sys-patch/releases/latest/download/sys-patch.zip"
#define TEMP_FILE       "/switch/sigpatch-updater/temp"

#define ON              1
#define OFF             0


#include <stdbool.h>

//
bool downloadFile(const char *url, const char *output, int api);

#endif
