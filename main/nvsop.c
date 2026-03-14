/*
 * nvsop.c
 *
 *  Created on: Mar 3, 2026
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
#include "argtable3/argtable3.h"
#include "esp_console.h"
#include <nvs.h>
#include "esp_partition.h"
#include "freertos/idf_additions.h"
#include "lwip/opt.h"
#include "project_specific.h"
#include "common_defines.h"
#include "ws_client_handler.h"
#include "nvs_editor.h"
#include "nvsop.h"

namespace_t *namespace = NULL;
nvskey_t *nvskey = NULL;
int nns, nkeys;
char nvs_selpart[16];
static TaskHandle_t update_task_handle = NULL;
QueueHandle_t receive_q = NULL;
rcv_keyval_t *rcv_keyval;
int nrcv = 0;

static struct
	{
    struct arg_str *part;
    struct arg_str *ns;
    struct arg_str *key;
    struct arg_str *op;
    struct arg_int *type;
    struct arg_end *end;
	} nvs_args;

static char *TAG = "NVSOP"; 

int do_nvs(int argc, char **argv)
	{
	int i, j;
	char pn[20];
	if(strcmp(argv[0], "nvs"))
		return 1;
	int nerrors = arg_parse(argc, argv, (void **)&nvs_args);
	if (nerrors != 0)
		{
		arg_print_errors(stderr, nvs_args.end, argv[0]);
		return ESP_FAIL;
		}
	if(nvs_args.part->count)
		strcpy(pn, nvs_args.part->sval[0]);
	else
		strcpy(pn, "nvs");
	ESP_LOGI(TAG, "selected partition: %s", pn);
	if(strcmp(pn, nvs_selpart))
		get_nvs_entries(pn);
	if(strcmp(nvs_args.op->sval[0], "list") == 0)
		{
		get_nvs_entries(pn);
		for(i = 0; i < nns; i++)
			{
			for(j = 0; j < nkeys; j++)
				{
				if(nvskey[j].ns_idx == i)
					ESP_LOGI(TAG, "table ns: %s key: %s type: %d", namespace[i].name, nvskey[j].name, nvskey[j].type);
				}
			}
		ESP_LOGI(TAG, "no of ns: %d / no of keys: %d", nns, nkeys);
		}
/*		
	if(strcmp(nvs_args.op->sval[0], "add") == 0)
		{
		if(nvs_args.ns->count && nvs_args.key->count && nvs_args.type->count)
			create_nvs_key(NULL, (char *)nvs_args.ns->sval[0], (char *)nvs_args.key->sval[0], nvs_args.type->ival[0]);
		else
			ESP_LOGI(TAG, "namespace or key name or type not provided");
		}
*/		
	return ESP_OK;
	}
void register_nvsop(void)
	{
	nvs_args.part = arg_str0("p", "part", "<op>", "NVS operation");
    nvs_args.ns = arg_str0("n", "nspace", "<namespace>", "name space name");
    nvs_args.key = arg_str0("k", "key", "<key>", "key name");
    nvs_args.op = arg_str1(NULL, NULL, "<op>", "operation");
    nvs_args.type = arg_int0("t", "type", "<t>", "data type");
    nvs_args.end = arg_end(2);
	const esp_console_cmd_t nvs_cmd =
    	{
        .command = "nvs",
        .help = "nvs cmds",
        .hint = NULL,
        .func = &do_nvs,
        .argtable = &nvs_args
    	};
    ESP_ERROR_CHECK( esp_console_cmd_register(&nvs_cmd));
    if(xTaskCreate(nvs_update_task, "recv_task", 8192, NULL, 5, &update_task_handle) != pdPASS)
		{
		ESP_LOGI(TAG, "Unable to create nvs update task");
		esp_restart();
		}
	}

