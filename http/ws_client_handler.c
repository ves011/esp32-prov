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
#include <string.h>
#include <sys/param.h>
#include "cmd_wifi.h"
#include "esp_err.h"
#include "esp_wifi.h"
#include <esp_wifi_types_generic.h>
#include "freertos/idf_additions.h"
#include "file_server.h"
#include "app_proto.h"
#include "ws_client_handler.h"
#include "../handlers/nvsop.h"
#include "lwip/netif.h"
#include "part_editor.h"
#include "protocoldef.h"

static char *TAG = "WS_CLIENT";
QueueHandle_t ws_msg_queue;

static void dispatch_command(app_proto_t *msg);
static void send_app_proto(app_proto_t *msg);

static void handle_setboot(app_proto_t *msg);
static void handle_erase_part(app_proto_t *msg);
static void handle_response_conf(app_proto_t *msg);
static void handle_reboot_req(app_proto_t *msg);
static void handle_update_key_req(app_proto_t *msg);
static void handle_update_key_val(app_proto_t *msg);
static void handle_update_key(app_proto_t *msg);
static void handle_create_key(app_proto_t *msg);
static void handle_delete_ns(app_proto_t *msg);
static void handle_delete_key(app_proto_t *msg);
static void handle_devinfo(app_proto_t *msg);
static void handle_synctime(app_proto_t *msg);
static void check_wifi_state();

static const cmd_entry_t cmd_table[] = {
    { CMD_SETBOOT,        	handle_setboot },
    { CMD_ERASEPART,      	handle_erase_part },
    { RSP_CONFIRMATION,     handle_response_conf },
    { CMD_REBOOT,     		handle_reboot_req },
    { CMD_UPDATEKEYREQ,   	handle_update_key_req },
    { CMD_UPDATEKEYVAL,   	handle_update_key_val },
    { CMD_UPDATEKEY,      	handle_update_key },
    { CMD_CREATEKEY,      	handle_create_key },
    { CMD_DELTENS,        	handle_delete_ns },
    { CMD_DELETEKEY,      	handle_delete_key },
    { URC_DEVINFO,      	handle_devinfo },
    { CMD_SYNCTIME,      	handle_synctime },
	};
#define CMD_TABLE_SIZE sizeof(cmd_table) / sizeof(cmd_table[0])

static char ssid[32];
static uint32_t apip, staip, starssi;

void send_ws_txtframe(char *msg)
	{
	int ret;
    httpd_ws_frame_t ws_pkt;
    memset(&ws_pkt, 0, sizeof(httpd_ws_frame_t));
    ws_pkt.payload = (uint8_t*)msg;
    ws_pkt.len = strlen(msg);
    ws_pkt.type = HTTPD_WS_TYPE_TEXT;
    if(wsfd)
    	{
    	ret = httpd_ws_send_frame_async(w_server, wsfd, &ws_pkt);
    	ESP_LOGI(TAG, "ws_send_frame_async: %d / %s", ret, msg);
    	}
    else
    	ESP_LOGI(TAG, "ws_send_frame_async error: wsfd = 0");
	}
/*	
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
*/
void ws_handler_task(void *pvParameters)
	{
	wsmsg_t msg;
	char buf[128];
	app_proto_t proto;
	while(1)
		{
		if(xQueueReceive(ws_msg_queue, &msg, portMAX_DELAY))
			{
			strncpy(buf, msg.payload.strpayload, 32);
			buf[32] = 0;
			ESP_LOGI(TAG, "websocket message fd:%d message: %s", msg.fd, buf);
			int ret = parse_app_proto((uint8_t *)msg.payload.strpayload,
                          msg.len,
                          &proto);
			if(ret == 0)
				dispatch_command(&proto);
			}
				
		}
	}

