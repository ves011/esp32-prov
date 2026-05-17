/*
 * part_editor.c
 *
 *  Created on: Feb 18, 2026
 *      Author: viorel_serbu
 */

#include "part_editor.h"

#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_system.h>
#include <nvs_flash.h>
#include <stdbool.h>
#include <sys/param.h>
#include <esp_log.h>
#include <spi_flash_mmap.h>
#include "esp_err.h"
#include "esp_http_server.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "esp_image_format.h"
#include "project_specific.h"
#include "cmd_wifi.h"
#include "utils.h"
#include "file_server.h"
#include "ws_client_handler.h"

static const char *TAG = "handler";
static void enum_partitions(httpd_req_t *req);
static void insert_part_options(httpd_req_t *req);
int npart;
ptable_t pTable[MAX_UPDPART];


esp_err_t part_get_handler(const uint8_t *start, const uint8_t *end, httpd_req_t *req)
	{
	//char buf[32];
	//char filepath[512];
	char *pchar = NULL, *last_pchar;
    
	if(restart_in_progress == 1)
		my_esp_restart();

	ESP_LOGI(TAG, "uri: \"%s\"", req->uri);
    last_pchar = (char *)start;
    pchar = strstr((char *)start, "id=\"devName\"");
	pchar = strstr(last_pchar, INSERTPARTITIONS);
	if(pchar)
		{
		ESP_LOGI(TAG, "insPart = found %d %d", strlen(last_pchar), strlen(pchar));
		// send page between las_pchar and pchar
		httpd_resp_send_chunk(req, last_pchar, pchar - last_pchar);
		pchar += strlen(INSERTPARTITIONS);
		enum_partitions(req);
		last_pchar = pchar;
		pchar = strstr(last_pchar, INSERTOPTPART);
		if(pchar)
			{
			ESP_LOGI(TAG, "insPartOptions = found %d %d", strlen(last_pchar), strlen(pchar));
			httpd_resp_send_chunk(req, last_pchar, pchar - last_pchar);
			insert_part_options(req);
			pchar += strlen(INSERTOPTPART);
			last_pchar = pchar;
			}
		}
	
	//send remaining data
	httpd_resp_send_chunk(req, last_pchar, (char *)end - last_pchar);
	
	//end of page
	httpd_resp_send_chunk(req, NULL, 0);
		
    return ESP_OK;
	}

esp_err_t part_update_handler(const uint8_t *start, const uint8_t *end, httpd_req_t *req)
	{
	ESP_LOGI(TAG, "root_update_handler %s", req->uri);
	char*  buf = malloc(req->content_len + 2);
	if(buf)
		{
		size_t off = 0;
		while (off < req->content_len) 
			{
			/* Read data received in the request */
			int ret = httpd_req_recv(req, buf + off, req->content_len - off);
			if (ret <= 0) 
				{
				if (ret == HTTPD_SOCK_ERR_TIMEOUT)
					httpd_resp_send_408(req);
				free (buf);
				return ESP_FAIL;
				}
			off += ret;
			ESP_LOGI(TAG, "root_post_handler recv length %d", ret);
			}
		buf[off] = '&';
		buf[off + 1] = '\0';
		ESP_LOGI(TAG, "root_post_handler buf=[%s]", buf);
		if(strstr(buf, REBOOTFORM"=1"))
			{
			ESP_LOGI(TAG, "restart triggered");
			restart_in_progress = 1;
			}
		httpd_resp_set_status(req, "303 See Other");
	    httpd_resp_set_hdr(req, "Location", "/");
	    httpd_resp_sendstr(req, "Update success");
		return ESP_OK;
		}
	else
		httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Payload too large");
	return ESP_FAIL;
	}
	
