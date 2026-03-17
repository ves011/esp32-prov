/*
 * handlers.c
 *
 *  Created on: Feb 18, 2026
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
#include "esp_http_server.h"
//#include "freertos/idf_additions.h"
//#include "freertos/projdefs.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"
#include "freertos/idf_additions.h"
#include "freertos/projdefs.h"
#include "project_specific.h"
#include "common_defines.h"
#include "cmd_wifi.h"
#include "utils.h"
#include "keep_alive.h"
#include "ws_client_handler.h"
#include "handlers.h"

const char *TAG = "handler";
static int genconf_update(char *params);
static void enum_partitions(httpd_req_t *req);
static void insert_part_options(httpd_req_t *req);
int npart;
ptable_t pTable[MAX_UPDPART];
//static char pname[10][20];
int wsfd;

/* Copies the full path into destination buffer and returns
 * pointer to path (skipping the preceding base path) */
static const char* get_path_from_uri(char *dest, const char *base_path, const char *uri, size_t destsize)
{
    const size_t base_pathlen = strlen(base_path);
    size_t pathlen = strlen(uri);

    const char *quest = strchr(uri, '?');
    if (quest) {
        pathlen = MIN(pathlen, quest - uri);
    }
    const char *hash = strchr(uri, '#');
    if (hash) {
        pathlen = MIN(pathlen, hash - uri);
    }

    if (base_pathlen + pathlen + 1 > destsize) {
        /* Full path string won't fit into destination buffer */
        return NULL;
    }

    /* Construct full path (base + path) */
    strcpy(dest, base_path);
    strlcpy(dest + base_pathlen, uri, pathlen + 1);

    /* Return pointer to path, skipping the base */
    return dest + base_pathlen;
}

/* Handler to respond with an icon file embedded in flash.
 * Browsers expect to GET website icon at URI /favicon.ico.
 * This can be overridden by uploading file with same name */
static esp_err_t favicon_get_handler(httpd_req_t *req)
	{
    extern const unsigned char favicon_ico_start[] asm("_binary_favicon_ico_start");
    extern const unsigned char favicon_ico_end[]   asm("_binary_favicon_ico_end");
    const size_t favicon_ico_size = (favicon_ico_end - favicon_ico_start);
    httpd_resp_set_type(req, "image/x-icon");
    httpd_resp_send(req, (const char *)favicon_ico_start, favicon_ico_size);
    return ESP_OK;
	}

esp_err_t root_get_handler(httpd_req_t *req)
	{
	char buf[32];
	char filepath[512];
	char *pchar = NULL, *last_pchar;
	extern char main_page_start[] asm("_binary_main_html_start");
    extern char main_page_end[]   asm("_binary_main_html_end");
    //insert_value("devName", dev_conf.dev_name);
    const size_t main_page_size = (main_page_end - main_page_start);
    
    if(restart_in_progress == 1)
    	esp_restart();
    const char *filename = get_path_from_uri(filepath, "/",
                                             req->uri, sizeof(filepath));
	ESP_LOGI(TAG, "uri: %s / fname: %s / fpath: %s", req->uri, filename, filepath);
	
    if (strcmp(filename, "/favicon.ico") == 0)
            return favicon_get_handler(req);                    
    httpd_resp_set_type(req, "text/html");
    //httpd_resp_send(req, "<h1>Hello Secure World!</h1>", HTTPD_RESP_USE_STRLEN);
    //httpd_resp_send(req, main_page_start, main_page_size);
    // find first value=\" 
    last_pchar = main_page_start;
    
    pchar = strstr((char *)main_page_start, "id=\"devName\"");
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
	httpd_resp_send_chunk(req, last_pchar, main_page_end - last_pchar);
	
	//end of page
	httpd_resp_send_chunk(req, pchar, 0);
		
    return ESP_OK;
	}

