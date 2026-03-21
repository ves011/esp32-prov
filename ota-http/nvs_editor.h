/*
 * nvs_editor.h
 *
 *  Created on: Feb 28, 2026
 *      Author: viorel_serbu
 */

#ifndef MAIN_NVS_EDITOR_H_
#define MAIN_NVS_EDITOR_H_

#include <esp_http_server.h>
#include <nvs.h>

typedef struct
	{
	nvs_handle_t nvsh;
	int type;
	char name[NVS_KEY_NAME_MAX_SIZE];
	char typestr[20];
	size_t len;
	union
		{
		int8_t i8;
		uint8_t u8;
		int16_t i16;
		uint16_t u16;
		int32_t i32;
		uint32_t u32;
		int64_t i64;
		uint64_t u64;
		} val;
	char valstr[24];
	} keydef_t;
	

esp_err_t nvs_get_handler(httpd_req_t *req);
int get_nvs_entries(char *pName);

#endif /* MAIN_NVS_EDITOR_H_ */