void enum_partitions(httpd_req_t *req)
	{
	char btmp[80], cversion[32], dtime[40];
	char part_chunk[1024];
	bool runp, upd;
	esp_app_desc_t pdesc;
	const esp_partition_t *np = NULL;
	const esp_partition_t *bootp = esp_ota_get_boot_partition();
	esp_partition_iterator_t pit = esp_partition_find(ESP_PARTITION_TYPE_ANY, ESP_PARTITION_SUBTYPE_ANY, NULL);
	//esp_partition_iterator_t it = pit;
	npart = 0;
    while(pit)
    	{
		runp = false;
		upd = false;
    	np = esp_partition_get(pit);
    	if(np)
    		{
			int ret = esp_ota_get_partition_description(np, &pdesc);
			if(ret == ESP_OK)
				{
				strcpy(cversion, pdesc.version);
				strcpy(dtime, pdesc.date);
				strcat(dtime, " ");
				strcat(dtime, pdesc.time);
				}
			else 
				{
				strcpy(cversion, "N/A");
				strcpy(dtime, "N/A");
				}
				
			ESP_LOGI(TAG, "part entry: %s %d %d %x %x", np->label, np->type, np->subtype, np->address, np->size);
			
			size_t phys_offs = spi_flash_cache2phys(enum_partitions);
			if (np->address <= phys_offs && np->address + np->size > phys_offs)
				runp = true;
			if(np->type == ESP_PARTITION_TYPE_APP)
				upd = true;
			if(np->type == ESP_PARTITION_TYPE_DATA && 
				(np->subtype == ESP_PARTITION_SUBTYPE_DATA_NVS || 
				 np->subtype == ESP_PARTITION_SUBTYPE_DATA_SPIFFS ||
				 np->subtype == ESP_PARTITION_SUBTYPE_DATA_LITTLEFS ||
				 np->subtype == ESP_PARTITION_SUBTYPE_DATA_FAT ||
				 np->subtype == ESP_PARTITION_SUBTYPE_DATA_COREDUMP))
				 upd = true;
			strcpy(part_chunk, "<tr><td>");
			if(np->subtype == ESP_PARTITION_SUBTYPE_DATA_NVS)
				{
				strlcat(part_chunk, "<a href=\"nvs?", sizeof(part_chunk));
				strlcat(part_chunk, np->label, sizeof(part_chunk));
				strlcat(part_chunk, "\">", sizeof(part_chunk));
				strlcat(part_chunk, np->label, sizeof(part_chunk));
				strlcat(part_chunk, "</a>", sizeof(part_chunk));
				}
			else if(np->subtype == ESP_PARTITION_SUBTYPE_DATA_SPIFFS)
				{
				strlcat(part_chunk, "<a href=\"spiffs_editor.html?", sizeof(part_chunk));
				strlcat(part_chunk, np->label, sizeof(part_chunk));
				strlcat(part_chunk, "\">", sizeof(part_chunk));
				strlcat(part_chunk, np->label, sizeof(part_chunk));
				strlcat(part_chunk, "</a>", sizeof(part_chunk));
				}
			else
				strlcat(part_chunk, np->label, sizeof(part_chunk));
			if(np == bootp)
				strlcat(part_chunk, "(b)", sizeof(part_chunk));
			if(runp)
				strlcat(part_chunk, "(*)", sizeof(part_chunk));
			strlcat(part_chunk, "</td><td>", sizeof(part_chunk));
			
			if(np->type == ESP_PARTITION_TYPE_APP) strlcat(part_chunk, "APP</td>", sizeof(part_chunk));
			else if(np->type == ESP_PARTITION_TYPE_DATA) strlcat(part_chunk, "DATA</td>", sizeof(part_chunk));
			else strlcat(part_chunk, "other</td>", sizeof(part_chunk));
			
			if(np->subtype >= ESP_PARTITION_SUBTYPE_APP_OTA_MIN && np->subtype <= ESP_PARTITION_SUBTYPE_APP_OTA_MAX)
				{
				sprintf(btmp, "%d</td>", np->subtype - ESP_PARTITION_SUBTYPE_APP_OTA_MIN);
				strlcat(part_chunk, "<td>ota_", sizeof(part_chunk));
				strlcat(part_chunk, btmp, sizeof(part_chunk));
				}
			else if(np->subtype == ESP_PARTITION_SUBTYPE_DATA_OTA)strlcat(part_chunk, "<td>OTA</td>", sizeof(part_chunk));
			else if(np->subtype == ESP_PARTITION_SUBTYPE_APP_FACTORY){strlcat(part_chunk, "<td>factory</td>", sizeof(part_chunk));}
			else if(np->subtype == ESP_PARTITION_SUBTYPE_DATA_PHY)strlcat(part_chunk, "<td>PHY</td>", sizeof(part_chunk));
			else if(np->subtype == ESP_PARTITION_SUBTYPE_DATA_NVS){strlcat(part_chunk, "<td>NVS</td>", sizeof(part_chunk)); }
			else if(np->subtype == ESP_PARTITION_SUBTYPE_DATA_COREDUMP){strlcat(part_chunk, "<td>COREDUMP</td>", sizeof(part_chunk));}
			else if(np->subtype == ESP_PARTITION_SUBTYPE_DATA_FAT){strlcat(part_chunk, "<td>FAT</td>", sizeof(part_chunk));;}
			else if(np->subtype == ESP_PARTITION_SUBTYPE_DATA_SPIFFS){strlcat(part_chunk, "<td>SPIFFS</td>", sizeof(part_chunk));}
			else if(np->subtype == ESP_PARTITION_SUBTYPE_DATA_LITTLEFS){strlcat(part_chunk, "<td>LITTLEFS</td>", sizeof(part_chunk));}
			else strlcat(part_chunk, "other</td>", sizeof(part_chunk));
			if(upd && npart < MAX_UPDPART)
				{
				strcpy(pTable[npart].name, np->label);
				if(np == bootp)
					pTable[npart].boot = 1;
				else				
					pTable[npart].boot = 0;
				
				if(runp)
					pTable[npart].run = 1;
				else				
					pTable[npart].run = 0;
				pTable[npart].address = np->address;
				pTable[npart].size = np->size;
				npart++;
				}
			sprintf(btmp, "<td style=\"text-align:right;\">0x%X</td>", (unsigned int)np->address);
			strlcat(part_chunk, btmp, sizeof(part_chunk));
			
			sprintf(btmp, "<td style=\"text-align:right;\">0x%X</td>\n", (unsigned int)np->size);
			strlcat(part_chunk, btmp, sizeof(part_chunk));
			
			sprintf(btmp, "<td style=\"text-align:left;\">%s</td>\n", cversion);
			strlcat(part_chunk, btmp, sizeof(part_chunk));
			
			sprintf(btmp, "<td style=\"text-align:left;\">%s</td></tr>\n", dtime);
			strlcat(part_chunk, btmp, sizeof(part_chunk));
			
			httpd_resp_sendstr_chunk(req, part_chunk);
    		}
    	pit = esp_partition_next(pit);
    	}
    esp_partition_iterator_release(pit);
	}
