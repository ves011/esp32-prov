/*
 * handlers.h
 *
 *  Created on: Feb 22, 2026
 *      Author: viorel_serbu
 */

#ifndef MAIN_WS_CLIENT_HANDLER_H_
#define MAIN_WS_CLIENT_HANDLER_H_

#include "esp_http_server.h"
#define MSG_RECEIVED		1
#define MSG_2SEND			2

#define RCV_CHUNK_SIZE		512

#define SETBOOT 	"setBoot"		// set boot partition
#define USTATUS 	"ustatus"		// upload partition image status
#define DSTATUS 	"dstatus"		// dump partition image status
#define ERASE		"erase"			// erase partition

#define CREATEKEY		"createkey"		// create NVS key/value pair 
#define UPD_REQUEST		"update req"
#define SEND_VAL		"send val"
#define UPDATE_VAL		"update val"
#define DELETE_NS		"delete ns"
#define DELETE_KEY		"delete key"
#define DELETE_KEY_RESP	"delete key resp"



typedef struct
	{
	int fd;
	int len;
	union
		{
		char strpayload[1024];
		uint8_t binpaiload[1024];
		} payload;
	} wsmsqg_t;
	
struct async_resp_arg {
    httpd_handle_t hd;
    int fd;
    char *payload;
};

extern QueueHandle_t ws_msg_queue;
int create_ws_client_handler();
void send_strmsg(char *msg);
void send_binmsg(char *msg, int len);


#endif /* MAIN_WS_CLIENT_HANDLER_H_*/
