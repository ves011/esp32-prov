/*
 * nvs_editor.c
 *
 *  Created on: Feb 28, 2026
 *      Author: viorel_serbu
 */

//

#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_system.h>
#include <nvs_flash.h>
#include <sys/param.h>
#include <esp_log.h>
#include <spi_flash_mmap.h>
#include "esp_err.h"
#include <esp_http_server.h>
#include <nvs.h>
#include "nvs_editor.h"
#include "nvsop.h"
#include "part_editor.h"
#include "esp_partition.h"
#include "html_builder.h"
#include "file_server.h"

static const char *TAG = "NVSEDITOR";
#define BUFSIZE			1024

static void insert_ns_row(httpd_req_t *req, int ns_idx);
static void insert_key_row(httpd_req_t *req, keydef_t *kd);
static void insert_input_field(httpd_req_t *req, keydef_t *kd);
//int get_nvs_entries(char *pName);
int get_key_val(keydef_t *kd)
	{
	switch(kd->type)
		{
		case NVS_TYPE_U8:
			strcpy(kd->typestr, "NVS_TYPE_U8");
			kd->len = 1;
			if(nvs_get_u8(kd->nvsh, kd->name, &kd->val.u8) == ESP_OK)
				sprintf(kd->valstr, "%u", kd->val.u8);
			else
				strcpy(kd->valstr, "NaN");
			break;
		case NVS_TYPE_I8:
			kd->len = 1;
    		strcpy(kd->typestr, "NVS_TYPE_I8");
			if(nvs_get_i8(kd->nvsh, kd->name, &kd->val.i8) == ESP_OK)
				sprintf(kd->valstr, "%d", kd->val.i8);
			else
				strcpy(kd->valstr, "NaN");
    		break;
    	case NVS_TYPE_U16:
    		kd->len = 2;
    		strcpy(kd->typestr, "NVS_TYPE_U16");
			if(nvs_get_u16(kd->nvsh, kd->name, &kd->val.u16) == ESP_OK)
				sprintf(kd->valstr, "%u", kd->val.u16);
			else
				strcpy(kd->valstr, "NaN");
    		break;
    	case NVS_TYPE_I16:
    		kd->len = 2;
    		strcpy(kd->typestr, "NVS_TYPE_I16");
			if(nvs_get_i16(kd->nvsh, kd->name, &kd->val.i16) == ESP_OK)
				sprintf(kd->valstr, "%d", kd->val.i16);
			else
				strcpy(kd->valstr, "NaN");
    		break;
    	case NVS_TYPE_U32:
    		kd->len = 4;
    		strcpy(kd->typestr, "NVS_TYPE_U32");
			if(nvs_get_u32(kd->nvsh, kd->name, &kd->val.u32) == ESP_OK)
				sprintf(kd->valstr, "%u", (unsigned int)kd->val.u32);
			else
				strcpy(kd->valstr, "NaN");
    		break;
    	case NVS_TYPE_I32:
    		kd->len = 4;
    		strcpy(kd->typestr, "NVS_TYPE_I32");
			if(nvs_get_i32(kd->nvsh, kd->name, &kd->val.i32) == ESP_OK)
				sprintf(kd->valstr, "%d", (int)kd->val.i32);
			else
				strcpy(kd->valstr, "NaN");
    		break;
    	case NVS_TYPE_U64:
    		kd->len = 8;
    		strcpy(kd->typestr, "NVS_TYPE_U64");
			if(nvs_get_u64(kd->nvsh, kd->name, &kd->val.u64) == ESP_OK)
				sprintf(kd->valstr, "%llu", kd->val.u64);
			else
				strcpy(kd->valstr, "NaN");
    		break;
    	case NVS_TYPE_I64:
    		kd->len = 8;
    		strcpy(kd->typestr, "NVS_TYPE_I64");
			if(nvs_get_i64(kd->nvsh, kd->name, &kd->val.i64) == ESP_OK)
				sprintf(kd->valstr, "%lld", kd->val.i64);
			else
				strcpy(kd->valstr, "NaN");
    		break;
    	case NVS_TYPE_STR:
    		strcpy(kd->typestr, "NVS_TYPE_STR");
    		kd->valstr[0] = 0;
    		if(nvs_get_str(kd->nvsh, kd->name, NULL, &kd->len) != ESP_OK)
    			kd->len = 0;
    		break;
		case NVS_TYPE_BLOB:
    		strcpy(kd->typestr, "NVS_TYPE_BLOB");
    		kd->valstr[0] = 0;
    		if(nvs_get_blob(kd->nvsh, kd->name, NULL, &kd->len) != ESP_OK)
				kd->len = 0;
			break;
		}
	return ESP_OK;
	}
