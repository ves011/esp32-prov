/*
 * nvs_editor.c
 *
 *  Created on: Feb 28, 2026
 *      Author: viorel_serbu
 */

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
//#include "freertos/idf_additions.h"
//#include "freertos/projdefs.h"
//#include "esp_ota_ops.h"
#include "esp_partition.h"
//#include "freertos/idf_additions.h"
//#include "freertos/projdefs.h"
#include "project_specific.h"
#include "common_defines.h"
#include "cmd_wifi.h"
//#include "utils.h"
//#include "keep_alive.h"
#include "ws_client_handler.h"
#include "nvsop.h"
#include "nvs_editor.h"

static const char *TAG = "NVSEDITOR";

int get_nvs_entries(char *pName);
int get_key_val(keydef_t *kd)
	{
	switch(kd->type)
		{
		case 0x01:
			strcpy(kd->typestr, "NVS_TYPE_U8");
			kd->len = 1;
			if(nvs_get_u8(kd->nvsh, kd->name, &kd->val.u8) == ESP_OK)
				sprintf(kd->valstr, "%u", kd->val.u8);
			else
				strcpy(kd->valstr, "NaN");
			break;
		case 0x011:
			kd->len = 1;
    		strcpy(kd->typestr, "NVS_TYPE_I8");
			if(nvs_get_i8(kd->nvsh, kd->name, &kd->val.i8) == ESP_OK)
				sprintf(kd->valstr, "%d", kd->val.i8);
			else
				strcpy(kd->valstr, "NaN");
    		break;
    	case 0x02:
    		kd->len = 2;
    		strcpy(kd->typestr, "NVS_TYPE_U16");
			if(nvs_get_u16(kd->nvsh, kd->name, &kd->val.u16) == ESP_OK)
				sprintf(kd->valstr, "%u", kd->val.u16);
			else
				strcpy(kd->valstr, "NaN");
    		break;
    	case 0x12:
    		kd->len = 2;
    		strcpy(kd->typestr, "NVS_TYPE_I16");
			if(nvs_get_i16(kd->nvsh, kd->name, &kd->val.i16) == ESP_OK)
				sprintf(kd->valstr, "%d", kd->val.i16);
			else
				strcpy(kd->valstr, "NaN");
    		break;
    	case 0x04:
    		kd->len = 4;
    		strcpy(kd->typestr, "NVS_TYPE_U32");
			if(nvs_get_u32(kd->nvsh, kd->name, &kd->val.u32) == ESP_OK)
				sprintf(kd->valstr, "%u", (unsigned int)kd->val.u32);
			else
				strcpy(kd->valstr, "NaN");
    		break;
    	case 0x14:
    		kd->len = 4;
    		strcpy(kd->typestr, "NVS_TYPE_I32");
			if(nvs_get_i32(kd->nvsh, kd->name, &kd->val.i32) == ESP_OK)
				sprintf(kd->valstr, "%d", (int)kd->val.i32);
			else
				strcpy(kd->valstr, "NaN");
    		break;
    	case 0x08:
    		kd->len = 8;
    		strcpy(kd->typestr, "NVS_TYPE_U64");
			if(nvs_get_u64(kd->nvsh, kd->name, &kd->val.u64) == ESP_OK)
				sprintf(kd->valstr, "%llu", kd->val.u64);
			else
				strcpy(kd->valstr, "NaN");
    		break;
    	case 0x18:
    		kd->len = 8;
    		strcpy(kd->typestr, "NVS_TYPE_I64");
			if(nvs_get_i64(kd->nvsh, kd->name, &kd->val.i64) == ESP_OK)
				sprintf(kd->valstr, "%lld", kd->val.i64);
			else
				strcpy(kd->valstr, "NaN");
    		break;
    	case 0x21:
    		strcpy(kd->typestr, "NVS_TYPE_STR");
    		kd->valstr[0] = 0;
    		if(nvs_get_str(kd->nvsh, kd->name, NULL, &kd->len) != ESP_OK)
    			kd->len = 0;
    		break;
		case 0x42:
    		strcpy(kd->typestr, "NVS_TYPE_BLOB");
    		kd->valstr[0] = 0;
    		if(nvs_get_blob(kd->nvsh, kd->name, NULL, &kd->len) != ESP_OK)
				kd->len = 0;
			break;
		}
	return ESP_OK;
	}