void insert_part_options(httpd_req_t *req)
	{
	char opt_chunk[100];
	//<select name="parts" id="parts"><option value="">insertOptions</option></select>
	for(int i = 0; i < npart; i++)
		{
		strcpy(opt_chunk, "<option value=\"");
		strcat(opt_chunk, pTable[i].name);
		strcat(opt_chunk, "\">");
		strcat(opt_chunk, pTable[i].name);
		strcat(opt_chunk, "</option>");
		httpd_resp_sendstr_chunk(req, opt_chunk);
		}	
	strcpy(opt_chunk, "</select>");
	httpd_resp_sendstr_chunk(req, opt_chunk);
	}
	

int set_bp(char *pName)
	{
	int ret = ESP_FAIL;
	esp_image_metadata_t meta;
	const esp_partition_t *np = NULL;
	esp_partition_iterator_t pit = esp_partition_find(ESP_PARTITION_TYPE_ANY, ESP_PARTITION_SUBTYPE_ANY, NULL);
	while(pit)
		{
		np = esp_partition_get(pit);
		if(!strcmp(pName, np->label))
			{
			esp_partition_pos_t pos = {
                .offset = np->address,
                .size   = np->size
            	};
			ret = esp_image_verify(ESP_IMAGE_VERIFY, &pos, &meta);
			if(ret == ESP_OK)
				ret = esp_ota_set_boot_partition(np);
			break;
			}
		pit = esp_partition_next(pit);
		}
	esp_partition_iterator_release(pit);
	if(ret != ESP_OK)
		ESP_LOGE(TAG, "Image verification failed for %s: %s", pName, esp_err_to_name(ret));
	return ret;
	}
	
	
