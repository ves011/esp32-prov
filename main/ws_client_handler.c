/*
 * ws-client_handler.c
 *
 *  Created on: Feb 22, 2026
 *      Author: viorel_serbu
 */
 
#include <esp_event.h>
#include <esp_log.h>
#include <esp_system.h>
#include <nvs_flash.h>
#include <sys/param.h>
//#include "protocol_examples_common.h"
#include "freertos/idf_additions.h"
//#include "project_specific.h"
//#include "common_defines.h"
//#include "lwip/sockets.h"
//#include "keep_alive.h"
#include "file_server.h"
//#include "common_defines.h"
//#include "utils.h"
#include "handlers.h"
#include "nvsop.h"
#include "ws_client_handler.h"

static char *TAG = "WS_CLIENT";
QueueHandle_t ws_msg_queue;


void send_strmsg(char *msg)
	{
	int ret;
    httpd_ws_frame_t ws_pkt;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.payload = (uint8_t*)msg;
    ws_pkt.len = strlen(msg);
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;
    ret = httpd_ws_send_frame_async(w_server, wsfd, &ws_pkt);
    ESP_LOGI(TAG, "ws_send_frame_async: %d / %s", ret, msg);
	}
void send_binmsg(char *msg, int len)
	{
	int ret;
    httpd_ws_frame_t ws_pkt;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.payload = (uint8_t*)msg;
    ws_pkt.len = len;
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;
    ret = httpd_ws_send_frame_async(w_server, wsfd, &ws_pkt);
    ESP_LOGI(TAG, "ws_send_frame_async: %d / %s", ret, msg);
	}

