/*
 * part_editor.h
 *
 *  Created on: Feb 18, 2026
 *      Author: viorel_serbu
 */

#ifndef PART_EDITOR_H_
#define PART_EDITOR_H_

#include "esp_http_server.h"
#include "esp_vfs.h"

#define FILE_PATH_MAX (ESP_VFS_PATH_MAX + CONFIG_SPIFFS_OBJ_NAME_LEN)
#define INSERTPARTITIONS	"<tr><td>insPart</td></tr>"
#define INSERTOPTPART		"<option value=\"1\">insertOptions</option>"
#define MAX_UPDPART			20
//#define MAX_BLOB_SIZE		SCRATCH_BUFSIZE

typedef struct
	{
	int boot;
	int run;
	char name[20];
	uint32_t address;
	uint32_t size;
	} ptable_t;

extern int npart;
extern ptable_t pTable[MAX_UPDPART];

esp_err_t part_get_handler(const uint8_t *start, const uint8_t *end, httpd_req_t *req);
esp_err_t part_post_handler(httpd_req_t *req);
esp_err_t part_update_handler(const uint8_t *start, const uint8_t *end, httpd_req_t *req);
esp_err_t set_boot_handler(httpd_req_t *req);
esp_err_t flashing_post_handler(const uint8_t *start, const uint8_t *end, httpd_req_t *req);
esp_err_t dump_get_handler(const uint8_t *start, const uint8_t *end, httpd_req_t *req);
int set_bp(char *pName);

#endif /* PART_EDITOR_H */