int get_nvs_entries(char *pName)
	{
	int i;
	
	if(namespace)
		free(namespace);
	namespace = NULL;
	
	if(nvskey)
		free(nvskey);
	nvskey = NULL;
	
	nns = nkeys = 0;
	strcpy(nvs_selpart, pName);
	nvs_iterator_t it = NULL;
 	esp_err_t res = nvs_entry_find(pName, NULL, NVS_TYPE_ANY, &it);
 	ESP_LOGI(TAG, "nvs_entry_find return: %s", esp_err_to_name(res));
	while(res == ESP_OK) 
		{
		nvs_entry_info_t info;
	    nvs_entry_info(it, &info); // Can omit error check if parameters are guaranteed to be non-NULL
	    ESP_LOGI(TAG, "ns: %s key: %s type: %d", info.namespace_name, info.key, info.type);
	    for(i = 0; i < nns; i++)
	    	{
	    	if(strcmp(namespace[i].name, info.namespace_name) == 0)
	    		break;
			}
		if(i == nns)
			{
			namespace = realloc(namespace, sizeof(namespace_t) * (nns + 1));
			if(namespace)
				{
				strcpy(namespace[nns].name, info.namespace_name);
				namespace[nns].nentries = 0;
				nns++;
				}
			else
				{
				ESP_LOGE(TAG, "error realloc namespace");
				return ESP_FAIL;
				}
			}
		
		nvskey = realloc(nvskey, sizeof(nvskey_t) * (nkeys + 1));
		if(nvskey)
			{
			strcpy(nvskey[nkeys].name, info.key);
			nvskey[nkeys].ns_idx = i;
			nvskey[nkeys].type = info.type;
			if(info.type < NVS_TYPE_STR)
				nvskey[nkeys].size = info.type & 0x0f;
			nkeys++;
			namespace[i].nentries++;
			}
		else
			{
			ESP_LOGE(TAG, "error realloc nvskey");
			return ESP_FAIL;
			}
		
		res = nvs_entry_next(&it);
	 	}
	nvs_release_iterator(it);
	return ESP_OK;
	}
int create_nvs_key(char *pName, char *ns, char *key, int type, int len, char *phv)
	{
	int ret = ESP_OK;
	uint64_t nval;
	if(nns == 0 && nkeys == 0)
		ret = get_nvs_entries(pName);
	if(ret == ESP_OK)
		{
		nvs_handle_t out_handle;;
		ret = nvs_open_from_partition(nvs_selpart, ns, NVS_READWRITE, &out_handle);
		ESP_LOGI(TAG, "name space open: %s / out handle %d", esp_err_to_name(ret), out_handle);
		if(ret == ESP_OK)
			{
			if(type < NVS_TYPE_STR)
				{
				int base = 10;
				char *endptr;
				if(strstr(phv, "0x") == phv || strstr(phv, "0X") == phv)
					base = 16;
				nval = strtoull(phv, &endptr, base);
				switch(type)
					{
					case NVS_TYPE_U8:
						ret = nvs_set_u8(out_handle, key, (uint8_t)nval);
						break;
					case NVS_TYPE_I8:
						ret = nvs_set_i8(out_handle, key, (int8_t)nval);
						break;
					case NVS_TYPE_U16:
						ret = nvs_set_u16(out_handle, key, (uint16_t)nval);
						break;
					case NVS_TYPE_I16:
						ret = nvs_set_i16(out_handle, key, (int16_t)nval);
						break;
					case NVS_TYPE_U32:
						ret = nvs_set_u32(out_handle, key, (uint32_t)nval);
						break;
					case NVS_TYPE_I32:
						ret = nvs_set_i32(out_handle, key, (int32_t)nval);
						break;
					case NVS_TYPE_U64:
						ret = nvs_set_u64(out_handle, key, (uint64_t)nval);
						break;
					case NVS_TYPE_I64:
						ret = nvs_set_i64(out_handle, key, (int64_t)nval);
						break;
					default:
						break;
					}
				}
			else
				{
				uint8_t *pval = calloc(len + 1, 1);
				int start = 0;
				ESP_LOGI(TAG, "new key val = %s / len = %d", phv, len);
				if(pval)
					{
					if(type == NVS_TYPE_STR)
						{
						while (start + strlen(phv) < len)
							{
							memcpy(pval + start, phv, strlen(phv));
							start += strlen(phv);
							}
						memcpy(pval + start, phv, len - start);
						*(pval + len) = 0;
						ret = nvs_set_str(out_handle, key, (const char *)pval);
						}
					else if(type == NVS_TYPE_BLOB)
						{
						char b[3], *eb; 
						b[2] = 0;
						int k = 0;
						for(int i = 0; i < len; i ++)
							{
							if(k >= strlen(phv))
								k = 0;
							b[0] = phv[k];
							b[1] = phv[k + 1];
							*(pval + i) = (uint8_t)strtoul(b, &eb, 16);
							k += 2;
							}
						ret = nvs_set_blob(out_handle, key, pval, len);
						}
					free(pval);
					}
				else
					ESP_LOGI(TAG, "cannot allocate memory for key val storage");
				}
/*
			if(ret == ESP_OK)
				{
				nvs_commit(out_handle);
				nvs_close(out_handle);
				ret = nvs_open_from_partition(nvs_selpart, ns, NVS_READONLY, &out_handle);
				if(ret == ESP_OK)
					{
					char b[2];
					uint64_t val = 0;
					size_t sz;
					switch(type)
						{
						case NVS_TYPE_U8:
							ret = nvs_get_u8(out_handle, key, (uint8_t *)&val);
							break;
						case NVS_TYPE_I8:
							ret = nvs_get_i8(out_handle, key, (int8_t *)&val);
							break;
						case NVS_TYPE_U16:
							ret = nvs_get_u16(out_handle, key, (uint16_t *)&val);
							break;
						case NVS_TYPE_I16:
							ret = nvs_get_i16(out_handle, key, (int16_t *)&val);
							break;
						case NVS_TYPE_U32:
							ret = nvs_get_u32(out_handle, key, (uint32_t *)&val);
							break;
						case NVS_TYPE_I32:
							ret = nvs_get_i32(out_handle, key, (int32_t *)&val);
							break;
						case NVS_TYPE_U64:
							ret = nvs_get_u64(out_handle, key, (uint64_t *)&val);
							break;
						case NVS_TYPE_I64:
							ret = nvs_get_i64(out_handle, key, (int64_t *)&val);
							break;
						case NVS_TYPE_STR:
							sz = 2;
							ret = nvs_get_str(out_handle, key, b, &sz);
							break;
						case NVS_TYPE_BLOB:
							sz = 1;
							ret = nvs_get_blob(out_handle, key, b, &sz);
							break;
						}
					ESP_LOGI(TAG, "key add check: %s / out value %x", esp_err_to_name(ret), val);
					nvs_close(out_handle);
					}
				}
			}
			
		}
*/
			}
		}
	return ret;
	}