esp_err_t nvs_get_handler(const uint8_t *start, const uint8_t *end, httpd_req_t *req)
	{
	char *buf, pn[32];
	char *pchar, *last_pchar, *tchar;
	nvs_stats_t nvs_stats;
	nvs_handle_t nvsh;
	int i, j, ret;
	keydef_t keydef;
	//extern char nvs_page_start[] asm("_binary_nvseditor_html_start");
	
    ESP_LOGI(TAG, "uri: %s", req->uri);

	tchar = strchr(req->uri, '?');
	if(tchar && strlen(tchar + 1) < sizeof(pn))
		strcpy(pn, tchar + 1);
	else
		{
		httpd_resp_send_chunk(req, "missing partition name</h3></div></body></html>", HTTPD_RESP_USE_STRLEN);
		httpd_resp_send_chunk(req, NULL, 0);
        return ESP_OK;
		}
			
    last_pchar = (char *)start;
    pchar = strstr(last_pchar, "partInfo");
    if(!pchar)
    	{
		httpd_resp_send_chunk(req, "Wrong page format</h3></div></body></html>", HTTPD_RESP_USE_STRLEN);
		httpd_resp_send_chunk(req, NULL, 0);
        return ESP_OK;
		}
    httpd_resp_send_chunk(req, last_pchar, pchar - last_pchar);
    last_pchar = pchar + strlen("partInfo");
    
    const esp_partition_t *np = NULL;
	esp_partition_iterator_t pit = esp_partition_find(ESP_PARTITION_TYPE_ANY, ESP_PARTITION_SUBTYPE_ANY, NULL);
    while(pit)
    	{
		np = esp_partition_get(pit);
    	if(np && strcmp(np->label, pn) == 0 && np->subtype == ESP_PARTITION_SUBTYPE_DATA_NVS)
    		{
			break;
			}
		pit = esp_partition_next(pit);
		}
	esp_partition_iterator_release(pit);
	if(!np)
		{
		char b[100];
		sprintf(b, "partition \"%s\" not found or not NVS type</h3></div></body></html>", pn);
		httpd_resp_send_chunk(req, b, strlen(b));
		httpd_resp_send_chunk(req, NULL, 0);
        return ESP_OK;
		}
	
	buf = malloc(BUFSIZE);
	if(!buf)
    	{
		httpd_resp_send_chunk(req, "Not enough memory to allocate work buffer</h3></div></body></html>", HTTPD_RESP_USE_STRLEN);
		httpd_resp_send_chunk(req, NULL, 0);
        return ESP_OK;
		}
	nvs_get_stats(np->label, &nvs_stats);
	get_nvs_entries(pn);

	if(pchar) // insert rows in partinfo table
    	{
		snprintf(buf, BUFSIZE, "<table><tr style=\"font-size: 22px;\"><td colspan=\"2\">Partition name:</td><td>%s</td></tr>", np->label);
		httpd_resp_send_chunk(req, buf, strlen(buf));
		
		snprintf(buf, BUFSIZE, "<tr><td style=\"width: 20px;\"> </td><td>size:</td><td style=\"text-align: right;\">%u (0X%X)</td></tr>", (unsigned int)np->size, (unsigned int)np->size);
		httpd_resp_send_chunk(req, buf, strlen(buf));
		
		snprintf(buf, BUFSIZE, "<tr><td></td><td>namespaces:</td><td style=\"text-align: right;\">%d</td></tr>", nvs_stats.namespace_count);
		httpd_resp_send_chunk(req, buf, strlen(buf));
		
		snprintf(buf, BUFSIZE, "<tr><td></td><td>total entries:</td><td style=\"text-align: right;\">%d</td></tr>", nvs_stats.total_entries);
		httpd_resp_send_chunk(req, buf, strlen(buf));
		
		snprintf(buf, BUFSIZE, "<tr><td></td><td>free entries:</td><td style=\"text-align: right;\">%d</td></tr>", nvs_stats.free_entries);
		httpd_resp_send_chunk(req, buf, strlen(buf));
		
		snprintf(buf, BUFSIZE, "<tr><td></td><td>available entries:</td><td style=\"text-align: right;\">%d</td></tr>", nvs_stats.available_entries);
		httpd_resp_send_chunk(req, buf, strlen(buf));
		
		snprintf(buf, BUFSIZE, "<tr><td></td><td>used entries:</td><td style=\"text-align: right;\">%d</td></tr></table>", nvs_stats.used_entries);
		httpd_resp_send_chunk(req, buf, strlen(buf));
		}
//insert namespaces in the dropdown list		
	pchar = strstr(last_pchar, "partInfoend");
	if(pchar)
		last_pchar = pchar + strlen("partInfoend");
	pchar = strstr(last_pchar, "<option value=\"nsoptions\">");
	if(pchar)
		{
		httpd_resp_send_chunk(req, last_pchar, pchar - last_pchar);
    	last_pchar = pchar + strlen("<option value=\"nsoptions\">");
    	for(i = 0; i < nns; i++)
    		{
			snprintf(buf, BUFSIZE, "<option value=\"%s\">", namespace[i].name);
			httpd_resp_send_chunk(req, buf, strlen(buf));
			}
		}
	pchar = strstr(last_pchar, "insertkeys");
	if(pchar)
		{
		httpd_resp_send_chunk(req, last_pchar, pchar - last_pchar);
    	last_pchar = pchar + strlen("insertkeys");
// insert namespaces and keys
		strcpy(buf, 
"<table class=\"fixed\">\
<tr>\n\
	<th style=\"width: 40px; text-align: center; font-size: 40px;\"></th>\n\
	<th style=\"width: 140px;\">name</th>\n\
	<th style=\"width: 140px;\">type</th>\n\
	<th style=\"width: 80px;\">length(B)</th>\n\
	<th style=\"width: 680px;\">value&nbsp;&nbsp;<button id = \"commit_ch\" type=\"button\" disabled onclick=\"commitc()\">Commit changes</button></th>\n\
    <th style=\"width: 80px;\"><button style=\"width: 80px;\" type=\"button\" onclick=\"delsel()\">Erase sel</button></th>\n\
</tr>\n");		
		httpd_resp_send_chunk(req, buf, strlen(buf));
		for(i = 0; i < nns; i++)
			{
			insert_ns_row(req, i);
        	ret = nvs_open_from_partition(pn, namespace[i].name, NVS_READONLY, &nvsh);
        	if(ret == ESP_OK)
        		{
	        	for(j = 0; j < nkeys && ret == ESP_OK; j++)
	        		{
					if(nvskey[j].ns_idx == i)
						{
						keydef.ns_idx = i;
						keydef.key_idx = j;
						keydef.nvsh = nvsh;
						keydef.type = nvskey[j].type;
						strcpy(keydef.name, nvskey[j].name);
						get_key_val(&keydef);
						nvskey[j].size = keydef.len;
						insert_key_row(req, &keydef);
						}
					}
				nvs_close(nvsh);
				}
			}
		}
	else
		{
		ESP_LOGI(TAG, "partInfo section not found");
		}
	strcpy(buf, "</table></div></body></html>");
	httpd_resp_send_chunk(req, buf, strlen(buf));
    //httpd_resp_send_chunk(req, last_pchar, nvs_page_size);
    //end of page
	httpd_resp_send_chunk(req, NULL, 0);
	free(buf);
    return ESP_OK;
	}

