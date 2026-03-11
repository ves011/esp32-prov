/*
 * handlers.h
 *
 *  Created on: Feb 18, 2026
 *      Author: viorel_serbu
 */

#ifndef MAIN_HANDLERS_H_
#define MAIN_HANDLERS_H_

#include "esp_http_server.h"
#include "esp_vfs.h"

#define FILE_PATH_MAX (ESP_VFS_PATH_MAX + CONFIG_SPIFFS_OBJ_NAME_LEN)
#define INSERTPARTITIONS	"<tr><td>insPart</td></tr>"
#define INSERTOPTPART		"<option value=\"1\">insertOptions</option>"
#define SCRATCH_BUFSIZE  8192
#define MAX_UPDPART		20

typedef struct
	{
	int boot;
	int run;
	char name[20];
	uint32_t address;
	uint32_t size;
	} ptable_t;

struct file_server_data {
    /* Base path of file storage */
    char base_path[ESP_VFS_PATH_MAX + 1];

    /* Scratch buffer for temporary storage during file transfer */
    char scratch[SCRATCH_BUFSIZE];
};
extern int npart;
extern ptable_t pTable[MAX_UPDPART];
extern int wsfd;

esp_err_t root_get_handler(httpd_req_t *req);
esp_err_t main_post_handler(httpd_req_t *req);
esp_err_t root_update_handler(httpd_req_t *req);
esp_err_t set_boot_handler(httpd_req_t *req);
esp_err_t flashing_post_handler(httpd_req_t *req);
esp_err_t dump_get_handler(httpd_req_t *req);
esp_err_t ws_handler(httpd_req_t *req);
int set_bp(char *pName);

#endif /* MAIN_HANDLERS_H_ */