int recv_update(int idn, int idk, int len, int nrc)
	{
	void *pr;
	int i;
	int ret = ESP_FAIL;
	char buf[64];
	void *b;
	if(nvskey[idk].ns_idx == idn)
		{
		for(i = 0; i < nrcv; i++)
			{
			if(rcv_keyval[i].idxkey == idk && rcv_keyval[i].idxns == idn)
				break;
			}
		if(i == nrcv)
			{
			if(nvskey[idk].ns_idx == idn)
				{
				if(nvskey[idk].type < NVS_TYPE_STR)
					b = calloc(1, 8);
				else
					b = calloc(1, len);
				
				if(b)
					{
					pr = realloc(rcv_keyval, sizeof(rcv_keyval_t) + (nrcv + 1));
					if(pr)
						{
						rcv_keyval = pr;
						rcv_keyval[i].idxkey = idk;
						rcv_keyval[i].idxns = idn;
						rcv_keyval[i].state = UPDATE_READY;
						rcv_keyval[i].len = len;
						rcv_keyval[i].rcvlen = 0;
						rcv_keyval[i].nr_cunks = nrc;
						rcv_keyval[i].rcv_chunks = 0;
						rcv_keyval[i].recvb = b;
						rcv_keyval[i].type = nvskey[idk].type;
						sprintf(buf, "%s\1[%d][%d]", SEND_VAL, idn, idk);
						send_strmsg(buf);
						ret = ESP_OK;
						nrcv++;
						}
					else
						{
						ESP_LOGI(TAG, "too many concurrent updates -  cannot allocate rcv_keyval struct");
						free(b);
						}
					}
				else
					ESP_LOGI(TAG, "too many concurrent updates - cannot allocate  receive buffer");
				}
			}
		else
			{
			ESP_LOGI(TAG, "rcv_keyval already allocated state: %d", rcv_keyval[i].state);
			if(rcv_keyval[i].state == UPDATE_INPROGRESS)
				{
				// error case needs handling
				ESP_LOGI(TAG, "ERROR: rcv_keyval update in progress: %d", rcv_keyval[i].state);
				ret = ESP_FAIL;
				}
			else
				{
				rcv_keyval[i].state = UPDATE_READY;
				rcv_keyval[i].len = len;
				rcv_keyval[i].rcvlen = 0;
				rcv_keyval[i].nr_cunks = nrc;
				rcv_keyval[i].rcv_chunks = 0;
				if(rcv_keyval[i].recvb)
					free(rcv_keyval[i].recvb);
				if(nvskey[idk].type < NVS_TYPE_STR)
					b = calloc(1, 8);
				else
					b = calloc(1, len);
				if(b)
					{
					rcv_keyval[i].recvb = b;
					ret = ESP_OK;
					}
				else
					ESP_LOGI(TAG, "too many concurrent updates - cannot allocate  receive buffer");
				}
			if(rcv_keyval[i].state == UPDATE_READY)
				{
				sprintf(buf, "%s\1[%d][%d]", SEND_VAL, idn, idk);
				send_strmsg(buf);
				ret = ESP_OK;
				}	
			}
		}
	return ret;
	}