esp_err_t nvs_get_handler(httpd_req_t *req)
	{
	char *buf, pn[32];
	char *pchar, *last_pchar, *tchar;
	nvs_stats_t nvs_stats;
	nvs_handle_t nvsh;
	int i, j, ret;
	keydef_t keydef;
	buf = malloc(1024);
	extern char nvs_page_start[] asm("_binary_nvseditor_html_start");
    extern char nvs_page_end[]   asm("_binary_nvseditor_html_end");
    //insert_value("devName", dev_conf.dev_name);
   // const size_t nvs_page_size = (nvs_page_end - nvs_page_start);
    
    ESP_LOGI(TAG, "uri: %s", req->uri);
    
    httpd_resp_set_type(req, "text/html");
	
    last_pchar = nvs_page_start;
    pchar = strstr(last_pchar, "partInfo");
    
    httpd_resp_send_chunk(req, last_pchar, pchar - last_pchar);
    last_pchar = pchar + strlen("partInfo");
    
	tchar = strchr(req->uri, '?');
	if(tchar)
		strcpy(pn, tchar + 1);
	else
		{
		strcpy(buf, "missing partition name</h3></div></body></html>");
		httpd_resp_send_chunk(req, buf, strlen(buf));
		httpd_resp_send_chunk(req, NULL, 0);
        return ESP_OK;
		}
    
    const esp_partition_t *np = NULL;
	esp_partition_iterator_t pit = esp_partition_find(ESP_PARTITION_TYPE_ANY, ESP_PARTITION_SUBTYPE_ANY, NULL);
    while(pit)
    	{
		np = esp_partition_get(pit);
    	if(np && strcmp(np->label, pn) == 0 && np->subtype == 2)
    		{
			break;
			}
		pit = esp_partition_next(pit);
		}
	
	if(!pit)
		{
		sprintf(buf, "partition \"%s\" not found or not NVS type</h3></div></body></html>", pn);
		httpd_resp_send_chunk(req, buf, strlen(buf));
		httpd_resp_send_chunk(req, NULL, 0);
        return ESP_OK;
		}
	nvs_get_stats(np->label, &nvs_stats);
	get_nvs_entries(pn);

	//strcpy(buf, "</div></body></html>");
	//httpd_resp_send_chunk(req, buf, strlen(buf));
	//httpd_resp_send_chunk(req, NULL, 0);
    //return ESP_OK;

	if(pchar) // insert rows in partinfo table
    	{
		//httpd_resp_send_chunk(req, last_pchar, pchar - last_pchar);
		//last_pchar = pchar + strlen("partInfo");
		sprintf(buf, "<table><tr style=\"font-size: 22px;\"><td colspan=\"2\">Partition name:</td><td>%s</td></tr>", np->label);
		httpd_resp_send_chunk(req, buf, strlen(buf));
		
		sprintf(buf, "<tr><td style=\"width: 20px;\"> </td><td>size:</td><td style=\"text-align: right;\">%u (0X%X)</td></tr>", (unsigned int)np->size, (unsigned int)np->size);
		httpd_resp_send_chunk(req, buf, strlen(buf));
		
		sprintf(buf, "<tr><td></td><td>namespaces:</td><td style=\"text-align: right;\">%d</td></tr>", nvs_stats.namespace_count);
		httpd_resp_send_chunk(req, buf, strlen(buf));
		
		sprintf(buf, "<tr><td></td><td>total entries:</td><td style=\"text-align: right;\">%d</td></tr>", nvs_stats.total_entries);
		httpd_resp_send_chunk(req, buf, strlen(buf));
		
		sprintf(buf, "<tr><td></td><td>free entries:</td><td style=\"text-align: right;\">%d</td></tr>", nvs_stats.free_entries);
		httpd_resp_send_chunk(req, buf, strlen(buf));
		
		sprintf(buf, "<tr><td></td><td>available entries:</td><td style=\"text-align: right;\">%d</td></tr>", nvs_stats.available_entries);
		httpd_resp_send_chunk(req, buf, strlen(buf));
		
		sprintf(buf, "<tr><td></td><td>used entries:</td><td style=\"text-align: right;\">%d</td></tr></table>", nvs_stats.used_entries);
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
			sprintf(buf, "<option value=\"%s\">", namespace[i].name);
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
<tr>\
	<th style=\"width: 40px; text-align: center; font-size: 40px;\"></th>\
	<th style=\"width: 140px;\">name</th>\
	<th style=\"width: 140px;\">type</th>\
	<th style=\"width: 80px;\">length(B)</th>\
	<th style=\"width: 680px;\">value&nbsp;&nbsp;<button id = \"commit_ch\" type=\"button\" disabled onclick=\"commitc()\">Commit changes</button></th>\
    <th style=\"width: 80px;\"><button style=\"width: 80px;\" type=\"button\" onclick=\"delsel()\">Erase sel</button></th>\
</tr>");		
		httpd_resp_send_chunk(req, buf, strlen(buf));
		for(i = 0; i < nns; i++)
			{
			strcpy(buf, "<tr class=\"strong\"><td class=\"shsign\"><a id = \"");
			strcat(buf, namespace[i].name);
			strcat(buf, "\" style=\"text-decoration: none;\" href=\"#\" onclick=\"showhide(\'");
			strcat(buf, namespace[i].name);
			strcat(buf, "\')\">+</a></td>");
			httpd_resp_send_chunk(req, buf, strlen(buf));
//namespace name			
        	sprintf(buf, "<td colspan=\"4\"><b>%s</b></td>", namespace[i].name);
        	httpd_resp_send_chunk(req, buf, strlen(buf));
//namespace checkbox        	
        	sprintf(buf, "<td><label> &nbsp; &nbsp; &nbsp; &nbsp;</label><input class=\"sel2delns\" type=\"checkbox\" name=\"%s\"></td></tr>\n", namespace[i].name);
        	httpd_resp_send_chunk(req, buf, strlen(buf));
        	
        	ret = nvs_open_from_partition(pn, namespace[i].name, NVS_READONLY, &nvsh);
        	for(j = 0; j < nkeys && ret == ESP_OK; j++)
        		{
				if(nvskey[j].ns_idx == i)
					{
					keydef.nvsh = nvsh;
					keydef.type = nvskey[j].type;
					strcpy(keydef.name, nvskey[j].name);
					get_key_val(&keydef);
					nvskey[j].size = keydef.len;
					
					sprintf(buf, "<tr class=\"%s\" style=\"display: none;\"><td></td>", namespace[i].name);
					httpd_resp_send_chunk(req, buf, strlen(buf));
//key name + key type					
					sprintf(buf, "<td id=\"[%d][%d]-name\">%s</td><td id=\"[%d][%d]-type\" name=\"%d\">%s</td>", i, j, nvskey[j].name, i, j, nvskey[j].type, keydef.typestr);
					httpd_resp_send_chunk(req, buf, strlen(buf));
//length					
					sprintf(buf, "<td id=\"[%d][%d]-len\">%d</td>", i, j, nvskey[j].size);
					httpd_resp_send_chunk(req, buf, strlen(buf));
// key value
					if(nvskey[j].type < NVS_TYPE_STR)
						{
						if(nvskey[j].type & 0x10)
							sprintf(buf, "<td><input id = \"[%d][%d]\" class = \"ied\" type=\"text\" value=\"%s\"></td>", i, j, keydef.valstr);
						else
							sprintf(buf, "<td><input id = \"[%d][%d]\" class = \"ued\" type=\"text\" value=\"%s\"></td>", i, j, keydef.valstr);
						httpd_resp_send_chunk(req, buf, strlen(buf));
						}
					else if(nvskey[j].type == NVS_TYPE_STR)
						{
						char *bstring = calloc(keydef.len + 128, 1);
						//char *bbstring = calloc(keydef.len + 64, 1);
						if(bstring) //&& bbstring)
							{
							sprintf(buf, "<td><textarea id=\"[%d][%d]\" class=\"sed\" style=\"width: 675px;resize: vertical;\" rows=\"1\">", i, j);
							httpd_resp_send_chunk(req, buf, strlen(buf));
							//sprintf(bstring, "<td><textarea id=\"[%d][%d]\" style=\"width: 675px;resize: vertical;\" rows=\"1\">", i, j);
							if(nvs_get_str(nvsh, nvskey[j].name, 
								bstring, &keydef.len) == ESP_OK)
								{
								strcat(bstring, "</textarea></td>");
								httpd_resp_send_chunk(req, bstring, strlen(bstring));
								}
							else
								{
								strcpy(bstring, "Error retrieving kay value</textarea></td>");
								httpd_resp_send_chunk(req, bstring, strlen(bstring));
								}
							free(bstring);
							//free(bbstring);
							}
						}
					else if(nvskey[j].type == NVS_TYPE_BLOB)
						{

						char *bstring = calloc(keydef.len, 1);
						if(bstring)
							{
							if(nvs_get_blob(nvsh, nvskey[j].name, bstring, &keydef.len) == ESP_OK)
								{
								int is = 0, ic = 0;
								char bhex[6];
								sprintf(buf, "<td><textarea id=\"[%d][%d]\" class=\"hed\" style=\"width: 675px;resize: vertical;\" rows=\"1\">", i, j);
								httpd_resp_send_chunk(req, buf, strlen(buf));
								buf[0] = 0;
								while(is < keydef.len)
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
										ic = 0;
										}
									//is++;

									}
								strcat(buf, "</textarea><br>");
								httpd_resp_send_chunk(req, buf, strlen(buf));
								}
							else
								{
								strcpy(buf, "<td>Error retrieving BLOB from NVS<br>");
								httpd_resp_send_chunk(req, buf, strlen(buf));
								}
							free(bstring);
							}
						else
							{
							strcpy(buf, "<td>BLOB too large. Cannot allocate memory for read<br>");
							httpd_resp_send_chunk(req, buf, strlen(buf));
							}
						sprintf(buf,"\
<button id=\"dump\" onclick=\"dump(\'[%d][%d]\')\">Dump to...</button>&nbsp;&nbsp;&nbsp;\
<button id=\"upload\" type=\"button\" onclick=\"document.getElementById('[%d][%d]-file').click()\">Update from file</button>&nbsp;\
<label id=\"[%d][%d]-ud\"></label>\
<input id=\"[%d][%d]-file\" type=\"file\" onchange=\"loadf('[%d][%d]')\" style=\"display: none;\"><br>&nbsp;</td>", i, j, i, j, i, j, i, j, i, j);
						httpd_resp_send_chunk(req, buf, strlen(buf));
						}
// key checkbox
					sprintf(buf, "<td><label> &nbsp; &nbsp; &nbsp; &nbsp;</label><input class=\"sel2del\" type=\"checkbox\" name=\"%s\" id=\"[%d][%d]-sel\"></td></tr>\n", namespace[i].name, i, j);
        			ret = httpd_resp_send_chunk(req, buf, strlen(buf));
        								
					}
				}
			nvs_close(nvsh);
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
    return ESP_OK;
	}