esp_err_t root_update_handler(httpd_req_t *req)
	{
	ESP_LOGI(TAG, "root_update_handler %s", req->uri);
	//if(req->content_len == 0)
	//	{
	//	esp_restart();
	//	restart_in_progress = 1;
	//	}
	char*  buf = malloc(req->content_len + 2);
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
/*
int genconf_update(char *params)
	{
	int ret = ESP_OK;
	dev_config_t dc;
	//params[strlen(params) - 1] = '&';
	char btemp[32], *pstr;
	pstr = strtok(params, "&");
	while(pstr)
		{
		if(strstr(pstr, DEVNAME"="))
			strcpy(dc.dev_name, pstr + strlen(DEVNAME"="));
		else if(strstr(pstr, DEVID"="))
			dc.dev_id = atoi(pstr + strlen(DEVID"="));
		else if(strstr(pstr, STASSID"="))
			strcpy(dc.sta_ssid, pstr + strlen(STASSID"="));
		else if(strstr(pstr, STAPASS"="))
			strcpy(dc.sta_pass, pstr + strlen(STAPASS"="));
		else if(strstr(pstr, APSSID"="))
			strcpy(dc.ap_ssid, pstr + strlen(APSSID"="));
		else if(strstr(pstr, APPASS"="))
			strcpy(dc.ap_pass, pstr + strlen(APPASS"="));
		else if(strstr(pstr, APIP"="))
			strcpy(btemp, pstr + strlen(APIP"="));
		pstr = strtok(NULL, "&");
		}
	strcat(btemp, ".");
	ESP_LOGI(TAG, "btemp: %s", btemp);
	pstr = strtok(btemp, ".");
	if(pstr)
		dc.ap_a = atoi(pstr);
	pstr = strtok(NULL, ".");
	if(pstr)
		dc.ap_b = atoi(pstr);
	pstr = strtok(NULL, ".");
	if(pstr)
		dc.ap_c = atoi(pstr);
	pstr = strtok(NULL, ".");
	if(pstr)
		dc.ap_d = atoi(pstr);
		
	if(strcmp(dev_conf.dev_name, dc.dev_name) ||
		dev_conf.dev_id != dc.dev_id ||
		strcmp(dev_conf.sta_ssid, dc.sta_ssid) ||
		strcmp(dev_conf.sta_pass, dc.sta_pass) ||
		strcmp(dev_conf.ap_ssid, dc.ap_ssid) ||
		strcmp(dev_conf.ap_pass, dc.ap_pass))
		{
		dc.cs = dev_conf.cs;
		memcpy(&dev_conf, &dc, sizeof(dev_config_t));
		rw_dev_config(PARAM_WRITE);
		}
	ESP_LOGI(TAG, "genconf update : %s %d %s %s %s %s %d.%d.%d.%d", 
		dc.dev_name, dc.dev_id, dc.sta_ssid, dc.sta_pass, dc.ap_ssid, dc.ap_pass, dc.ap_a, dc.ap_b, dc.ap_c, dc.ap_d);
	return ret;
	}	
esp_err_t main_post_handler(httpd_req_t *req)
	{
	ESP_LOGI(TAG, "main_post_handler %s", req->uri);
	return ESP_OK;
	}
*/
esp_err_t set_boot_handler(httpd_req_t *req)
	{
	ESP_LOGI(TAG, "set_boot_handler %s", req->uri);
	char*  buf = malloc(req->content_len + 2);
	char pbuf[60];
	int err = ESP_FAIL;
	bool sb = false;
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
		ESP_LOGI(TAG, "set_boot_handler recv length %d", ret);
		}
	buf[off] = 0;
	ESP_LOGI(TAG, "set_boot_handler buf=[%s]", buf);
	if(strstr(buf, "bp="))
		{
		strcpy(pbuf, buf + strlen("bp="));
		ESP_LOGI(TAG, "new boot partition: %s ", pbuf);
		const esp_partition_t *bootp = esp_ota_get_boot_partition();
		if(strcmp(pbuf, bootp->label))
			{
			const esp_partition_t *np = NULL;
			esp_partition_iterator_t pit = esp_partition_find(ESP_PARTITION_TYPE_ANY, ESP_PARTITION_SUBTYPE_ANY, NULL);
			while(pit)
				{
				np = esp_partition_get(pit);
				if(!strcmp(pbuf, np->label))
					{
					sb = true;
					err = esp_ota_set_boot_partition(np);
					break;
					}
				pit = esp_partition_next(pit);
				}
			}
		}
	if(sb)
		{
		if(err == ESP_OK)
			strcpy(pbuf, "/?tab=part&setboot=0");
		else
			sprintf(pbuf, "/?tab=part&setboot=%-x", err);
		}
	else
		strcpy(pbuf, "/?tab=part");
	httpd_resp_set_status(req, "303 See Other");
    httpd_resp_set_hdr(req, "Location", pbuf);
    httpd_resp_sendstr(req, "Update success");
	return ESP_OK;
	}
