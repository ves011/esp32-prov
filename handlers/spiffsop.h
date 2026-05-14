/*
 * spiffs_editor.h
 *
 *  Created on: Apr 11, 2026
 *      Author: viorel_serbu
 */

#ifndef HANDLERS_SPIFFSOP_H_
#define HANDLERS_SPIFFSOP_H_

#include <esp_http_server.h>

typedef struct
	{
	char *fname;
	int size;
	char fdate[26];
	}fentry_t;
		
esp_err_t spiffs_get_handler(httpd_req_t *req);
int list_files(char *pname, size_t *total, size_t * used);
int create_file(char * fname);


#endif /* HANDLERS_SPIFFSOP_H_ */
