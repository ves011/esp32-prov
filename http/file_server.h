
#ifndef FILE_SERVER_H_
#define FILE_SERVER_H_

#include "esp_http_server.h"
#include "esp_vfs.h"

#define SCRATCH_BUFSIZE  	8192
#define MAX_BLOB_SIZE		SCRATCH_BUFSIZE

#define NVSK_DOWNLOAD		"/keydump/"
#define PART_DOWNLOAD		"/download/"
#define PART_UPLOAD			"/upload/"

struct file_server_data {
    /* Base path of file storage */
    char base_path[ESP_VFS_PATH_MAX + 1];

    /* Scratch buffer for temporary storage during file transfer */
    char scratch[SCRATCH_BUFSIZE];
};

typedef int (*uri_handler_t)(const uint8_t *start, const uint8_t *end, httpd_req_t *req);

typedef struct {
    const char *uri;
    const uint8_t *start;
    const uint8_t *end;
    const char *type;
    uri_handler_t page_handler;
} asset_t;

extern struct file_server_data server_data;
extern httpd_handle_t w_server;
extern int wsfd;

esp_err_t start_file_server(const char *base_path);
esp_err_t ws_handler(httpd_req_t *req);

#endif // FILE_SERVER_H_
