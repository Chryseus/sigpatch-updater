#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <curl/curl.h>
#include <switch.h>

#include "download.h"
#include "util.h"

#define API_AGENT           "ITotalJustice"
#define _1MiB   0x100000

typedef struct {
    char *memory;
    size_t size;
} MemoryStruct_t;

typedef struct {
    uint8_t *data;
    size_t data_size;
    uint64_t offset;
    FILE *out;
} ntwrk_struct_t;

static size_t WriteMemoryCallback(void *contents, size_t size, size_t num_files, void *userp) {
    ntwrk_struct_t *data_struct = (ntwrk_struct_t *)userp;
    const size_t realsize = size * num_files;

    if (realsize + data_struct->offset >= data_struct->data_size) {
        fwrite(data_struct->data, data_struct->offset, 1, data_struct->out);
        data_struct->offset = 0;
    }

    memcpy(&data_struct->data[data_struct->offset], contents, realsize);
    data_struct->offset += realsize;
    data_struct->data[data_struct->offset] = 0;
    return realsize;
}

static int download_progress(void *clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow) {
    const DlProgressCallback callback = (DlProgressCallback)clientp;
    if (!callback(dltotal, dlnow, ultotal, ulnow)) {
        return 1;
    }
    return 0;
}

bool downloadFile(const char *url, const char *output, DlProgressCallback pcall) {
    CURL *curl = curl_easy_init();
    if (curl) {
        FILE *fp = fopen(output, "wb");
        if (fp) {
            consolePrint("\nDownload in progress, Press (B) to cancel...\n\n");

            ntwrk_struct_t chunk = {0};
            chunk.data = malloc(_1MiB);
            chunk.data_size = _1MiB;
            chunk.out = fp;

            curl_easy_setopt(curl, CURLOPT_URL, url);
            curl_easy_setopt(curl, CURLOPT_USERAGENT, API_AGENT);
            curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
            curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
            curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);

            // write calls
            curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, &chunk);

            curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
            curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, download_progress);
            curl_easy_setopt(curl, CURLOPT_XFERINFODATA, pcall);

            CURLM *multi_handle = curl_multi_init();
            curl_multi_add_handle(multi_handle, curl);
            int still_running = 1;
            CURLMcode mc;

            while (still_running) {
                mc = curl_multi_perform(multi_handle, &still_running);

                if (!mc)
                    mc = curl_multi_poll(multi_handle, NULL, 0, 1000, NULL);

                if (mc) {
                    break;
                }
            }

            int msgs_left;
            CURLMsg *msg;
            CURLcode res = 1;

            while ((msg = curl_multi_info_read(multi_handle, &msgs_left))) {
                if (msg->msg == CURLMSG_DONE) {
                    res = msg->data.result;
                }
            }

            // write from mem to file
            if (chunk.offset) {
                fwrite(chunk.data, 1, chunk.offset, fp);
            }

            // clean
            curl_multi_remove_handle(multi_handle, curl);
            curl_easy_cleanup(curl);
            curl_multi_cleanup(multi_handle);
            free(chunk.data);
            fclose(chunk.out);

            if (res == CURLE_OK) {
                consolePrint("\n\ndownload complete!\n\n");
                return true;
            } else {
                consolePrint("\n\ncurl error: %s", curl_easy_strerror(res));
            }
        }
    }

    consolePrint("\n\ndownload failed...\n\n");
    return false;
}