void enum_partitions(httpd_req_t *req)
	{
	char btmp[60];
	char part_chunk[1024];
	bool runp, upd;
	const esp_partition_t *np = NULL;
	const esp_partition_t *bootp = esp_ota_get_boot_partition();
	esp_partition_iterator_t pit = esp_partition_find(ESP_PARTITION_TYPE_ANY, ESP_PARTITION_SUBTYPE_ANY, NULL);
	npart = 0;
    while(pit)
    	{
		runp = false;
		upd = false;
    	np = esp_partition_get(pit);
    	if(np)
    		{
			ESP_LOGI(TAG, "part entry: %s %d %d %x %x", np->label, np->type, np->subtype, np->address, np->size);
			
			size_t phys_offs = spi_flash_cache2phys(enum_partitions);
			if (np->address <= phys_offs && np->address + np->size > phys_offs)
				runp = true;

			strcpy(part_chunk, "<tr><td>");
			if(np->subtype == 2)
				{
				strcat(part_chunk, "<a href=\"nvs_editor.html?");
				strcat(part_chunk, np->label);
				strcat(part_chunk, "\"/a>");
				strcat(part_chunk, np->label);
				}
			else
				strcat(part_chunk, np->label);
			if(np == bootp)
				strcat(part_chunk, "(b)");
			if(runp)
				strcat(part_chunk, "(*)");
			strcat(part_chunk, "</td><td>");
			
			if(np->type == 0) strcat(part_chunk, "APP</td>");
			else if(np->type == 1) strcat(part_chunk, "DATA</td>");
			else strcat(part_chunk, "other</td>");
			
			if(np->subtype >= ESP_PARTITION_SUBTYPE_APP_OTA_MIN && np->subtype <= ESP_PARTITION_SUBTYPE_APP_OTA_MAX)
				{
				sprintf(btmp, "%d</td>", np->subtype - ESP_PARTITION_SUBTYPE_APP_OTA_MIN);
				strcat(part_chunk, "<td>ota_");
				strcat(part_chunk, btmp);
				upd = true;
				}
			else if(np->subtype == 0)strcat(part_chunk, "<td>OTA</td>");
			else if(np->subtype == 1)strcat(part_chunk, "<td>PHY</td>");
			else if(np->subtype == 2){strcat(part_chunk, "<td>NVS</td>"); upd = true;}
			else if(np->subtype == 3){strcat(part_chunk, "<td>COREDUMP</td>"); upd = true;}
			else if(np->subtype == 0x81){strcat(part_chunk, "<td>FAT</td>"); upd = true;}
			else if(np->subtype == 0x82){strcat(part_chunk, "<td>SPIFFS</td>"); upd = true;}
			else strcat(part_chunk, "other</td>");
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
			strcat(part_chunk, btmp);
			
			sprintf(btmp, "<td style=\"text-align:right;\">0x%X</td></tr>", (unsigned int)np->size);
			strcat(part_chunk, btmp);
			httpd_resp_sendstr_chunk(req, part_chunk);
    		}
    	pit = esp_partition_next(pit);
    	}
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
	
esp_err_t ws_handler(httpd_req_t *req)
	{
	wsmsqg_t msg;
    if (req->method == HTTP_GET) 
    	{
        ESP_LOGI(TAG, "Handshake done, the new connection was opened");
        ESP_LOGI(TAG, "HTTP_GET ws_handler: httpd_handle_t=%p, sockfd=%d, client_info:%d", 
         			req->handle,
                 	httpd_req_to_sockfd(req), 
                 	httpd_ws_get_fd_info(req->handle, httpd_req_to_sockfd(req)));
        if(httpd_ws_get_fd_info(req->handle, httpd_req_to_sockfd(req)) == 2)
        	wsfd = httpd_req_to_sockfd(req);
        return ESP_OK;
    	}
    httpd_ws_frame_t ws_pkt;
    uint8_t *buf = NULL;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));

    // First receive the full ws message
    /* Set max_len = 0 to get the frame len */
    esp_err_t ret = httpd_ws_recv_frame(req, &ws_pkt, 0);
    if (ret != ESP_OK) 
    	{
        ESP_LOGE(TAG, "httpd_ws_recv_frame failed to get frame len with %d", ret);
        return ret;
    	}
    ESP_LOGI(TAG, "frame len is %d", ws_pkt.len);
    if (ws_pkt.len) 
    	{
        /* ws_pkt.len + 1 is for NULL termination as we are expecting a string */
        buf = calloc(1, ws_pkt.len + 1);
        if (buf == NULL) 
        	{
            ESP_LOGE(TAG, "Failed to calloc memory for buf");
            return ESP_ERR_NO_MEM;
        	}
        ws_pkt.payload = buf;
        /* Set max_len = ws_pkt.len to get the frame payload */
        ret = httpd_ws_recv_frame(req, &ws_pkt, ws_pkt.len);
        if (ret != ESP_OK) 
        	{
            ESP_LOGE(TAG, "httpd_ws_recv_frame failed with %d", ret);
            free(buf);
            return ret;
        	}
    	}
    // If it was a PONG, update the keep-alive
    if (ws_pkt.type == HTTPD_WS_TYPE_PONG) 
    	{
        ESP_LOGI(TAG, "Received PONG message");
        free(buf);
        return wss_keep_alive_client_is_active(httpd_get_global_user_ctx(req->handle),
                httpd_req_to_sockfd(req));

    // If it was a TEXT message, just echo it back
    	} 
    else if (ws_pkt.type == HTTPD_WS_TYPE_TEXT || ws_pkt.type == HTTPD_WS_TYPE_BINARY || ws_pkt.type == HTTPD_WS_TYPE_PING || ws_pkt.type == HTTPD_WS_TYPE_CLOSE) 
    	{
        if (ws_pkt.type == HTTPD_WS_TYPE_TEXT || ws_pkt.type == HTTPD_WS_TYPE_BINARY) 
        	{
			char b[40];
			memcpy(b, (char *)ws_pkt.payload, 32);
			b[32] = 0;
            ESP_LOGI(TAG, "Received packet with message: %s", b);
            msg.fd = httpd_req_to_sockfd(req);
            msg.len = ws_pkt.len + 1; 
            memcpy(msg.payload.strpayload, ws_pkt.payload, sizeof(msg.payload.binpayload));
            xQueueSend(ws_msg_queue, &msg, pdMS_TO_TICKS(20));
            free(buf);
            return ESP_OK;
        	} 
        else if (ws_pkt.type == HTTPD_WS_TYPE_PING) 
        	{
            // Respond PONG packet to peer
            ESP_LOGI(TAG, "Got a WS PING frame, Replying PONG");
            ws_pkt.type = HTTPD_WS_TYPE_PONG;
        	} 
        else if (ws_pkt.type == HTTPD_WS_TYPE_CLOSE) 
        	{
            // Response CLOSE packet with no payload to peer
            ws_pkt.len = 0;
            ws_pkt.payload = NULL;
        	}
        ret = httpd_ws_send_frame(req, &ws_pkt);
        if (ret != ESP_OK) 
        	{
            ESP_LOGE(TAG, "httpd_ws_send_frame failed with %d", ret);
        	}
        ESP_LOGI(TAG, "ws_handler: httpd_handle_t=%p, sockfd=%d, client_info:%d", req->handle,
                 httpd_req_to_sockfd(req), httpd_ws_get_fd_info(req->handle, httpd_req_to_sockfd(req)));
        free(buf);
        return ret;
    	}
    free(buf);
    return ESP_OK;
	}