/* Handler to upload a file and flash it to partition of choice*/
//esp_err_t flashing_post_handler(httpd_req_t *req)
esp_err_t flashing_post_handler(const uint8_t *start, const uint8_t *end, httpd_req_t *req)
	{
	char btmp[128];
	int idx, i, rcv, size = 0, ret;
    char part_buf[20];
	strlcpy(part_buf, req->uri + strlen(PART_UPLOAD), sizeof(part_buf));
	const char *part = part_buf;
   	size = req->content_len;
	ESP_LOGI(TAG, "URI: %s / req len: %d / size: %d", req->uri, req->content_len, size);
                                             
	// get partition index in pTable
	for(i = 0; i < npart; i++)
		{
		if(strcmp(pTable[i].name, part) == 0)
			break;
		}
	if(i < npart)
		idx = i;
	else
		{
		snprintf(btmp, sizeof(btmp), "Invalid partition name: \"%s\"", part_buf);
		ws_send_status(OP_UPLOAD, PAR_ERROR, -1, btmp);
        return ESP_OK;
		}
    /* Retrieve the pointer to scratch buffer for temporary storage */
    char *buf = ((struct file_server_data *)req->user_ctx)->scratch;
    int received;
	if(size > pTable[idx].size)
		{
		ws_send_status(OP_UPLOAD, PAR_ERROR, -1, "file size larger than partition size");
    	return ESP_OK;
		}
	else
		{
		if(pTable[idx].run)
			{
			ws_send_status(OP_UPLOAD, PAR_ERROR, -1, "flashing running partition not allowed");
    		return ESP_OK;
			}
		}

	const esp_partition_t *np = NULL;
	esp_partition_iterator_t pit = esp_partition_find(ESP_PARTITION_TYPE_ANY, ESP_PARTITION_SUBTYPE_ANY, NULL);
    while(pit)
    	{
		np = esp_partition_get(pit);
    	if(np && (strcmp(np->label, pTable[idx].name) == 0))
				break;
		pit = esp_partition_next(pit);
		}
	esp_partition_iterator_release(pit);
	if(!pit)
		{
		snprintf(btmp, sizeof(btmp), "Partition not found: %s", part);
		ESP_LOGE(TAG, "%s", btmp);
		ws_send_status(OP_UPLOAD, PAR_ERROR, -1, btmp);
    	return ESP_OK;
		}
	ret = ESP_OK;
	if(np && np->subtype == ESP_PARTITION_SUBTYPE_DATA_NVS)
		ret = nvs_flash_deinit_partition(np->label);
	if(ret == ESP_OK)
		{
		ret = esp_partition_erase_range(np, 0, np->size);
		if(ret != ESP_OK)
			{
			snprintf(btmp, sizeof(btmp), "error erasing partition\n%s", esp_err_to_name(ret));
			ws_send_status(OP_UPLOAD, PAR_ERROR, -1, btmp);
			return ESP_OK;
			}
		}
	else
		{
		ws_send_status(OP_UPLOAD, PAR_ERROR, -1, "error while dinit NVS partition");
		return ESP_OK;
		}
	rcv = 0;
	
    while (rcv < size) 
    	{
        ESP_LOGI(TAG, "Remaining size : %d", size - rcv);
        /* Receive the file part by part into a buffer */
		if ((received = httpd_req_recv(req, buf, MIN(size - rcv, SCRATCH_BUFSIZE))) < 0) 
        	{
            if(received == HTTPD_SOCK_ERR_TIMEOUT) 
                continue;
			snprintf(btmp, sizeof(btmp), "File reception failed! - %d", received);
            ESP_LOGE(TAG, "%s", btmp);
            /* In case of unrecoverable error,
            Respond with 500 Internal Server Error */
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, btmp);
            return ESP_FAIL;
        	}
		else if(received == 0 && rcv < size)
        	{
			ESP_LOGW(TAG, "Connection closed before receiving all data");
			httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Connection closed before receiving all data");
			return ESP_FAIL;
			}
        ESP_LOGI(TAG, "Receiving file : %s size: %d", part, received);
        ret = esp_partition_write(np, rcv, buf, received);
		if(ret != ESP_OK)
			{
			snprintf(btmp, sizeof(btmp), "error writing partition: %s", esp_err_to_name(ret));
			ws_send_status(OP_UPLOAD, PAR_ERROR, ret, btmp);
			return ESP_OK;
			}
        snprintf(btmp, sizeof(btmp), "%d", rcv * 100 / size);
        ws_send_status(OP_UPLOAD, PAR_PROGRESS, 0, btmp); 
        rcv += received;
    	}
	if(np && np->subtype == ESP_PARTITION_SUBTYPE_DATA_NVS)
		{
		ret = nvs_flash_init_partition(np->label);
		if(ret != ESP_OK)
			{
			ws_send_status(OP_UPLOAD, PAR_ERROR, -1, "error while dinit NVS partition");
			return ESP_OK;
			}
		}
    ESP_LOGI(TAG, "File reception complete");
    httpd_resp_set_status(req, "200 OK");
    httpd_resp_sendstr(req, "File upload status");
    return ESP_OK;
	}

