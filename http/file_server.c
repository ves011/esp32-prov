/* 
	HTTP File Server
*/

#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/param.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include <dirent.h>

#include "app_proto.h"
#include "esp_err.h"
#include "esp_log.h"

#include "esp_vfs.h"
//#include "esp_spiffs.h"
#include "esp_http_server.h"
#include "freertos/idf_additions.h"
#include "cmd_wifi.h"
#include "protocoldef.h"
#include "ws_client_handler.h"
#include "keep_alive.h"
#include "file_server.h"
#include "nvs_editor.h"
#include "nvsop.h"
#include "spiffsop.h"
#include "part_editor.h"

/* Max length a file path can have on storage */
#define FILE_PATH_MAX (ESP_VFS_PATH_MAX + CONFIG_SPIFFS_OBJ_NAME_LEN)

httpd_handle_t w_server;
int wsfd;
struct file_server_data server_data;

static const char *TAG = "file_server";

static esp_err_t simple_get_handler(const uint8_t *start, const uint8_t *end, httpd_req_t *req);

extern const uint8_t _binary_main_html_start[] 			asm("_binary_main_html_start");
extern const uint8_t _binary_main_html_end[]			asm("_binary_main_html_end");

extern const uint8_t _binary_nvs_page_start[]			asm("_binary_nvseditor_html_start");
extern const uint8_t _binary_nvs_page_end[]				asm("_binary_nvseditor_html_end");

extern const uint8_t _binary_main_js_start[] 			asm("_binary_main_js_start");
extern const uint8_t _binary_main_js_end[]				asm("_binary_main_js_end");

extern const uint8_t _binary_nvs_js_start[] 			asm("_binary_nvs_js_start");
extern const uint8_t _binary_nvs_js_end[]				asm("_binary_nvs_js_end");

extern const uint8_t _binary_protocoldef_js_start[] 	asm("_binary_protocoldef_js_start");
extern const uint8_t _binary_protocoldef_js_end[]		asm("_binary_protocoldef_js_end");

extern const uint8_t _binary_hexed_js_start[] 			asm("_binary_hexed_js_start");
extern const uint8_t _binary_hexed_js_end[]				asm("_binary_hexed_js_end");

extern const uint8_t _binary_proto_js_start[] 			asm("_binary_appprotolegacy_js_start");
extern const uint8_t _binary_proto_js_end[]				asm("_binary_appprotolegacy_js_end");

extern const uint8_t _binary_favicon_ico_start[] 		asm("_binary_favicon_ico_start");
extern const uint8_t _binary_favicon_ico_end[]	 		asm("_binary_favicon_ico_end");
/*
 * because the way URI is parsed in generic_handler(), the rule below is critical:
 * !!!the URI string in the assets SHALL NOT be part or included of any other URI string!!!  
*/
static const asset_t assets[] = 
	{
    {"/part",					_binary_main_html_start,		_binary_main_html_end,		"text/html", 				part_get_handler},
    {"/js/main.js",	_binary_main_js_start,	_binary_main_js_end,"application/javascript",simple_get_handler},
    {"/js/appprotolegacy.js",	_binary_proto_js_start, 		_binary_proto_js_end,  		"application/javascript", 	simple_get_handler},
    {"/js/nvs.js",				_binary_nvs_js_start, 			_binary_nvs_js_end,  		"application/javascript", 	simple_get_handler},
    {"/js/hexed.js",			_binary_hexed_js_start, 		_binary_hexed_js_end,  		"application/javascript", 	simple_get_handler},
    {"/js/protocoldef.js",		_binary_protocoldef_js_start, 	_binary_protocoldef_js_end,	"application/javascript", 	simple_get_handler},
    { "/favicon.ico",			_binary_favicon_ico_start, 		_binary_favicon_ico_end,	"image/x-icon", 			simple_get_handler},
    { "/upload/",				NULL,							NULL,						"text/html", 				flashing_post_handler},
    { "/download/",				NULL,							NULL,						"text/html", 				dump_get_handler},
    { "/a",						NULL,							NULL,						"text/html", 				part_update_handler},
    { "/nvs",					_binary_nvs_page_start,			_binary_nvs_page_end,		"text/html", 				nvs_get_handler},
    { "/keydump/",				NULL,							NULL,						"application/octet-stream",	nvskey_get_handler},
	};

esp_err_t generic_handler(httpd_req_t *req);

static void dev_timer_callback(void* arg)
	{
	//wsmsg_t *msgw = (wsmsg_t *)arg;
	static wsmsg_t msgw;
	app_proto_t msg;
	time_t t = time(NULL);
	if(t % 60 == 0)
		{
	    memset(&msg, 0, sizeof(msg));
	    memset(&msgw, 0, sizeof(wsmsg_t));
	    msg.version = PROTO_VERSION;
	    msg.hdr_fields = 7;
	    msg.payload_len = 0;
	    msg.command = URC_DEVINFO;
	    msg.nparams = 2;
	    msg.params[0] = PAR_DEVTIME;
	    msg.params[1] = "";
	    if(!build_app_proto(msgw.payload.binpayload, MAX_LEN_PROTO_MSG, &msg, &msgw.len))
			xQueueSend(ws_msg_queue, &msgw, 0);
		}
	if(t % 10 == 0)
		{
		//check wifi state
	    memset(&msg, 0, sizeof(msg));
	    memset(&msgw, 0, sizeof(wsmsg_t));
	    msg.version = PROTO_VERSION;
	    msg.hdr_fields = 7;
	    msg.payload_len = 0;
	    msg.command = URC_DEVINFO;
	    msg.nparams = 2;
	    msg.params[0] = PAR_WIFI;
	    msg.params[1] = "";
	    if(!build_app_proto(msgw.payload.binpayload, MAX_LEN_PROTO_MSG, &msg, &msgw.len))
			xQueueSend(ws_msg_queue, &msgw, 0);
		}
	}