int create_ws_client_handler()
	{
	ws_msg_queue = xQueueCreate(10, sizeof(wsmsg_t));
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
	
static void handle_update_key(app_proto_t *msg)
	{
	int idxn, idxk;
	sscanf(msg->params[0], "%d_%d", &idxn, &idxk);
	if(msg->nparams < 2)
		{
		ESP_LOGI(TAG, "malformated websocket message");
		return;
		}
	int ret = nvs_update_key(atoi(msg->params[1]), idxn, idxk,msg->payload_len, msg->payload);
	send_rsp_cmd(msg, ret, NULL);	
	}
static void handle_update_key_req(app_proto_t *msg) //init rcv_kayval for key update
	{
	int in, ik;
	errrep_t errrep = {0};
	sscanf(msg->params[0], "%d_%d", &in, &ik);
	if(msg->nparams < 3)
		{
		ESP_LOGI(TAG, "malformated websocket message");
		return;
		}
	int ret = init_update_key(in, ik, atoi(msg->params[1]), atoi(msg->params[2]), &errrep);
	send_rsp_cmd(msg, ret, errrep.errmsg);
	}
static void handle_update_key_val(app_proto_t *msg) //update key with the chunks
	{
	int in, ik;
	errrep_t errrep = {0};
	sscanf(msg->params[0], "%d_%d", &in, &ik);
	if(msg->nparams < 3)
		{
		ESP_LOGI(TAG, "malformated websocket message");
		return;
		}
	int ret = update_key_chunk(in, ik, atoi(msg->params[1]), atoi(msg->params[2]), msg->payload, &errrep);
	if(ret == 1) //it should be safe because error values are > ESP_ERR_NVS_BASE (0x1100)
		ws_send_status(OP_UPDATEKEY, PAR_PROGRESS, 1, msg->params[0]);
	else if(ret != ESP_OK)
		send_rsp_cmd(msg, ret, errrep.errmsg);
	}
static void handle_delete_ns(app_proto_t *msg)
	{
	if(msg->nparams < 1)
		{
		ESP_LOGI(TAG, "malformated websocket message");
		return;
		}
	int ret = erase_nvs_key(atoi(msg->params[0]), -1);
	send_rsp_cmd(msg, ret, NULL);
	}
static void handle_delete_key(app_proto_t *msg)
	{
	if(msg->nparams < 1)
		{
		ESP_LOGI(TAG, "malformated websocket message");
		return;
		}
	int ret = erase_nvs_key(atoi(msg->params[0]), atoi(msg->params[1]));
	send_rsp_cmd(msg, ret, NULL);
	}
static void handle_create_key(app_proto_t *msg)
	{
	int type, len, ret;
	if(msg->nparams < 4)
		{
		ESP_LOGI(TAG, "malformated websocket message");
		return;
		}
	type = atoi(msg->params[2]);
	len = atoi(msg->params[3]);
	char *phv = calloc(1, msg->payload_len + 2);
	if(phv)
		{
		memcpy(phv, msg->payload, msg->payload_len);
		ret = create_nvs_key(NULL, msg->params[1], msg->params[0], type, len, phv);
		send_rsp_cmd(msg, ret, NULL);
		free(phv);
		}
	else
		send_rsp_cmd(msg, 0, "cannot allocate memory for placeholder value");
	}
static void handle_setboot(app_proto_t *msg)
	{
	int ret;
	char ws_buf[256], berrtxt[40], berr[40];
	int ws_buf_len = 0;
	if(msg->nparams < 1)
		{
		ESP_LOGI(TAG, "malformated websocket message");
		return;
		}
	app_proto_t rmsg;
	rmsg.version = PROTO_VERSION;
	rmsg.hdr_fields = 9;
	rmsg.payload_len = 0;
	rmsg.timestamp = time(NULL);
	ret = set_bp(msg->params[0]);
	rmsg.command = RSP_CMD;
	rmsg.nparams = 4;
	rmsg.params[0] = msg->command;
	rmsg.params[1] = msg->params[0];
	if(ret != ESP_OK)
		{
		sprintf(berrtxt, "%s", esp_err_to_name(ret));
		sprintf(berr, "%d", ret);
		}
	else
		{
		strcpy(berrtxt,  "N/A");
		strcpy(berr, "0");
		}
	rmsg.params[2] = berr;
	rmsg.params[3] = berrtxt;
	if(!build_app_proto((uint8_t *)ws_buf, 256, &rmsg, &ws_buf_len))
		send_ws_txtframe(ws_buf);
	}
static void handle_erase_part(app_proto_t *msg)
	{
	char ws_buf[256];
	int ws_buf_len = 0;
	app_proto_t rmsg;
	if(msg->nparams < 1)
		{
		ESP_LOGI(TAG, "malformated websocket message");
		return;
		}
	ESP_LOGI(TAG, "erase partition: %s", msg->params[0]);
	rmsg.version = PROTO_VERSION;
	rmsg.payload_len = 0;
	rmsg.timestamp = time(NULL);
	for(int i = 0; i < npart; i++)
		{
		if(strcmp(pTable[i].name, msg->params[0]) == 0)
			{
			if(pTable[i].run)
				{
				rmsg.command = RSP_CMD;
				rmsg.hdr_fields = 9;
				rmsg.params[0] = msg->command;
				rmsg.params[1] = msg->params[0];
				rmsg.params[2] = "1";
				rmsg.params[3] = "Cannot erase running partition";
				rmsg.nparams = 4;
				if(!build_app_proto((uint8_t *)ws_buf, 256, &rmsg, &ws_buf_len))
					send_ws_txtframe(ws_buf);
				}
			else
				{
				rmsg.command = CMD_REQCONF;
				rmsg.hdr_fields = 7;
				rmsg.params[0] = msg->command;
				rmsg.params[1] = msg->params[0];
				rmsg.nparams = 2;
				if(!build_app_proto((uint8_t *)ws_buf, 256, &rmsg, &ws_buf_len))
					send_ws_txtframe(ws_buf);
				}
			break;
			}
		}
	}
static void handle_response_conf(app_proto_t *msg)
	{
	int ret = ESP_FAIL, ws_buf_len;
	app_proto_t rmsg;
	char btmp[128], berr[20], ws_buf[256];
	if(msg->nparams < 3)
		{
		ESP_LOGI(TAG, "malformated websocket message");
		return;
		}
//	RSP_CONFIRMATION for CMD_ERASEPART ---------------- start -----------------------------------
	if(strcmp(msg->params[0], CMD_ERASEPART) == 0 && msg->nparams == 3)
		{
		if(strcmp(msg->params[2], "OK") == 0)
			{
			const esp_partition_t *np = NULL;
			esp_partition_iterator_t pit = esp_partition_find(ESP_PARTITION_TYPE_ANY, ESP_PARTITION_SUBTYPE_ANY, NULL);
			while(pit)
    			{
				np = esp_partition_get(pit);
				if(np && strcmp(np->label, msg->params[1]) == 0)
					{
					if(np->subtype == ESP_PARTITION_SUBTYPE_DATA_NVS)
						{
						ret = nvs_flash_deinit_partition(msg->params[1]);
						if(ret == ESP_OK)
							{
							ret = nvs_flash_erase_partition(msg->params[1]);
							if(ret == ESP_OK)
								nvs_flash_init_partition(msg->params[1]);
							}
						}
					else
						ret = esp_partition_erase_range(np, 0, np->size);
					
					ESP_LOGI(TAG, "Erasing partition result %s", msg->params[1], ret);						
					break;
					}
				pit = esp_partition_next(pit);
				}
			esp_partition_iterator_release(pit);
			rmsg.version = PROTO_VERSION;
			rmsg.hdr_fields = 9;
			rmsg.timestamp = time(NULL);
			rmsg.payload_len = 0;
			rmsg.command = RSP_CMD;
			rmsg.params[0] = msg->command;
			rmsg.params[1] = msg->params[0];
			if(ret == ESP_OK)
				{
				rmsg.params[2] = "0";
				sprintf(btmp, "Partition \"%s\" erased successfully", msg->params[1]);
				rmsg.params[3] = btmp;
				}
			else
				{
				sprintf(berr, "%d", ret);
				rmsg.params[2] = berr;
				sprintf(btmp, "Partition \"%s\" erase error\n%s", msg->params[1], esp_err_to_name(ret));
				rmsg.params[3] = btmp;
				}
			rmsg.nparams = 4;
			ESP_LOGI(TAG, "%s", btmp);
			if(!build_app_proto((uint8_t *)ws_buf, 256, &rmsg, &ws_buf_len))
				send_ws_txtframe(ws_buf);
			}
		}
//	RSP_CONFIRMATION for CMD_ERASEPART ---------------- end ----------------------		
	}
static void handle_reboot_req(app_proto_t *msg)
	{
	app_proto_t rmsg;
	char ws_buf[256];
	int ws_buf_len = 0;
	restart_in_progress = 1;
	rmsg.version = PROTO_VERSION;
	rmsg.hdr_fields = 9;
	rmsg.timestamp = time(NULL);
	rmsg.payload_len = 0;
	rmsg.command = RSP_CMD;
	rmsg.nparams = 4;
	rmsg.params[0] = msg->command;
	rmsg.params[1] = "";
	rmsg.params[2] = "";
	rmsg.params[3] = "";
	if(!build_app_proto((uint8_t *)ws_buf, 256, &rmsg, &ws_buf_len))
		send_ws_txtframe(ws_buf);
	}	


static void dispatch_command(app_proto_t *msg)
	{
    for (int i = 0; i < CMD_TABLE_SIZE; i++)
    	{
        if (strcmp(msg->command, cmd_table[i].cmd) == 0)
        	{
            cmd_table[i].handler(msg);
            return;
        	}
    	}

    ESP_LOGI(TAG, "Unhandled command: %s", msg->command);
	}


	
void ws_send_status(const char *op, const char *status, int err,  const char *txt)
	{
    app_proto_t msg;
    memset(&msg, 0, sizeof(msg));
    msg.version = PROTO_VERSION;
    msg.hdr_fields = 9;
    msg.payload_len = 0;
    msg.command = URC_STATUS;
    msg.nparams = 4;
    msg.params[0] = (char *)op;
    msg.params[1] = (char *)status;

    char berr[16];
	snprintf(berr, sizeof(berr), "%d", err);
    msg.params[2] = berr;
    msg.params[3] = (char *)txt;
    send_app_proto(&msg);
	}

void send_rsp_cmd(app_proto_t *msgIn, int err, char *errtxt)
	{
	char berr[10], berrtxt[32];
	app_proto_t msgOut;
	msgOut.version = PROTO_VERSION;
	msgOut.hdr_fields = 9;
	msgOut.payload_len = 0;
	msgOut.timestamp = time(NULL);
	msgOut.command = RSP_CMD;
	msgOut.nparams = 4;
	msgOut.params[0] = msgIn->command;
	msgOut.params[1] = (msgIn->nparams > 0) ? msgIn->params[0] : "";
	sprintf(berr, "%d", err);
	msgOut.params[2] = berr;
	if(errtxt)
		msgOut.params[3] = errtxt;
	else
		{
		sprintf(berrtxt, "%s", esp_err_to_name(err));
		msgOut.params[3] = berrtxt;
		}
	send_app_proto(&msgOut);
	}
void send_devinfo(const char *info_type, const char *info_val)
	{
	app_proto_t msgOut;	
	msgOut.version = PROTO_VERSION;
	msgOut.hdr_fields = 7;
	msgOut.payload_len = 0;
	msgOut.timestamp = time(NULL);
	msgOut.command = URC_DEVINFO;
	msgOut.nparams = 2;
	msgOut.params[0] = (char *)info_type;
	msgOut.params[1] = (char *)info_val;
	send_app_proto(&msgOut);
	}
static void send_app_proto(app_proto_t *msg)
	{
	char ws_buf[MAX_LEN_PROTO_MSG];
	int ws_buf_len = 0;
	if(!build_app_proto((uint8_t *)ws_buf, MAX_LEN_PROTO_MSG, msg, &ws_buf_len))
		send_ws_txtframe(ws_buf);
	}
	
static void handle_devinfo(app_proto_t *msg)
	{
	char ws_buf[MAX_LEN_PROTO_MSG];
	int ws_buf_len = 0;
	if(strcmp(msg->params[0], PAR_DEVTIME) == 0)
		{
		time_t ts = time(NULL);
		char btime[20];
		snprintf(btime, sizeof(btime), "%llu", ts);
		msg->params[1] = btime;
		if(!build_app_proto((uint8_t *)ws_buf, MAX_LEN_PROTO_MSG, msg, &ws_buf_len))
			send_ws_txtframe(ws_buf);
		}
	else if(strcmp(msg->params[0], PAR_WIFI) == 0)
		check_wifi_state();
	}

static void check_wifi_state()
	{
	char capip[32], cstaip[32], crssi[32];
	esp_err_t err;
	wifi_mode_t mode;
	wifi_ap_record_t ap_info;
	esp_netif_t *netif;
	esp_netif_ip_info_t ip_info;
	if(wsfd == 0)
		return;
		
	err = esp_wifi_get_mode(&mode);
	if(err == ESP_ERR_WIFI_NOT_INIT)
		{
		//no connection don't bother to send any status
		return;
		}
	err = esp_wifi_sta_get_ap_info(&ap_info);
	if(err == ESP_OK)
		{
		//if(strcmp((char *)ap_info.ssid, ssid))
			{
			send_devinfo(PAR_STASSID, (char *)ap_info.ssid);
			strncpy(ssid, (char *)ap_info.ssid, sizeof(ssid) - 1);
			ssid[sizeof(ssid) - 1] = 0;
			}
		if(starssi != ap_info.rssi)
			{
			sprintf(crssi, "%d", ap_info.rssi);
			send_devinfo(PAR_STARSSI, crssi);
			starssi = ap_info.rssi;
			}
		}
	else if(err == ESP_ERR_WIFI_NOT_CONNECT)
		{
		if(strcmp(ssid, "not connected"))
			{
			strcpy(ssid, "not connected");
			send_devinfo(PAR_STASSID, ssid);
			strcpy(cstaip, "not connected");
			send_devinfo(PAR_STAIP, cstaip);
			strcpy(crssi, "not connected");
			send_devinfo(PAR_STARSSI, crssi);
			}
		}
	netif = NULL;
	strcpy(capip, "not available");
	do
		{
		netif = esp_netif_next_unsafe(netif);
		if(!netif)
			break;
		err = esp_netif_get_ip_info(netif, &ip_info);
		if(err == ESP_OK)
			{
			if(strcmp(esp_netif_get_desc(netif),"ap") == 0)
				{
				sprintf(capip, IPSTR, IP2STR(&ip_info.ip));
				apip = ip_info.ip.addr;
				send_devinfo(PAR_APIP, capip);
				}
			else if(strcmp(esp_netif_get_desc(netif),"sta") == 0)
				{
				sprintf(cstaip, IPSTR, IP2STR(&ip_info.ip));
				staip = ip_info.ip.addr;
				send_devinfo(PAR_STAIP, cstaip);
				}
			}
		} while (netif);
	}
void reset_wifi_state()
	{
	ssid[0] = 0;
	apip = staip = starssi = 0;
	return;
	}
static void handle_synctime(app_proto_t *msg)
	{
	struct timeval tv;
	ESP_LOGI(TAG, "sync time");
	tv.tv_sec = msg->timestamp / 1000;
	tv.tv_usec = (msg->timestamp % 1000) * 1000;
	settimeofday(&tv, NULL);
	}