static void insert_ns_row(httpd_req_t *req, int ns_idx)
	{
	char buf[BUFSIZE], esc_name[100];
	html_escape_str(esc_name, sizeof(esc_name), namespace[ns_idx].name);
	snprintf(buf, BUFSIZE, 
"<tr class=\"strong\"><td class=\"shsign\"><a id = \"ns_%d\" style=\"text-decoration: none;\" href=\"#\" onclick=\"showhide(this.id)\">+</a></td>\n \
<td colspan=\"4\"><b>%s</b></td>" \
"<td><label> &nbsp; &nbsp; &nbsp; &nbsp;</label><input class=\"sel2delns\" type=\"checkbox\" name=\"ns_%d\"></td></tr>\n",
			ns_idx, esc_name, ns_idx);
 	httpd_resp_send_chunk(req, buf, strlen(buf));
	}
static void insert_key_row(httpd_req_t *req, keydef_t *kd)
	{
	char buf[BUFSIZE];
	char esc_name[100];
	html_escape_str(esc_name, sizeof(esc_name), kd->name);
	snprintf(buf, BUFSIZE, "<tr class=\"ns_%d\" style=\"display: none;\"><td></td>", kd->ns_idx);
	httpd_resp_send_chunk(req, buf, strlen(buf));
	//key name + key type					
	snprintf(buf, BUFSIZE, "<td id=\"kn_%d_%d\">%s</td><td id=\"t_%d_%d\" name=\"%d\">%s</td>", kd->ns_idx, kd->key_idx, esc_name, kd->ns_idx, kd->key_idx, kd->type, kd->typestr);
	httpd_resp_send_chunk(req, buf, strlen(buf));
	//length					
	snprintf(buf, BUFSIZE, "<td id=\"l_%d_%d\">%d</td>", kd->ns_idx, kd->key_idx, kd->len);
	httpd_resp_send_chunk(req, buf, strlen(buf));
	insert_input_field(req, kd);
	// key checkbox
	snprintf(buf, BUFSIZE, 
	"<td><label> &nbsp; &nbsp; &nbsp; &nbsp;</label><input class=\"sel2del\" type=\"checkbox\" name=\"ns_%d\" id=\"sd_%d_%d\"></td></tr>\n", 
		kd->ns_idx, kd->ns_idx, kd->key_idx);
	httpd_resp_send_chunk(req, buf, strlen(buf));
	}
	
