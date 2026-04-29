/*
 * spiffs_editor.c
 *
 *  Created on: Apr 11, 2026
 *      Author: viorel_serbu
 */

#include "spiffsop.h"

#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_system.h>
#include <fenv.h>
#include <nvs_flash.h>
#include <sys/param.h>
#include <esp_log.h>
#include <spi_flash_mmap.h>
#include "esp_err.h"
#include <esp_http_server.h>
#include <nvs.h>
#include <sys/stat.h>
#include "errno.h"
//#include "freertos/idf_additions.h"
//#include "freertos/projdefs.h"
//#include "esp_ota_ops.h"
#include "esp_partition.h"
//#include "freertos/idf_additions.h"
//#include "freertos/projdefs.h"
#include "esp_spiffs.h"
#include "project_specific.h"
#include "common_defines.h"
#include "cmd_wifi.h"
//#include "utils.h"
//#include "keep_alive.h"
#include "sys/dirent.h"
#include "ws_client_handler.h"
#include "spiffsop.h"

static char *TAG = "filesys";
static fentry_t *flist = NULL;
static int nfiles = 0;
static 	char *mpoint = "/var";

static void free_list_file()
	{
	for(int i = 0; i < nfiles; i++)
		free(flist[i].fname);
	free(flist);
	flist = NULL;
	nfiles = 0;
	}
	
int cmp_name(const void *f1, const void *f2)
	{
	return
		strcmp(((fentry_t *)f1)->fname, ((fentry_t *)f2)->fname);
	}

int list_files(char *pname, size_t *total, size_t * used)
	{
	int ret;
	//char *buf = malloc(1024);
	DIR *dir = NULL;
	struct stat st;
	struct dirent *ent;
	char ftype;
	char path[64], pfname[64], outputm[300];

	free_list_file();
	esp_vfs_spiffs_conf_t conf =
		{
		.base_path = mpoint,
		.partition_label = pname,
		.max_files = MAX_NO_FILES,
		.format_if_mount_failed = true
		};
	*total = 0, *used = 0;
/*	
	ret = esp_vfs_spiffs_unregister(pname);
	ESP_LOGI(TAG, "spiffs unregister: %d", ret);
	
	ret = esp_vfs_spiffs_register(&conf);
	ESP_LOGI(TAG, "spiffs register: %d", ret);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE)
		{
        if (ret == ESP_FAIL)
            ESP_LOGE(TAG, "Failed to mount or format filesystem");
        else if (ret == ESP_ERR_NOT_FOUND)
            ESP_LOGE(TAG, "Failed to find SPIFFS partition");
        else
            ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
        return ESP_FAIL;
		}
*/		
	ret = esp_spiffs_info(conf.partition_label, total, used);
	if (ret != ESP_OK)
		{
		ESP_LOGI(TAG, "Failed to get SPIFFS partition information (%s). Formatting...", esp_err_to_name(ret));
		ret = esp_spiffs_format(conf.partition_label);
		if(ret != ESP_OK)
			{
			ESP_LOGI(TAG, "Failed to format SPIFFS partition (%s)", esp_err_to_name(ret));
			esp_vfs_spiffs_unregister(conf.partition_label);
			return ret;
			}
		ret = esp_spiffs_info(conf.partition_label, total, used);
		if(ret != ESP_OK)
			{
			ESP_LOGI(TAG, "Failed to get SPIFFS partition information (%s).", esp_err_to_name(ret));
			esp_vfs_spiffs_unregister(conf.partition_label);
			return ret;
			}
		}
	ESP_LOGI(TAG, "Partition name: %s / total size: %-d / used: %-d\n", conf.partition_label, &total, &used);
	
	dir = opendir(conf.base_path);
	if (!dir)
		{
		ESP_LOGI(TAG, "Error opening %s directory\n", conf.base_path);
		return ESP_FAIL;
		}
	ESP_LOGI(TAG, "mount point: %s", conf.base_path);
	while ((ent = readdir(dir)) != NULL)
		{
		if(strlen(ent->d_name))
			{
			fentry_t *tmp = realloc(flist, (nfiles + 1) * sizeof(fentry_t));
			if(tmp)
				{
				flist = tmp;
				flist[nfiles].fname = calloc(1, strlen(ent->d_name) + 2);
				if(flist[nfiles].fname)
					strcpy(flist[nfiles].fname, ent->d_name);
				else
					{
					ESP_LOGE(TAG, "Not enough memory to allocate for list of files");
					free_list_file();
					return ESP_FAIL;
					}
				}
			else
				{
				ESP_LOGE(TAG, "Not enough memory to allocate for list of files");
				free_list_file();
				return ESP_FAIL;
				}
			}
		else
			continue;
		strcpy(pfname, conf.base_path);
		strcat(pfname, "/");
		strcat(pfname, ent->d_name);
		if(stat(pfname, &st) == 0)
			{
			strcpy(flist[nfiles].fdate, ctime (&st.st_mtim.tv_sec));
			flist[nfiles].size = st.st_size;
			}
		nfiles++;
		}
	closedir(dir);
	qsort(flist, nfiles, sizeof(fentry_t), cmp_name);
	for(int i = 0; i < nfiles; i++)
		ESP_LOGI(TAG, "%s  %d  %s", flist[i].fname, flist[i].size, flist[i].fdate);
	
	//free(buf);
	//ret = esp_vfs_spiffs_unregister(pname);
	//ESP_LOGI(TAG, "spiffs unregister: %d", ret);
	return ESP_OK;
	}

