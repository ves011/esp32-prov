/*
 * nvsop.h
 *
 *  Created on: Mar 3, 2026
 *      Author: viorel_serbu
 */

#ifndef MAIN_NVSOP_H_
#define MAIN_NVSOP_H_

#include "esp_http_server.h"
#include "nvs.h"

#define UPDATE_READY		0
#define UPDATE_INPROGRESS	1
#define UPDATE_COMPLETE		2

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
int erase_nvs_key(char *ns, char *key);
esp_err_t nvskey_get_handler(httpd_req_t *req);


#endif /* MAIN_NVSOP_H_ */