int set_bp(char *pName)
	{
	int ret = ESP_OK;
	const esp_partition_t *np = NULL;
	esp_partition_iterator_t pit = esp_partition_find(ESP_PARTITION_TYPE_ANY, ESP_PARTITION_SUBTYPE_ANY, NULL);
	while(pit)
		{
		np = esp_partition_get(pit);
		if(!strcmp(pName, np->label))
			{
			ret = esp_ota_set_boot_partition(np);
			break;
			}
		pit = esp_partition_next(pit);
		}
	return ret;
	}
/* Handler to upload a file onto the server */
esp_err_t flashing_post_handler(httpd_req_t *req)
	{
	wsmsqg_t msg;
	int idx, i, rcv, test = 0, size = 0, ret;
    char filepath[FILE_PATH_MAX];
    ESP_LOGI(TAG, "Receiving file uri: %s", req->uri);
	const char *part = get_path_from_uri(filepath, ((struct file_server_data *)req->user_ctx)->base_path,
                                             req->uri + sizeof("/upload/") - 1, sizeof(filepath));
	if(strchr(part, '.'))
		{
		test = 1;
		size = atoi(strchr(part, '.') + 1);
		*strchr(part, '.') = 0;
		}
	if(size == 0)
    	size = req->content_len;
                                             
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
		msg.fd = httpd_req_to_sockfd(req);
        sprintf(msg.payload.strpayload, USTATUS"\1error\1Invalid partition \"%s\"\1", part);
        msg.len = strlen(msg.payload.strpayload); 
        xQueueSend(ws_msg_queue, &msg, pdMS_TO_TICKS(20));
        httpd_resp_set_status(req, "303 file size mismatch");
    	httpd_resp_sendstr(req, "invalid partition name");
        return ESP_OK;
		}
    /* Retrieve the pointer to scratch buffer for temporary storage */
    char *buf = ((struct file_server_data *)req->user_ctx)->scratch;
    int received;

    /* Content length of the request gives
     * the size of the file being uploaded */
    //int remaining = req->content_len;
    //size = req->content_len;
    ESP_LOGI(TAG, "Receiving file : %s size: %d / %s", part, size, req->uri);
    //check if file size matches with partition size
    if(size > 0 && test)
    	{
		if(size > pTable[idx].size)
    		{
	        httpd_resp_set_status(req, "303 file size mismatch");
	    	httpd_resp_sendstr(req, "file size larger than partition size");
	    	return ESP_OK;
			}
		else
    		{
			if(pTable[idx].run)
				{
				httpd_resp_set_status(req, "303 file size mismatch");
	    		httpd_resp_sendstr(req, "flashing running partition not allowed");
	    		return ESP_OK;
				}
			else
				{
	        	httpd_resp_set_status(req, "302 file size OK");
	    		httpd_resp_sendstr(req, "file size OK");
	    		}
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
	if(np->subtype == ESP_PARTITION_SUBTYPE_DATA_NVS)
		ret = nvs_flash_deinit_partition(np->label);
	ret = esp_partition_erase_range(np, 0, np->size);
	if(ret != ESP_OK)
		{
		sprintf(msg.payload.strpayload, "error erasing partition\n%s", esp_err_to_name(ret));
		httpd_resp_set_status(req, "303 file size mismatch");
		httpd_resp_sendstr(req, msg.payload.strpayload);
		return ESP_OK;
		}

	rcv = 0;
	
    while (rcv < size) 
    	{
        ESP_LOGI(TAG, "Remaining size : %d", size - rcv);
        /* Receive the file part by part into a buffer */
        if ((received = httpd_req_recv(req, buf, MIN(size - rcv, SCRATCH_BUFSIZE))) <= 0) 
        	{
            if(received == HTTPD_SOCK_ERR_TIMEOUT) 
                continue;

            ESP_LOGE(TAG, "File reception failed!");
            /* In case of unrecoverable error,
            Respond with 500 Internal Server Error */
            httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to receive file");
            msg.fd = httpd_req_to_sockfd(req);
            sprintf(msg.payload.strpayload, USTATUS"\1error\1ESP failed to receive file\1");
            msg.len = strlen(msg.payload.strpayload); 
            xQueueSend(ws_msg_queue, &msg, pdMS_TO_TICKS(20));
            return ESP_FAIL;
        	}
        ESP_LOGI(TAG, "Receiving file : %s size: %d", part, received);
        ret = esp_partition_write(np, rcv, buf, received);
		if(ret != ESP_OK)
			{
			sprintf(msg.payload.strpayload, "error erasing partition\n%s", esp_err_to_name(ret));
			httpd_resp_set_status(req, "303 file size mismatch");
			httpd_resp_sendstr(req, msg.payload.strpayload);
			return ESP_OK;
			}
        msg.fd = httpd_req_to_sockfd(req);
        sprintf(msg.payload.strpayload, USTATUS"\1progress\1%d\1", rcv * 100 / size); 
		msg.len = strlen(msg.payload.strpayload);
        xQueueSend(ws_msg_queue, &msg, pdMS_TO_TICKS(20));
        rcv += received;
    	}
	if(np->subtype == ESP_PARTITION_SUBTYPE_DATA_NVS)
		ret = nvs_flash_init_partition(np->label);
    ESP_LOGI(TAG, "File reception complete");
    httpd_resp_set_status(req, "200 OK");
    httpd_resp_sendstr(req, "File upload status");
    return ESP_OK;
	}

esp_err_t dump_get_handler(httpd_req_t *req)
	{
    char pname[20];
    char *buf;
    int sent, size, ret, sz2r, bsize;
    wsmsqg_t msg;
    
    //strncpy(buf, req->uri, 30);
	//ESP_LOGI(TAG, "download handler: %d", strlen(req->uri));
	vTaskDelay(pdMS_TO_TICKS(5000));
	strcpy(pname, req->uri + strlen("/download/"));
	
	buf = calloc(5000, 1);
	if(buf == NULL)
		{
		ESP_LOGI(TAG, "cannot allocate 5000 bytes");
		return ESP_FAIL;
		}
	bsize = 5000;
	strncpy(buf, req->uri, 30);
	strcpy(pname, req->uri + strlen(PART_DOWNLOAD));
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
		size = np->size;
		sent = 0;
		while(sent < size)
			{
			sz2r = MIN(bsize, size - sent);
			ret = esp_partition_read(np, sent, buf, sz2r);
			if(ret != ESP_OK)
				{
				ESP_LOGI(TAG, "error reading partition %d", ret);
				// Abort sending file 
                httpd_resp_send_chunk(req, NULL, 0);
	            msg.fd = httpd_req_to_sockfd(req);
    	        sprintf(msg.payload.strpayload, DSTATUS"\1progress\1error reading partition\n%s\1", esp_err_to_name(ret));
        	    msg.len = strlen(msg.payload.strpayload); 
            	xQueueSend(ws_msg_queue, &msg, pdMS_TO_TICKS(20));
				return ESP_OK;
				}
			sent += sz2r;
			ret = httpd_resp_send_chunk(req, buf, sz2r);
			if (ret != ESP_OK) 
				{
                ESP_LOGE(TAG, "File sending failed!");
                // Abort sending file 
                httpd_resp_send_chunk(req, NULL, 0);
                // Respond with 500 Internal Server Error 
                httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to send file");
                msg.fd = httpd_req_to_sockfd(req);
    	        sprintf(msg.payload.strpayload, DSTATUS"\1progress\1httpd error sending file\n%s\1", esp_err_to_name(ret));
        	    msg.len = strlen(msg.payload.strpayload); 
            	xQueueSend(ws_msg_queue, &msg, pdMS_TO_TICKS(20));
               	return ESP_FAIL;
           		}
           	//ESP_LOGI(TAG, "dump file %d", (sent * 100) / size);
			}
		}
    ESP_LOGI(TAG, "File sending complete");
    httpd_resp_send_chunk(req, NULL, 0);
    return ESP_OK;
	}