/*	
int update_keyval(int idxn, int idxk, void *pstr)
	{
	int i, k, j;
	int ret = ESP_FAIL;
	void *pr;
	for(i = 0; i < nrcv; i++)
		{
		if(rcv_keyval[i].idxns == idxn && rcv_keyval[i].idxkey == idxk)
			{
			if(rcv_keyval[i].state < UPDATE_COMPLETE)
				{
				if(rcv_keyval[i].type < NVS_TYPE_STR)
					{
					ret = set_nvs_value(idxk, pstr);
					rcv_keyval[i].state = UPDATE_COMPLETE;
					ESP_LOGI(TAG, "update_keyval: %d", ret);
					//free rcv_keyval[i]
					if(rcv_keyval[i].recvb)
						free(rcv_keyval[i].recvb);
					for(j = i + 1; j < nrcv; j++)
						{
						k = j - 1;
						memcpy(&rcv_keyval[k], &rcv_keyval[j], sizeof(rcv_keyval_t));
						}
					pr = realloc(rcv_keyval, sizeof(rcv_keyval_t) * (nrcv - 1));
					if(pr)
						{
						rcv_keyval = pr;
						nrcv --;
						}
					break;
					}
				}
			}
		}
	return ret;	
	}
*/	
void nvs_update_task(void *pvParameters)
	{
	rcv_keyval_t rval;
	int i;
	void *pr, *b;
	char buf[80];
	nvs_handle_t handle;
	int ret;
	receive_q = xQueueCreate(10, sizeof(rcv_keyval_t));
	if(!receive_q)
		{
		ESP_LOGE(TAG, "Cannot create receive_q");
		esp_restart();
		}
	rcv_keyval = NULL;
	while(1)
		{
		if(xQueueReceive(receive_q, &rval, portMAX_DELAY))
			{
			for(i = 0; i < nrcv; i++)
				{
				if(rcv_keyval[i].idxkey == rval.idxkey && rcv_keyval[i].idxns == rval.idxns)
					break;
				}
			if(rval.recvb == NULL) //update request. len = full length of the key
				{
				if(i == nrcv)
					{
					if(nvskey[rval.idxkey].ns_idx == rval.idxns)
						{
						if(rval.type < NVS_TYPE_STR)
							b = calloc(1, 8);
						else
							b = calloc(1, rval.len);
						if(b)
							{
							pr = realloc(rcv_keyval, sizeof(rcv_keyval_t) * (nrcv + 1));
							if(pr)
								{
								rcv_keyval = pr;
								rcv_keyval[i].idxkey = rval.idxkey;
								rcv_keyval[i].idxns = rval.idxns;
								rcv_keyval[i].state = UPDATE_READY;
								rcv_keyval[i].len = rval.len;
								rcv_keyval[i].nr_cunks = rval.nr_cunks;
								rcv_keyval[i].rcv_chunks = 0;
								rcv_keyval[i].recvb = b;
								rcv_keyval[i].type = nvskey[rval.idxkey].type;
								//sprintf(buf, "%s\1[%d][%d]", SEND_VAL, rval.idxns, rval.idxkey);
								//send_strmsg(buf);
								nrcv++;
								}
							else
								{
								ESP_LOGI(TAG, "too many concurrent updates -  cannot allocate rcv_keyval struct");
								free(b);
								}
							}
						else
							ESP_LOGI(TAG, "too many concurrent updates - cannot allocate  receive buffer");
						}
					}
				else
					{
					ESP_LOGI(TAG, "rcv_keyval already allocated state: %d", rcv_keyval[i].state);
					if(rcv_keyval[i].state == UPDATE_INPROGRESS)
						{
						// error case needs handling TBD how
						ESP_LOGI(TAG, "ERROR: rcv_keyval update in progress: %d", rcv_keyval[i].state);
						//rcv_keyval[i].state = UPDATE_READY;
						}
					else //UPDATE_READY or UPDATE_COMPLETE
						{
						if(rcv_keyval[i].recvb)
							free(rcv_keyval[i].recvb);
						if(rval.type < NVS_TYPE_STR)
							b = calloc(1, 8);
						else
							b = calloc(1, rval.len);
						if(b)
							{
							rcv_keyval[i].state = UPDATE_READY;
							rcv_keyval[i].len = rval.len;
							rcv_keyval[i].nr_cunks = rval.nr_cunks;
							rcv_keyval[i].rcv_chunks = 0;
							rcv_keyval[i].recvb = b;
							}
						else
							{
							ESP_LOGI(TAG, "too many concurrent updates - cannot allocate  receive buffer");
							}
						}
					}
				if(rcv_keyval[i].state == UPDATE_READY)
					{
					sprintf(buf, "%s\1[%d][%d]", SEND_VAL, rval.idxns, rval.idxkey);
					send_strmsg(buf);
					}	
				}
			else  //chunk with data. len = chunk length
				{
				if(i < nrcv && rcv_keyval[i].state < UPDATE_COMPLETE)
					{
					if(rval.type < NVS_TYPE_STR)
						{
						ret = nvs_open_from_partition(nvs_selpart, namespace[rcv_keyval[i].idxns].name, NVS_READWRITE, &handle);
						ESP_LOGI(TAG, "name space open: %s / out handle %d", esp_err_to_name(ret), handle);
						if(ret == ESP_OK)
							{
							ret = nvs_set_val(nvskey[rcv_keyval[i].idxkey].type, handle, nvskey[rcv_keyval[i].idxkey].name, nvskey[rcv_keyval[i].idxkey].size, rval.recvb);
							if(ret == ESP_OK)
								{
								ESP_LOGI(TAG, "rcv_keyval, nrcv %x %d", rcv_keyval, nrcv);
								if(rcv_keyval[i].recvb)
									free(rcv_keyval[i].recvb);
								for(int j = i + 1; j < nrcv; j++)
									memcpy(&rcv_keyval[j - 1], &rcv_keyval[j], sizeof(rcv_keyval_t));
								pr = realloc(rcv_keyval, sizeof(rcv_keyval_t) * (nrcv - 1));
								nrcv--;
								if(nrcv)
									{
									if(pr)
										rcv_keyval = pr;
									}
								else
									rcv_keyval = NULL;
								
								}
							else
								ESP_LOGI(TAG, "Error updating key %s (%d)", esp_err_to_name(ret), ret);
							}
						}
					else if(rval.type == NVS_TYPE_STR)
						{
						ret = ESP_FAIL;
						memcpy(rcv_keyval[i].recvb + rval.rcv_chunks * RCV_CHUNK_SIZE, rval.recvb, rval.len);
						rcv_keyval[i].rcvlen += rval.len;
						if(rcv_keyval[i].rcvlen == rcv_keyval[i].len)
							{
							rcv_keyval[i].state = UPDATE_COMPLETE;
							ret = nvs_open_from_partition(nvs_selpart, namespace[rcv_keyval[i].idxns].name, NVS_READWRITE, &handle);
							ESP_LOGI(TAG, "name space open: %s / out handle %d", esp_err_to_name(ret), handle);
							rcv_keyval[i].state = UPDATE_COMPLETE;
							if(ret == ESP_OK)
								{
								ret = nvs_set_val(nvskey[rcv_keyval[i].idxkey].type, handle, nvskey[rcv_keyval[i].idxkey].name, nvskey[rcv_keyval[i].idxkey].size, rcv_keyval[i].recvb);
								if(ret != ESP_OK)
									ESP_LOGI(TAG, "Error updating key %s (%d)", esp_err_to_name(ret), ret);
								}
							else
								ESP_LOGI(TAG, "Error updating key %s (%d)", esp_err_to_name(ret), ret);
							}
						else if(rcv_keyval[i].rcvlen > rcv_keyval[i].len)
							{
							rcv_keyval[i].state = UPDATE_COMPLETE;
							ESP_LOGI(TAG, "%s wrong length received. Expected %d received %d", 
								nvskey[rcv_keyval[i].idxkey].name,  rcv_keyval[i].len, rcv_keyval[i].rcvlen);
							}
						else
							{
							ESP_LOGI(TAG, "%s progress. Expected %d received %d", 
								nvskey[rcv_keyval[i].idxkey].name,  rcv_keyval[i].len, rcv_keyval[i].rcvlen);
							rcv_keyval[i].state = UPDATE_INPROGRESS;
							}	
						if(rcv_keyval[i].state == UPDATE_COMPLETE)
							{
							sprintf(buf, "update key\1[%d][%d]\1%s\1%d\1%s\1", 
								rcv_keyval[i].idxns, rcv_keyval[i].idxkey, nvskey[rcv_keyval[i].idxkey].name, ret, esp_err_to_name(ret));
							send_strmsg(buf);
							if(rcv_keyval[i].recvb)
								free(rcv_keyval[i].recvb);
							for(int j = i + 1; j < nrcv; j++)
								memcpy(&rcv_keyval[j - 1], &rcv_keyval[j], sizeof(rcv_keyval_t));
							pr = realloc(rcv_keyval, sizeof(rcv_keyval_t) * (nrcv - 1));
							nrcv--;
							if(nrcv)
								{
								if(pr)
									rcv_keyval = pr;
								}
							else
								rcv_keyval = NULL;
							
							}
						}
					}
				else
					ESP_LOGI(TAG, "NO rcv_keyval found or state = UPDATE_INPROGRESS (i: %d / %d)", i, nrcv);
				}
			}
		}
	}