//esp_err_t dump_get_handler(httpd_req_t *req)
esp_err_t dump_get_handler(const uint8_t *start, const uint8_t *end, httpd_req_t *req)
	{
    char pname[20];
    char *buf;
    int sent, size, ret = ESP_FAIL, sz2r, bsize = 4096;
    
	buf = calloc(bsize, 1);
	if(buf == NULL)
		{
		ESP_LOGI(TAG, "cannot allocate chunk buffer bytes");
		ws_send_status(OP_DOWNLOAD, PAR_ERROR, -1, "cannot allocate chunk buffer");
		return ESP_OK;
		}

	strncpy(pname, req->uri + strlen(PART_DOWNLOAD), sizeof(pname) -1 );
	ESP_LOGI(TAG, "download handler: %s %d", pname, strlen(req->uri));
	
	const esp_partition_t *np = NULL;
	esp_partition_iterator_t pit = esp_partition_find(ESP_PARTITION_TYPE_ANY, ESP_PARTITION_SUBTYPE_ANY, NULL);
    while(pit)
    	{
		np = esp_partition_get(pit);
		if(!strcmp(np->label, pname))
			break;
		pit = esp_partition_next(pit);
		}
	if(pit)
		{
		char dispo[64];
		size = np->size;
		sent = 0;
		httpd_resp_set_type(req, "application/octet-stream");
	
	    snprintf(dispo,  sizeof(dispo), "attachment; filename=\"%s.bin\"", pname);
		httpd_resp_set_hdr(req, "Content-Disposition", dispo);
		
		while(sent < size)
			{
			sz2r = MIN(bsize, size - sent);
			ret = esp_partition_read(np, sent, buf, sz2r);
			if(ret != ESP_OK)
				{
				ESP_LOGI(TAG, "error reading partition %d", ret);
				ws_send_status(OP_DOWNLOAD, PAR_ERROR, ret, esp_err_to_name(ret));
				break;
				}
			ret = httpd_resp_send_chunk(req, buf, sz2r);
			if (ret != ESP_OK) 
				{
				ESP_LOGE(TAG,"send chunk failed: %s", esp_err_to_name(ret));
				break;
           		}
           	sent += sz2r;
			}
		}
	else
		ws_send_status(OP_DOWNLOAD, PAR_ERROR, -1, "partition not found");
	free(buf);
	esp_partition_iterator_release(pit);
    ESP_LOGI(TAG, "File send status %d", ret);
    if(ret == ESP_OK)
    	httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
	}