static void insert_input_field(httpd_req_t *req, keydef_t *kd)
	{
	char buf[BUFSIZE];
	int ret;
	char *bstring = server_data.scratch;
	if(kd->type < NVS_TYPE_STR)
		{
		if(kd->type & 0x10)
			snprintf(buf, BUFSIZE, "<td><input id = \"%d_%d\" class = \"ied\" type=\"text\" value=\"%s\"></td>", kd->ns_idx, kd->key_idx, kd->valstr);
		else
			snprintf(buf, BUFSIZE, "<td><input id = \"%d_%d\" class = \"ued\" type=\"text\" value=\"%s\"></td>", kd->ns_idx, kd->key_idx, kd->valstr);
		httpd_resp_send_chunk(req, buf, strlen(buf));
		}
	else if(kd->type == NVS_TYPE_STR)
		{
		snprintf(buf, BUFSIZE, "<td><textarea id=\"%d_%d\" class=\"sed\" style=\"width: 675px;resize: vertical;\" rows=\"1\">", kd->ns_idx, kd->key_idx);
		httpd_resp_send_chunk(req, buf, strlen(buf));
		//sprintf(bstring, "<td><textarea id=\"[%d][%d]\" style=\"width: 675px;resize: vertical;\" rows=\"1\">", i, j);
		if(nvs_get_str(kd->nvsh, kd->name, bstring, &kd->len) == ESP_OK)
			{
			buf[0] = 0;
			int pos = 0;
			char eb[10];
			while(pos < kd->len)
				{
				int len = strlcat(buf, html_escape_chr(bstring[pos++], eb), BUFSIZE);
				if(len > BUFSIZE - 50)
					{
					httpd_resp_send_chunk(req, buf, strlen(buf));
					buf[0] = 0;
					}
				}
			if(buf[0])			
				httpd_resp_send_chunk(req, buf, strlen(buf));
			strcpy(buf, "</textarea></td>");
			httpd_resp_send_chunk(req, buf, strlen(buf));
			}
		else
			{
			strcpy(bstring, "Error retrieving kay value</textarea></td>");
			httpd_resp_send_chunk(req, bstring, strlen(bstring));
			}
		}
	else if(kd->type == NVS_TYPE_BLOB)
		{
		if(kd->len > MAX_BLOB_SIZE)
			{
			snprintf(buf, BUFSIZE, "<td><div class=\"large-blob\"><b>!!! Blob size > %d !!!</b></div><br> \
<button id=\"dump\" onclick=\"dump(\'%d_%d\')\">Dump to...</button>&nbsp;&nbsp;&nbsp;<br>&nbsp;</td>", 
				MAX_BLOB_SIZE, kd->ns_idx, kd->key_idx);
			httpd_resp_send_chunk(req, buf, strlen(buf));
			}
		else
			{
			ret = nvs_get_blob(kd->nvsh, kd->name, bstring, &kd->len);
			if(ret != ESP_OK)
				{
				strcpy(buf, "<td>Error retrieving BLOB from NVS<br>");
				httpd_resp_send_chunk(req, buf, strlen(buf));
				}
			else
				{
				int is = 0;
				char bhex[6];
				snprintf(buf, BUFSIZE, "<td><textarea id=\"%d_%d\" class=\"hed\" style=\"width: 675px;resize: vertical;\" rows=\"1\">", kd->ns_idx, kd->key_idx);
				httpd_resp_send_chunk(req, buf, strlen(buf));
				buf[0] = 0;
				while(is < kd->len)
					{
					sprintf(bhex, "%02x ", bstring[is]);
					strcat(buf,bhex);
					is++;
					if(is % 8 == 0 && is % 16)
						strcat(buf, "- ");
					
					if(is % 16 == 0)
						{
						strcat(buf, "\n");
						httpd_resp_send_chunk(req, buf, strlen(buf));
						buf[0] = 0;
						}
					}
				strcat(buf, "</textarea><br>");
				httpd_resp_send_chunk(req, buf, strlen(buf));
				snprintf(buf, BUFSIZE,"\
<button id=\"dump\" onclick=\"dump(\'%d_%d\')\">Dump to...</button>&nbsp;&nbsp;&nbsp;\
<button id=\"upload\" type=\"button\" onclick=\"document.getElementById('f_%d_%d').click()\">Upload from file</button>&nbsp;\
<label id=\"u_%d_%d\"></label>\
<input id=\"f_%d_%d\" type=\"file\" onchange=\"loadf('%d_%d', event)\" style=\"display: none;\"><br>&nbsp;</td>", 
	kd->ns_idx, kd->key_idx, kd->ns_idx, kd->key_idx, kd->ns_idx, kd->key_idx, kd->ns_idx, kd->key_idx, kd->ns_idx, kd->key_idx);
				httpd_resp_send_chunk(req, buf, strlen(buf));
				}
			}				
		}
	}
	
