/*
 * nvsop.h
 *
 *  Created on: Mar 3, 2026
 *      Author: viorel_serbu
 */

#ifndef MAIN_NVSOP_H_
#define MAIN_NVSOP_H_

#include "esp_http_server.h"
#include "ws_client_handler.h"
#include "nvs.h"

#define MAX_CONCURRENT_UPDATES 10

#define UPDATE_NOTUSED		0
#define UPDATE_READY		1
#define UPDATE_INPROGRESS	2
#define UPDATE_COMPLETE		3

typedef struct
	{
	char name[NVS_NS_NAME_MAX_SIZE];
	int nentries;
	} namespace_t;
typedef struct
	{
	char name[NVS_KEY_NAME_MAX_SIZE];
	int ns_idx;
	int type;
	size_t size;
	} nvskey_t;
	
typedef struct
	{
	int idxns;
	int idxkey;
	int type;
	int len;
	int rcvlen;
	int state;
	int nr_cunks;
	int rcv_chunks;
	uint8_t *recvb;
	} rcv_keyval_t;
	
typedef struct
	{
	int ns;
	int key;
	int len;
	int nrc;
	} update_req_t;

extern namespace_t *namespace;
extern nvskey_t *nvskey;
extern int nns, nkeys;
//extern char nvs_sel[16];			//populated by nvs_get_entries
extern QueueHandle_t receive_q;
void register_nvsop(void);
int get_nvs_entries(char *pName);
int create_nvs_key(char *pName, char *ns, char *key, int type, int len, char *phv);
int recv_update(int idn, int idk, int len, int nrc);
void nvs_update_task(void *pvParameters);
//int update_keyval(int idxn, int idxk, void *pstr);
//int set_nvs_value(int idxkey, void *val);
int nvs_set_val(int type, nvs_handle_t handle, char *name, int len, void *val);
int nvs_update_key(int type, int nsidx, int keyidx, int len, void *val);
int erase_nvs_key(int nsID, int keyID);
int nvskey_get_handler(const uint8_t *start, const uint8_t *end, httpd_req_t *req);
int init_update_key(int idxns, int idxkey, int ktype, size_t upd_len, errrep_t *errrep);
int update_key_chunk(int idxns, int idxkey, int offset, int len, void *chunk, errrep_t *errrep);
#if 0
esp_err_t nvskey_upload_handler(httpd_req_t *req);
#endif


#endif /* MAIN_NVSOP_H_ */