void ws_handler_task(void *pvParameters)
	{
	wsmsqg_t msg;
	char *pstr;
	char buf[128];
	int idxn, idxk, len, nrc, type, ret, chunk_nr;
	rcv_keyval_t rval;
	uint8_t *b;
	while(1)
		{
		if(xQueueReceive(ws_msg_queue, &msg, portMAX_DELAY))
			{
			strncpy(buf, msg.payload.strpayload, 32);
			buf[32] = 0;
			ESP_LOGI(TAG, "websocket message fd:%d message: %s", msg.fd, buf);
			pstr = strtok(msg.payload.strpayload, "\1");
			if(strcmp(pstr, SETBOOT) == 0)
				{
				pstr = strtok(NULL, "\1");
				if(pstr)
					{
					
					ret = set_bp(pstr);
					if(ret != ESP_OK)
						sprintf(buf, "setBoot\1%s\1%s", esp_err_to_name(ret), pstr);
					else
						strcpy(buf, "refresh\1tab\1part");
					send_strmsg(buf);
					}				
				}
			else if(strcmp(pstr, USTATUS) == 0)
				{
				pstr = strtok(NULL, "\1");
				if(strcmp(pstr, "progress") == 0)
					{
					pstr = strtok(NULL, "\1");
					if(pstr)
						{
						sprintf(buf, USTATUS"\1progress\1%s %%\1", pstr);
						send_strmsg(buf);
						}
					}
				else if(strcmp(pstr, "error"))
					{
					strcpy(buf, USTATUS"\1error\1");
					send_strmsg(buf);
					}
				}
			else if(strcmp(pstr, DSTATUS) == 0)
				{
				pstr = strtok(NULL, "\1");
				if(strcmp(pstr, "progress") == 0)
					{
					pstr = strtok(NULL, "\1");
					if(pstr)
						{
						sprintf(buf, DSTATUS"\1progress\1%s\1", pstr);
						send_strmsg(buf);
		                }
					}
				else if(strcmp(pstr, "error"))
					{
					
					}
				}
			else if(strcmp(pstr, ERASE) == 0)
				{
				pstr = strtok(NULL, "\1");
				if(strcmp(pstr, "query") == 0)
					{
					pstr = strtok(NULL, "\1");
					for(int i = 0; i < npart; i++)
						{
						if(strcmp(pTable[i].name, pstr) == 0)
							{
							if(pTable[i].run)
								{
								sprintf(buf, ERASESTATUS"\1error\1Cannot erase running partition\1");
								send_strmsg(buf);
								}
							else
								{
								sprintf(buf, ERASESTATUS"\1confirm\1%s\1", pTable[i].name);
								send_strmsg(buf);
								}
							break;
							}
						}
					}
				else if(strcmp(pstr, "confirmok") == 0) //erase 
					{
					pstr = strtok(NULL, "\1");
					const esp_partition_t *np = NULL;
					esp_partition_iterator_t pit = esp_partition_find(ESP_PARTITION_TYPE_ANY, ESP_PARTITION_SUBTYPE_ANY, NULL);
				    while(pit)
    					{
						np = esp_partition_get(pit);
						if(strcmp(np->label, pstr) == 0)
							{
							if(np->subtype == ESP_PARTITION_SUBTYPE_DATA_NVS)
								{
								ret = nvs_flash_deinit_partition(pstr);
								if(ret == ESP_OK)
									{
									ret = nvs_flash_erase_partition(pstr);
									if(ret == ESP_OK)
										nvs_flash_init_partition(pstr);
									}
								}
							else
								ret = esp_partition_erase_range(np, 0, np->size);
							ESP_LOGI(TAG, "Erasing partition %s", pstr, ret);
							}
						pit = esp_partition_next(pit);
						}
					if(ret == ESP_OK)
						sprintf(buf, "erasestatus\1progress\1Erase completed successfully\1");
					else
						sprintf(buf, "erasestatus\1progress\1error while erasing %s (%s)\1", pstr, esp_err_to_name(ret));
					send_strmsg(buf);
					}
				}
			else if(strcmp(pstr, CREATEKEY) == 0)
				{
				char ns[NVS_NS_NAME_MAX_SIZE];
				char key[NVS_KEY_NAME_MAX_SIZE];
				char *phv;
				pstr = strtok(NULL, "\1");
				if(pstr)
					{
					strcpy(ns, pstr);
					pstr = strtok(NULL, "\1");
					if(pstr)
						{
						strcpy(key, pstr);
						pstr = strtok(NULL, "\1");
						if(pstr)
							{
							type = atoi(pstr);	
							pstr = strtok(NULL, "\1");
							if(pstr)
								{
								len = atoi(pstr);
								pstr = strtok(NULL, "\1");
								if(pstr)
									{
									phv = malloc(len + 1);
									if(phv)
										{
										strcpy(phv, pstr);
										ret = create_nvs_key(NULL, ns, key, type, len, phv);
										if(ret == ESP_OK)
											sprintf(buf, "createkey\1ESP_OK\1New key created successfully\1");
										else
											sprintf(buf, "createkey\1ESP_FAIL\1%s\1", esp_err_to_name(ret));
										send_strmsg(buf);
										}
									}
								}
							}
						}
					}
				}
			else if(strcmp(pstr, UPD_REQUEST) == 0)
				{
				
				pstr = strtok(NULL, "\1");
				if(pstr)
					{
					sscanf(pstr, "[%d][%d]", &idxn, &idxk);
					pstr = strtok(NULL, "\1");
					if(pstr)
						{
						len = atoi(pstr);
						pstr = strtok(NULL, "\1");
						if(pstr)
							{
							nrc = atoi(pstr);
							rval.idxkey = idxk; rval.idxns = idxn; rval.len = len; rval.nr_cunks = nrc;
							rval.recvb = NULL; 
							xQueueSend(receive_q, &rval, 50);
							//recv_update(idxn, idxk, len, nrc);
							}
						}
					}
				}
			else if(strcmp(pstr, UPDATE_VAL) == 0)
				{
				//update val - token
				//id - 
				//chunk no
				//chunk length
				//payload
				pstr = strtok(NULL, "\1"); //id
				if(pstr)
					{
					sscanf(pstr, "[%d][%d]", &idxn, &idxk); //id
					ESP_LOGI(TAG, "%d  %d %s", idxn, idxk, pstr);
					pstr = strtok(NULL, "\1"); // length
					if(pstr)
						{
						chunk_nr = atoi(pstr); //chunk no
						pstr = strtok(NULL, "\1"); 
						if(pstr)
							{
							len = atoi(pstr); // chunk length 
							b = calloc(len, 1);
							if(b)
								{
								rval.idxkey = idxk; rval.idxns = idxn;
								rval.len = len;
								rval.recvb = b;
								rval.rcv_chunks = chunk_nr;
								memcpy(rval.recvb, pstr + strlen(pstr) + 1, rval.len);
								xQueueSend(receive_q, &rval, 50);
								}
							else
								ESP_LOGI(TAG, "ERROR - cannot allocate chunk memory");
							}
						}
					}
				}
			else if(strcmp(pstr, DELETE_NS) == 0)
				{
				pstr = strtok(NULL, "\1");
				if(pstr)
					{
					ret = erase_nvs_key(pstr, NULL);
					sprintf(buf, "delete key resp\1%s\1%d\1%s",pstr, ret, esp_err_to_name(ret));
					send_strmsg(buf);
					}
				}
			else if(strcmp(pstr, DELETE_KEY) == 0)
				{
				pstr = strtok(NULL, "\1");
				if(pstr)
					{
					sscanf(pstr, "[%d][%d]", &idxn, &idxk); //id
					ret = erase_nvs_key(namespace[idxn].name, nvskey[idxk].name);
					sprintf(buf, "delete key resp\1%s\1%d\1%s",nvskey[idxk].name, ret, esp_err_to_name(ret));
					send_strmsg(buf);
					}
				}
			else
				{
				ESP_LOGI(TAG, "unhandled message %s", pstr);
				}
			}
		}
	}
int create_ws_client_handler()
	{
	ws_msg_queue = xQueueCreate(10, sizeof(wsmsqg_t));
	if(!ws_msg_queue)
		{
		ESP_LOGE(TAG, "Cannot create ws_msg_queue");
		esp_restart();
		}
	if(xTaskCreate(ws_handler_task, "ws_cl_task", 8192, NULL, 5, NULL) != pdPASS)
		{
		ESP_LOGI(TAG, "Unable to create temp mon task");
		return ESP_FAIL;
		}
	return ESP_OK;
	}
