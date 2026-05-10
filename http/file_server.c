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

#include "esp_err.h"
#include "esp_log.h"

#include "esp_vfs.h"
//#include "esp_spiffs.h"
#include "esp_http_server.h"
#include "handlers.h"
#include "nvs_editor.h"
#include "ws_client_handler.h"
#include "nvsop.h"
#include "spiffsop.h"
#include "file_server.h"

/* Max length a file path can have on storage */
#define FILE_PATH_MAX (ESP_VFS_PATH_MAX + CONFIG_SPIFFS_OBJ_NAME_LEN)

/* Max size of an individual file. Make sure this
 * value is same as that set in upload_script.html */
//#define MAX_FILE_SIZE   (200*1024) // 200 KB
//#define MAX_FILE_SIZE_STR "200KB"

httpd_handle_t w_server;
static const char *TAG = "file_server";

static esp_err_t simple_get_handler(const uint8_t *start, const uint8_t *end, httpd_req_t *req);

extern const uint8_t _binary_main_html_start[] 		asm("_binary_main_html_start");
extern const uint8_t _binary_main_html_end[]		asm("_binary_main_html_end");

extern const uint8_t _binary_nvs_page_start[]		asm("_binary_nvseditor_html_start");
extern const uint8_t _binary_nvs_page_end[]			asm("_binary_nvseditor_html_end");

extern const uint8_t _binary_main_js_start[] 		asm("_binary_main_js_start");
extern const uint8_t _binary_main_js_end[]			asm("_binary_main_js_end");

extern const uint8_t _binary_nvs_js_start[] 		asm("_binary_nvs_js_start");
extern const uint8_t _binary_nvs_js_end[]			asm("_binary_nvs_js_end");

extern const uint8_t _binary_proto_js_start[] 		asm("_binary_appprotolegacy_js_start");
extern const uint8_t _binary_proto_js_end[]			asm("_binary_appprotolegacy_js_end");

extern const uint8_t _binary_favicon_ico_start[] 	asm("_binary_favicon_ico_start");
extern const uint8_t _binary_favicon_ico_end[]	 	asm("_binary_favicon_ico_end");

static const asset_t assets[] = 
	{
    {"/part",					_binary_main_html_start, 	_binary_main_html_end,		"text/html", 				root_get_handler},
    {"/js/main.js",				_binary_main_js_start, 		_binary_main_js_end,   		"application/javascript", 	simple_get_handler},
    {"/js/appprotolegacy.js",	_binary_proto_js_start, 	_binary_proto_js_end,  		"application/javascript", 	simple_get_handler},
    {"/js/nvs.js",				_binary_nvs_js_start, 		_binary_nvs_js_end,  		"application/javascript", 	simple_get_handler},
    { "/favicon.ico",			_binary_favicon_ico_start, 	_binary_favicon_ico_end,	"image/x-icon", 			simple_get_handler},
    { "/upload/",				NULL,						NULL,						"text/html", 				flashing_post_handler},
    { "/download/",				NULL,						NULL,						"text/html", 				dump_get_handler},
    { "/a",						NULL,						NULL,						"text/html", 				root_update_handler},
    { "/nvs",					_binary_nvs_page_start,		_binary_nvs_page_end,		"text/html", 				nvs_get_handler},
	};


esp_err_t generic_handler(httpd_req_t *req);

static esp_err_t simple_get_handler(const uint8_t *start, const uint8_t *end, httpd_req_t *req)
	{
    httpd_resp_send(req, (const char *)start, end - start);
    ESP_LOGI(TAG, "simple_get_handler()");
    return ESP_OK;
	}