esp_err_t spiffs_get_handler(httpd_req_t *req)
	{
	char pn[32], path[100];
	char *buf, *pchar, *last_pchar, *tchar;
	int ret;
	size_t tot, usd;
	extern char fs_page_start[] asm("_binary_spiffsed_html_start");
	
	ESP_LOGI(TAG, "uri: %s", req->uri);
	
	tchar = strchr(req->uri, '?');
	if(tchar)
		strcpy(pn, tchar + 1);
	else
		{
		httpd_resp_send_chunk(req, "<html><h1>missing partition name!</h1></html>", HTTPD_RESP_USE_STRLEN);
		httpd_resp_send_chunk(req, NULL, 0);
        return ESP_OK;
		}
	if(list_files(pn, &tot, &usd) == ESP_OK)
		{
		buf = malloc(1024);
		httpd_resp_set_type(req, "text/html");
	    last_pchar = fs_page_start;
    	pchar = strstr(last_pchar, "partInfo");
	    httpd_resp_send_chunk(req, last_pchar, pchar - last_pchar);
    	last_pchar = pchar + strlen("partInfo");
    	snprintf(buf,  1022, 
			"<table> \
				<tr style=\"font-size: 22px;\"> \
					<td colspan=\"2\">Partition name:</td> \
					<td>%s</td> \
				</tr> \
				<tr> \
					<td style=\"width: 20px;\"> </td> \
					<td>size:</td> \
					<td style=\"text-align: right;\">%d</td> \
				</tr> \
				<tr> \
					<td></td> \
					<td>used:</td> \
					<td style=\"text-align: right;\">%d</td> \
				</tr> \
				<tr> \
					<td></td> \
					<td>mount point:</td> \
					<td style=\"text-align: right;\">%s</td> \
				</tr> \
			</table></div></div>", pn, tot, usd, mpoint);
		buf[1023] = 0;
		httpd_resp_send_chunk(req, buf, HTTPD_RESP_USE_STRLEN);
		pchar = strstr(last_pchar, "addFile");
		if(pchar)
			{
			last_pchar = pchar + strlen("addFile");
			pchar = strstr(last_pchar, "insertFiles");
			if(pchar)
				{
	    		httpd_resp_send_chunk(req, last_pchar, pchar - last_pchar);
	    		last_pchar = pchar + strlen("insertFiles");
				for(int i = 0; i < nfiles; i++)
					{
					sprintf(buf, 
					"<tr><td style=\"text-align:left;\">%s</td> \
        			<td id=\"size-%d\" style=\"text-align:right;\">%d</td> \
        			<td></td><td>%s</td> \
        			<td><textarea class = \"sed\" id=\"ta-%d\" rows=\"1\" style=\"width: 673px; resize: vertical;\">", 
        			flist[i].fname, i, flist[i].size, flist[i].fdate, i);
        			httpd_resp_send_chunk(req, buf, HTTPD_RESP_USE_STRLEN);
        			strcpy(path, mpoint);
    				strcat(path, "/");
					strcat(path, flist[i].fname);
					FILE *f = fopen(path, "r");
					if (f != NULL)
						{
						int idx = 0;
						while(!feof(f))
							{
							int cr = fgetc(f);
							if(cr == EOF)
								{
								if(idx)
									httpd_resp_send_chunk(req, buf, idx);
								break;
								}
							if((cr >= 9 && cr <= 13) ||
								(cr >= 32 && cr <= 127))
								{
								buf[idx++] = cr;
								if(idx == 512)
									{
									httpd_resp_send_chunk(req, buf, idx);
									idx = 0;
									}
								}
							else
								{
								httpd_resp_send_chunk(req, " !!!binary files not supported !!!", HTTPD_RESP_USE_STRLEN);
								ESP_LOGI(TAG, "binary files not supported");
								break;
								}
							}
						fclose(f);
        				}
        			sprintf(buf, "</textarea></td><td><label> &nbsp; &nbsp; &nbsp; &nbsp;</label> \
        			<input id=\"sel-%d\" class=\"sel2delns\" type=\"checkbox\"></td></tr>", i);
        			httpd_resp_send_chunk(req, buf, HTTPD_RESP_USE_STRLEN);
					}
				}
			}
		free(buf);
		httpd_resp_send_chunk(req, "</table></div></body></html>", HTTPD_RESP_USE_STRLEN);
		}
	else
		{
		httpd_resp_send_chunk(req, "<html><h1>error retrieving list of files!</h1></html>", HTTPD_RESP_USE_STRLEN);
		}
	httpd_resp_send_chunk(req, NULL, 0);
	//ret = esp_vfs_spiffs_unregister(pn);
	//ESP_LOGI(TAG, "spiffs unregister: %d", ret);
	return ESP_OK;
	}

int create_file(char * fname)
	{
	char path[64];
	strcpy(path, mpoint);
	strcat(path, "/");
	strcat(path, fname);
	FILE *f = fopen(path, "w");
	if(!f)
		return errno;
	else
		fclose(f);
	return ESP_OK;
	}