/*
int set_nvs_value(int idxkey, void *val)
	{
	int ret = ESP_FAIL;
	if(idxkey < nkeys)
		{
		nvs_handle_t handle;;
		ret = nvs_open_from_partition(nvs_selpart, namespace[nvskey[idxkey].ns_idx].name, NVS_READWRITE, &handle);
		ESP_LOGI(TAG, "name space open: %s / out handle %d", esp_err_to_name(ret), handle);
		if(ret == ESP_OK)
			ret = nvs_set_val(nvskey[idxkey].type, handle, nvskey[idxkey].name, nvskey[idxkey].size, val);
		}
	return ret;
	}
*/
int nvs_set_val(int type, nvs_handle_t handle, char *name, int len, void *val)
	{
	int ret = ESP_FAIL;
	uint8_t swapb[8];
	if(type < NVS_TYPE_STR)
		{
		for(int i = 0; i < 8; i++)
			swapb[i] = *(uint8_t *)(val + 7 - i);
		}
	ESP_LOGI(TAG, "%s update %x %x %x %x %x %x %x %x ", name, 
				*(uint8_t *)val, *(uint8_t *)(val + 1), *(uint8_t *)(val + 2), *(uint8_t *)(val + 3), *(uint8_t *)(val + 4), *(uint8_t *)(val + 5), *(uint8_t *)(val + 6), *(uint8_t *)(val + 7));
	ESP_LOGI(TAG, "uint64_t %08llx", *(uint64_t *)swapb);
	switch(type)
		{
		case NVS_TYPE_U8:
			ret = nvs_set_u8(handle, name, *(uint8_t *)swapb);
			break;
		case NVS_TYPE_I8:
			ret = nvs_set_i8(handle, name, *(int8_t *)swapb);
			break;
		case NVS_TYPE_U16:
			ret = nvs_set_u16(handle, name, *(uint16_t *)swapb);
			break;
		case NVS_TYPE_I16:
			ret = nvs_set_i16(handle, name, *(int16_t *)swapb);
			break;
		case NVS_TYPE_U32:
			ret = nvs_set_u32(handle, name, *(uint32_t *)swapb);
			break;
		case NVS_TYPE_I32:
			ret = nvs_set_i32(handle, name, *(int32_t *)swapb);
			break;
		case NVS_TYPE_U64:
			ret = nvs_set_u64(handle, name, *(uint64_t *)swapb);
			break;
		case NVS_TYPE_I64:
			ret = nvs_set_i64(handle, name, *(int64_t *)swapb);
			break;
		case NVS_TYPE_STR:
			ret = nvs_set_str(handle, name, val);
			break;
/*			
		case NVS_TYPE_BLOB:
			ret = nvs_set_blob(handle, name, b, 1);
			break;
		*/
		}
	if(ret == ESP_OK)
		nvs_commit(handle);
	return ret;
	}

int erase_nvs_key(char *ns, char *key)
	{
	int ret = ESP_FAIL;
	nvs_handle_t handle;;
	ret = nvs_open_from_partition(nvs_selpart, ns, NVS_READWRITE, &handle);
	if(ret == ESP_OK)
		{
		if(key == NULL) // delete all keys in the namespace
			ret =  nvs_erase_all(handle);
		else
			ret = nvs_erase_key(handle, key);
		if(ret == ESP_OK)
			nvs_commit(handle);
		nvs_close(handle);
		}
	return ret;
	} 