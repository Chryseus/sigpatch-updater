#include <curl/curl.h>
#include <stdio.h>
#include <stdarg.h>
#include <unistd.h> // chdir
#include <dirent.h> // mkdir
#include <switch.h>

#include "download.h"
#include "unzip.h"


#define ROOT                    "/"
#define APP_PATH                "/switch/sigpatch-updater/"
#define APP_OUTPUT              "/switch/sigpatch-updater/sigpatch-updater.nro"
#define OLD_APP_PATH            "/switch/sigpatch-updater.nro"

#define APP_VERSION             "0.2.0"

#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))

typedef bool (*func_handler)(void);
struct Entry {
    const char* display_text;
    func_handler func;
    const char* description;
};

static bool update_sigpatches_handler(void) {
    if (downloadFile(AMS_SIG_URL, TEMP_FILE, OFF)) {
        if (!unzip(TEMP_FILE)) {
            return false;
        }
        printf("\nfinished!\n\nRemember to reboot for the patches to be loaded!\n");
        consoleUpdate(NULL);
        return true;
    } else {
        return false;
    }
}

static bool update_syspatch_handler(void) {
    if (downloadFile(SYS_PATCH_URL, TEMP_FILE, OFF)) {
        if (!unzip(TEMP_FILE)) {
            return false;
        }

        u64 pid = 0;
        const NcmProgramLocation location = {
            .program_id = 0x420000000000000BULL,
            .storageID = NcmStorageId_None,
        };

        if (R_FAILED(pmshellLaunchProgram(0, &location, &pid))) {
            printf(
                "\nFailed to start sys-patch!\n"
                "A reboot is needed...\n"
                "Report this issue on GitHub please!\n");
            consoleUpdate(NULL);
            return false;
        } else {
            printf("\nsys-patch ran successfully, patches should be applied!\n");
            consoleUpdate(NULL);
            return true;
        }
    } else {
        return false;
    }
}

static bool update_app_handler(void) {
    if (downloadFile(APP_URL, TEMP_FILE, OFF)) {
        remove(APP_OUTPUT);
        rename(TEMP_FILE, APP_OUTPUT);
        remove(OLD_APP_PATH);
        printf("\nApp updated!\nRestart app to take effect");
        consoleUpdate(NULL);
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

static void printDisplay(const char *text, ...) {
    va_list v;
    va_start(v, text);
    vfprintf(stdout, text, v);
    va_end(v);
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

    // change directory to root (defaults to /switch/)
    chdir(ROOT);

    // set the cursor position to 0
    int cursor = 0;

    PadState pad = {0};
    padConfigureInput(1, HidNpadStyleSet_NpadStandard);
    padInitializeDefault(&pad);

    // main menu
    refreshScreen(cursor);

    // muh loooooop
    while (appletMainLoop()) {
        padUpdate(&pad);
        const u64 kDown = padGetButtonsDown(&pad);

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
                printDisplay(
                    "\n\n[Error] not connected to the internet!\n\n"
                    "An internet connection is required to download updates!\n");
            }
            else {
                if (!ENTRIES[cursor].func()) {
                    printDisplay("Failed to %s\n", ENTRIES[cursor].display_text);
                }
            }
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