int nvskey_get_handler(const uint8_t *start, const uint8_t *end, httpd_req_t *req)
	{
    char *buf, berr[60], bmsg[120];
    int ret = ESP_FAIL, idxn, idxk;
    nvs_handle_t nvsh;
	sscanf(req->uri + strlen(NVSK_DOWNLOAD), "%d_%d", &idxn, &idxk);
	ESP_LOGI(TAG, "uri: %s /msg: %s / idxn: %d / idxk: %d", req->uri, bmsg, idxn, idxk);
	if(idxk < nkeys && nvskey[idxk].type == NVS_TYPE_BLOB)
		{
		buf = calloc(nvskey[idxk].size, 1);
		if(buf)
			{
			ret = nvs_open_from_partition(nvs_selpart, namespace[idxn].name, NVS_READONLY, &nvsh);
			if(ret == ESP_OK)
				{
				ret = nvs_get_blob(nvsh, nvskey[idxk].name, buf, &nvskey[idxk].size);
				if(ret == ESP_OK)
					{
					ret = httpd_resp_send_chunk(req, buf, nvskey[idxk].size);
					if(ret == ESP_OK)
						ESP_LOGI(TAG, "key dump complete");
					httpd_resp_send_chunk(req, NULL, 0);
					}
				}
			}
		else
			strcpy(berr, "cannot allocate memory to read BLOB data");
		}
	else
		sprintf(berr,  "invalid key index or key type not BLOB (%d)", idxk);

	if(ret == ESP_FAIL)
		ESP_LOGI(TAG, "%s", berr);
	else
		strcpy(berr, esp_err_to_name(ret));
	
	ESP_LOGI(TAG, "dump key: %s", berr);
	return ret;	
	}