/* Function to start the file server */
esp_err_t start_file_server(const char *base_path)
	{
    //static struct file_server_data *server_data = NULL;
/*
    if (server_data) 
    	{
        ESP_LOGE(TAG, "File server already started");
        return ESP_ERR_INVALID_STATE;
    	}

    // Allocate memory for server data 
    server_data = calloc(1, sizeof(struct file_server_data));
    if (!server_data) 
    	{
        ESP_LOGE(TAG, "Failed to allocate memory for server data");
        return ESP_ERR_NO_MEM;
    	}
    strlcpy(server_data->base_path, base_path,
            sizeof(server_data->base_path));
*/
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.stack_size = 8192;

    /* Use the URI wildcard matching function in order to
     * allow the same handler to respond to multiple different
     * target URIs which match the wildcard scheme */
    config.uri_match_fn = httpd_uri_match_wildcard;
    config.max_uri_handlers = 20;
    config.recv_wait_timeout = 10;
    config.send_wait_timeout = 10;
    config.lru_purge_enable = true;

    ESP_LOGI(TAG, "Starting HTTP Server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) != ESP_OK) 
    	{
		//free(server_data);
    	//server_data = NULL;
        ESP_LOGE(TAG, "Failed to start file server!");
        return ESP_FAIL;
    	}
    create_ws_client_handler();
	w_server = server;

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
/*
	httpd_uri_t root = 
		{
	    .uri       = "/",
	    .method    = HTTP_GET,
	    .handler   = root_get_handler,
	    .user_ctx = &server_data,
		};
	httpd_register_uri_handler(server, &root);
*/
	/*
	static const httpd_uri_t main = 
		{
	    .uri       = "/main",
	    .method    = HTTP_POST,
	    .handler   = main_post_handler
		};
	httpd_register_uri_handler(server, &main);
	*/
/*	
	httpd_uri_t roota = 
		{
	    .uri       = "/a",
	    .method    = HTTP_POST,
	    .handler   = root_update_handler,
	    .user_ctx = &server_data,
		};
//	httpd_register_uri_handler(server, &roota);
*/
/*	
	static const httpd_uri_t setboot = 
		{
	    .uri       = "/sb",
	    .method    = HTTP_POST,
	    .handler   = set_boot_handler
		};
	httpd_register_uri_handler(server, &setboot);
*/

    /* URI handler for getting uploaded files */
/*    
    httpd_uri_t file_download = {
        .uri       = "/download/",  // Match all URIs of type /path/to/file
        .method    = HTTP_GET,
        .handler   = dump_get_handler,
        .user_ctx  = &server_data    // Pass server data as context
    };
//    httpd_register_uri_handler(server, &file_download);
*/
    /* URI handler for uploading files to server */
/*    
    httpd_uri_t file_upload = {
        .uri       = PART_UPLOAD"*",   // Match all URIs of type /upload/path/to/file
        .method    = HTTP_POST,
        .handler   = flashing_post_handler,
        .user_ctx  = &server_data    // Pass server data as context
    };
//    httpd_register_uri_handler(server, &file_upload);
*/  
    /* URI handler for nvs editor */
/*    
    httpd_uri_t nvs_editor = {
        .uri       = "/nvs_editor.html",   
        .method    = HTTP_GET,
        .handler   = nvs_get_handler,
        .user_ctx  = &server_data    // Pass server data as context
    };
//    httpd_register_uri_handler(server, &nvs_editor);
*/
    /* URI handler for getting nvs blob key */
    httpd_uri_t nvsk_download = {
        .uri       = NVSK_DOWNLOAD"*",  // Match all URIs of type /path/to/file
        .method    = HTTP_GET,
        .handler   = nvskey_get_handler,
        .user_ctx  = &server_data    // Pass server data as context
    };
//    httpd_register_uri_handler(server, &nvsk_download);
    
    /* URI handler for spiffs editor */
    httpd_uri_t spiffs_editor = {
        .uri       = "/spiffs_editor.html",   
        .method    = HTTP_GET,
        .handler   = spiffs_get_handler,
        .user_ctx  = &server_data    // Pass server data as context
    };
//    httpd_register_uri_handler(server, &spiffs_editor);
    
    /* URI handler for favicon*/
/*    
	httpd_uri_t favicon = {
	    .uri     = "/favicon.ico",
	    .method  = HTTP_ANY,
	    .handler = favicon_get_handler,
	    .user_ctx  = &server_data    // Pass server data as context
	};
//	httpd_register_uri_handler(server, &favicon);
*/
#if 0	
	/* URI handler for uloading NVS BLOBs*/
	httpd_uri_t nvskup = {
	    .uri     = NVSK_UPLOAD"*",
	    .method  = HTTP_POST,
	    .handler = nvskey_upload_handler,
	    .user_ctx  = &server_data    // Pass server data as context
	};
	httpd_register_uri_handler(server, &nvskup);
#endif
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