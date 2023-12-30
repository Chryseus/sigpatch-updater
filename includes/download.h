#ifndef _DOWNLOAD_H_
#define _DOWNLOAD_H_

#define AMS_SIG_URL     "https://sigmapatches.su/sigpatches.zip"
#define APP_URL         "https://github.com/ITotalJustice/sigpatch-updater/releases/latest/download/sigpatch-updater.nro"
#define SYS_PATCH_URL   "https://github.com/ITotalJustice/sys-patch/releases/latest/download/sys-patch.zip"
#define TEMP_FILE       "/switch/sigpatch-updater/temp"

#include <stdbool.h>
#include <switch.h>

typedef bool(*DlProgressCallback)(u32 dltotal, u32 dlnow, u32 ultotal, u32 ulnow);

//
bool downloadFile(const char *url, const char *output, DlProgressCallback pcall);

#endif