static esp_err_t simple_get_handler(const uint8_t *start, const uint8_t *end, httpd_req_t *req)
	{
    httpd_resp_send(req, (const char *)start, end - start);
    ESP_LOGI(TAG, "simple_get_handler()");
    return ESP_OK;
	}

/* Function to start the file server */
esp_err_t start_file_server(const char *base_path)
	{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    //static wsmsg_t wsmsg;
    config.stack_size = 8192;

    config.uri_match_fn = httpd_uri_match_wildcard;
    config.max_uri_handlers = 20;
    config.recv_wait_timeout = 10;
    config.send_wait_timeout = 10;
    config.lru_purge_enable = true;

    ESP_LOGI(TAG, "Starting HTTP Server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) != ESP_OK) 
    	{
        ESP_LOGE(TAG, "Failed to start file server!");
        return ESP_FAIL;
    	}
    create_ws_client_handler();
	w_server = server;
	esp_timer_handle_t dev_timer;
	esp_timer_create_args_t dev_timer_args = 
		{
    	.callback = &dev_timer_callback,
    	//.arg = &wsmsg,
        .name = "dev_timer"
    	};
    ESP_ERROR_CHECK(esp_timer_create(&dev_timer_args, &dev_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(dev_timer, 1000000));
	static const httpd_uri_t ws = {
        .uri        = "/ws",
        .method     = HTTP_GET,
        .handler    = ws_handler,
        .user_ctx   = NULL,
        .is_websocket = true,
        .handle_ws_control_frames = true
		};
	httpd_register_uri_handler(server, &ws);
	
	httpd_uri_t uri = 
		{
    	.uri = "/*",
    	.method = HTTP_ANY,
    	.handler = generic_handler,
    	.user_ctx = &server_data,
		};
	httpd_register_uri_handler(server, &uri);
    
    return ESP_OK;
	}

esp_err_t generic_handler(httpd_req_t *req)
	{
	char turi[CONFIG_HTTPD_MAX_URI_LEN + 1];
	ESP_LOGI(TAG, "URI: %s", req->uri);
	strcpy(turi, req->uri);
	if(strcmp(turi, "/") == 0)		//if uri = / go to /part
		strcpy(turi, "/part");
    for (int i = 0; i < sizeof(assets)/sizeof(assets[0]); i++) 
    	{
		if(strstr(turi, assets[i].uri) == turi)
        	{
			httpd_resp_set_type(req, assets[i].type);
			return 
				assets[i].page_handler(assets[i].start, assets[i].end, req);
	        }
    	}
    httpd_resp_send_404(req);
    return ESP_FAIL;
	}

esp_err_t ws_handler(httpd_req_t *req)
	{
	wsmsg_t msg;
    if (req->method == HTTP_GET) 
    	{
        ESP_LOGI(TAG, "Handshake done, the new connection was opened");
        ESP_LOGI(TAG, "HTTP_GET ws_handler: httpd_handle_t=%p, sockfd=%d, client_info:%d", 
         			req->handle,
                 	httpd_req_to_sockfd(req), 
                 	httpd_ws_get_fd_info(req->handle, httpd_req_to_sockfd(req)));
        if(httpd_ws_get_fd_info(req->handle, httpd_req_to_sockfd(req)) == 2)
        	{
			int fd = httpd_req_to_sockfd(req);
			if(wsfd != 0 && wsfd != fd)
				{
				ESP_LOGI(TAG, "Rejecting second client");
				return ESP_FAIL;
				}
        	wsfd = httpd_req_to_sockfd(req);
        	app_proto_t msgproto;
        	memset(&msg, 0, sizeof(wsmsg_t));
	    	memset(&msgproto, 0, sizeof(app_proto_t));
	    	msgproto.version = PROTO_VERSION;
		    msgproto.hdr_fields = 7;
		    msgproto.payload_len = 0;
		    msgproto.command = URC_DEVINFO;
		    msgproto.nparams = 2;
		    
		    msgproto.params[0] = PAR_DEVTIME;
		    msgproto.params[1] = "";
		    if(!build_app_proto(msg.payload.binpayload, MAX_LEN_PROTO_MSG, &msgproto, &msg.len))
				xQueueSend(ws_msg_queue, &msg, 0);
			
			msgproto.params[0] = PAR_WIFI;
		    msgproto.params[1] = "";
		    if(!build_app_proto(msg.payload.binpayload, MAX_LEN_PROTO_MSG, &msgproto, &msg.len))
				xQueueSend(ws_msg_queue, &msg, 0);
        	}
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
			size_t copy_len = MIN(ws_pkt.len, sizeof(msg.payload.binpayload) - 1);
			memcpy(msg.payload.strpayload, ws_pkt.payload, copy_len);
			msg.payload.strpayload[copy_len] = '\0';
            
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
            int fd = httpd_req_to_sockfd(req);
            ESP_LOGI(TAG, "websocket closed %d / %d", wsfd, fd);
            if (wsfd == fd)
            	{
        		wsfd = 0;
        		reset_wifi_state();
        		}
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