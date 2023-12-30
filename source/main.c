#include <curl/curl.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h> // chdir
#include <dirent.h> // mkdir
#include <time.h>
#include <math.h>
#include <switch.h>

#include "download.h"
#include "unzip.h"
#include "util.h"


#define ROOT                    "/"
#define APP_PATH                "/switch/sigpatch-updater/"
#define APP_OUTPUT              "/switch/sigpatch-updater/sigpatch-updater.nro"
#define OLD_APP_PATH            "/switch/sigpatch-updater.nro"

#define APP_VERSION             "0.3.0"

#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))

typedef bool (*func_handler)(void);
struct Entry {
    const char* display_text;
    func_handler func;
    const char* description;
};

static PadState g_pad;

static bool progressCallback(u32 dltotal, u32 dlnow, u32 ultotal, u32 ulnow) {
    if (!stillRunning()) {
        return false;
    }

    padUpdate(&g_pad);
    const u64 kDown = padGetButtonsDown(&g_pad);
    if (kDown & HidNpadButton_B) {
        return false;;
    }

    if (dltotal > 0) {
        struct timeval tv = {0};
        gettimeofday(&tv, NULL);
        const int counter = round(tv.tv_usec / 100000);

        if (counter == 0 || counter == 2 || counter == 4 || counter == 6 || counter == 8) {
            const double mb = 1024*1024;
            consolePrint("* DOWNLOADING: %.2fMB of %.2fMB *\r", (double)dlnow / mb, (double)dltotal / mb);
        }
    }

    return true;
}

static bool update_sigpatches_handler(void) {
    if (downloadFile(AMS_SIG_URL, TEMP_FILE, progressCallback)) {
        if (!unzip(TEMP_FILE)) {
            return false;
        }
        consolePrint("\nfinished!\n\nRemember to reboot for the patches to be loaded!\n");
        return true;
    } else {
        return false;
    }
}

static bool update_syspatch_handler(void) {
    if (downloadFile(SYS_PATCH_URL, TEMP_FILE, progressCallback)) {
        if (!unzip(TEMP_FILE)) {
            return false;
        }

        u64 pid = 0;
        const NcmProgramLocation location = {
            .program_id = 0x420000000000000BULL,
            .storageID = NcmStorageId_None,
        };

        if (R_FAILED(pmshellLaunchProgram(0, &location, &pid))) {
            consolePrint(
                "\nFailed to start sys-patch!\n"
                "A reboot is needed...\n"
                "Report this issue on GitHub please!\n");
            return false;
        } else {
            consolePrint("\nsys-patch ran successfully, patches should be applied!\n");
            return true;
        }
    } else {
        return false;
    }
}

static bool update_app_handler(void) {
    if (downloadFile(APP_URL, TEMP_FILE, progressCallback)) {
        remove(APP_OUTPUT);
        rename(TEMP_FILE, APP_OUTPUT);
        remove(OLD_APP_PATH);
        consolePrint("\nApp updated!\nRestart app to take effect");
        return true;
    } else {
        return false;
    }
}

static const struct Entry ENTRIES[] = {
    { "Update sigpatches", update_sigpatches_handler, "sigpatches require a reboot" },
    { "Update sys-patch", update_syspatch_handler, "sys-patch applies patches without reboot" },
    { "Update app      ", update_app_handler, "requires restarting the app" },
};

enum WifiState {
    WifiState_AIRPLANE,
    WifiState_0, // no signal
    WifiState_1, // low strength
    WifiState_2, // medium strength
    WifiState_3, // max strength
};

static enum WifiState get_wifi_state() {
    Result rc = 0;
    NifmInternetConnectionType type = {0};
    NifmInternetConnectionStatus status = {0};
    u32 strength = 0;

    if (R_FAILED(rc = nifmGetInternetConnectionStatus(&type, &strength, &status))) {
        return WifiState_AIRPLANE;
    }

    if (type == NifmInternetConnectionType_Ethernet) {
        if (status != NifmInternetConnectionStatus_Connected) {
            return WifiState_0;
        } else {
            return WifiState_3;
        }
    }

    switch (strength) {
        case 0: return WifiState_0;
        case 2: return WifiState_2;
        case 3: return WifiState_3;
        default: return WifiState_1;
    }
}

static void refreshScreen(int cursor) {
    consoleClear();

    printf("\x1B[36mSigpatch-Updater: v%s\x1B[37m\n\n\n", APP_VERSION);
    // printf("This app is unmaintained, please consider using a different tool to update your patches!\n\n\n");
    printf("Press (A) to select option\n\n");
    printf("Press (+) to exit\n\n\n");

    for (int i = 0; i < ARRAY_SIZE(ENTRIES); i++) {
        printf("[%c] = %s\t\t(%s)\n\n", cursor == i ? 'X' : ' ', ENTRIES[i].display_text, ENTRIES[i].description);
    }

    consoleUpdate(NULL);
}

// update the cursor so that it wraps around
static void update_cursor(int* cur, int new_value, int max) {
    if (new_value >= max) {
        new_value = 0;
    } else if (new_value < 0) {
        new_value = max - 1;
    }

    *cur = new_value;
    refreshScreen(new_value);
}

// where the app starts
int main(int argc, char **argv) {
    curl_global_init(CURL_GLOBAL_ALL);

    // init stuff
    mkdir(APP_PATH, 0777);

    // move nro to app folder
    if (!strstr(argv[0], APP_OUTPUT)) {
        remove(APP_OUTPUT);
        rename(argv[0], APP_OUTPUT);
    }

    // change directory to root (defaults to /switch/)
    chdir(ROOT);

    // set the cursor position to 0
    int cursor = 0;

    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    padInitializeDefault(&g_pad);

    // main menu
    refreshScreen(cursor);

    // muh loooooop
    while (stillRunning()) {
        padUpdate(&g_pad);
        const u64 kDown = padGetButtonsDown(&g_pad);

        // move cursor down...
        if (kDown & HidNpadButton_AnyDown) {
            update_cursor(&cursor, cursor + 1, ARRAY_SIZE(ENTRIES));
        }

        // move cursor up...
        else if (kDown & HidNpadButton_AnyUp) {
            update_cursor(&cursor, cursor - 1, ARRAY_SIZE(ENTRIES));
        }

        else if (kDown & HidNpadButton_A) {
            if (get_wifi_state() < WifiState_1) {
                consolePrint(
                    "\n\n[Error] not connected to the internet!\n\n"
                    "An internet connection is required to download updates!\n");
            }
            else {
                if (!ENTRIES[cursor].func()) {
                    consolePrint("Failed to %s\n", ENTRIES[cursor].display_text);
                }
            }

            consolePrint("\nPress (A) to continue...\n");

            while (stillRunning()) {
                padUpdate(&g_pad);
                const u64 kDown = padGetButtonsDown(&g_pad);
                if (kDown & HidNpadButton_A) {
                    break;
                }
                svcSleepThread(1e+9 / 60);
            }

            refreshScreen(cursor);
        }

        // exit...
        else if (kDown & HidNpadButton_Plus) {
            break;
        }

        // 1e+9 = 1 second
        svcSleepThread(1e+9 / 60);
    }

    curl_global_cleanup();
    return 0;
}

// this is called before main
void userAppInit(void) {
    appletLockExit();
    pmshellInitialize();
    consoleInit(NULL);
    socketInitializeDefault();
    nifmInitialize(NifmServiceType_User);
}

// this is called after main exits
void userAppExit(void) {
    pmshellExit();
    nifmExit();
    socketExit();
    consoleExit(NULL);
    appletUnlockExit();
}
