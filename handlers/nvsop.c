/*
 * nvsop.c
 *
 *  Created on: Mar 3, 2026
 *      Author: viorel_serbu
 */

#include "../handlers/nvsop.h"

#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_system.h>
#include <nvs_flash.h>
#include <string.h>
#include <sys/param.h>
#include <esp_log.h>
#include <spi_flash_mmap.h>
#include "esp_err.h"
#include <nvs.h>

#include "nvs_editor.h"
#include "part_editor.h"
#include "ws_client_handler.h"

namespace_t *namespace = NULL;
nvskey_t *nvskey = NULL;
int nns, nkeys;
char nvs_selpart[16];

// request for update structure
rcv_keyval_t rcv_keyval[MAX_CONCURRENT_UPDATES] = {0};
int nrcv = 0;

static int cmp_ns(const void *a, const void *b)
	{
	const namespace_t *na = a;
    const namespace_t *nb = b;
	return strcmp(na->name, nb->name);
	}
	
static int cmp_nvskey(const void *a, const void *b)
	{
    const nvskey_t *ka = a;
    const nvskey_t *kb = b;
    int ns_cmp = strcmp(namespace[ka->ns_idx].name, namespace[kb->ns_idx].name);
    if(ns_cmp != 0)
        return ns_cmp;
    return strcmp(ka->name, kb->name);
	}

static char *TAG = "NVSOP"; 
int get_nvs_entries(char *pName)
	{
	int i;
	esp_err_t res;
	if(namespace)
		free(namespace);
	namespace = NULL;
	
	if(nvskey)
		free(nvskey);
	nvskey = NULL;
	
	nns = nkeys = 0;
	strcpy(nvs_selpart, pName);
	nvs_iterator_t it = NULL;
	nvs_entry_info_t info;
	// first step populate namespace array
	res = nvs_entry_find(pName, NULL, NVS_TYPE_ANY, &it);
 	ESP_LOGI(TAG, "nvs_entry_find return: %s", esp_err_to_name(res));
	while(res == ESP_OK) 
		{
	    nvs_entry_info(it, &info); 
	    for(i = 0; i < nns; i++)
	    	{
	    	if(strcmp(namespace[i].name, info.namespace_name) == 0)
	    		break;
			}
		if(i == nns)
			{
			namespace_t *n = realloc(namespace, sizeof(namespace_t) * (nns + 1));
			if(n)
				{
				namespace = n;
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
		res = nvs_entry_next(&it);
		}
	nvs_release_iterator(it);
	//sort namesapaces
	qsort(namespace, nns, sizeof(namespace_t), cmp_ns);
	
	//second step populate nvskey
 	res = nvs_entry_find(pName, NULL, NVS_TYPE_ANY, &it);
 	ESP_LOGI(TAG, "nvs_entry_find return: %s", esp_err_to_name(res));
	while(res == ESP_OK) 
		{
	    nvs_entry_info(it, &info); // Can omit error check if parameters are guaranteed to be non-NULL
	    ESP_LOGI(TAG, "ns: %s key: %s type: %d", info.namespace_name, info.key, info.type);
	    for(i = 0; i < nns; i++)
	    	{
	    	if(strcmp(namespace[i].name, info.namespace_name) == 0)
	    		break;
			}
		if(i < nns)
			{
			nvskey_t *k = realloc(nvskey, sizeof(nvskey_t) * (nkeys + 1));
			if(k)
				{
				nvskey = k;
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
			}
		res = nvs_entry_next(&it);
	 	}
	nvs_release_iterator(it);
	//sort nvskeys
	qsort(nvskey, nkeys, sizeof(nvskey_t), cmp_nvskey);
	return ESP_OK;
	}
int create_nvs_key(char *pName, char *ns, char *key, int type, int len, char *phv)
	{
	int ret = ESP_OK;
	uint64_t nval;
	if(nns == 0 && nkeys == 0 && pName)
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
			}
		}
	if(ret == ESP_OK)
		get_nvs_entries(nvs_selpart);
	return ret;
	}
int update_key_chunk(int idxns, int idxkey, int offset, int len, void *chunk, errrep_t *errrep)
	{
	int ret = ESP_FAIL, i;
	nvs_handle_t handle;
	for(i = 0; i < MAX_CONCURRENT_UPDATES; i++)
		{
		if(rcv_keyval[i].idxkey == idxkey && 
			rcv_keyval[i].idxns == idxns && 
				(rcv_keyval[i].state == UPDATE_INPROGRESS || rcv_keyval[i].state == UPDATE_READY))
			break;
		}
	if(i < MAX_CONCURRENT_UPDATES && 
		(nvskey[idxkey].type == NVS_TYPE_STR || nvskey[idxkey].type == NVS_TYPE_BLOB) &&
			offset >= 0 && offset + len <= rcv_keyval[i].len)
		{
		memcpy((uint8_t *)rcv_keyval[i].recvb + offset, chunk, len);
		rcv_keyval[i].rcvlen = MAX(rcv_keyval[i].rcvlen, offset + len);
		if(rcv_keyval[i].rcvlen == rcv_keyval[i].len)
			{
			ret = nvs_open_from_partition(nvs_selpart, namespace[idxns].name, NVS_READWRITE, &handle);
			ESP_LOGI(TAG, "name space open: %s / out handle %d", esp_err_to_name(ret), handle);
			if(ret == ESP_OK)
				{
				ret = nvs_set_val(nvskey[idxkey].type, handle, nvskey[idxkey].name, rcv_keyval[i].len, rcv_keyval[i].recvb);
				if(ret == ESP_OK)
					{
					nvs_commit(handle);
					ret = 1;
					}
				else
					{
					snprintf(errrep->errmsg, sizeof(errrep->errmsg), "Error updating key %s (%d)", esp_err_to_name(ret), ret);
					ESP_LOGI(TAG, "%s", errrep->errmsg);
					}
				nvs_close(handle);
				}
			else
				{
				snprintf(errrep->errmsg, sizeof(errrep->errmsg), "Error updating key %s (%d)", esp_err_to_name(ret), ret);
				ESP_LOGI(TAG, "%s", errrep->errmsg);
				}
			if(rcv_keyval[i].recvb)
				{
				free(rcv_keyval[i].recvb);
				rcv_keyval[i].recvb = NULL;
				}
			memset(&rcv_keyval[i], 0, sizeof(rcv_keyval_t));
			rcv_keyval[i].state = UPDATE_COMPLETE;
			}
		else if(rcv_keyval[i].rcvlen > rcv_keyval[i].len)
			{
			rcv_keyval[i].state = UPDATE_COMPLETE;
			snprintf(errrep->errmsg, sizeof(errrep->errmsg), "%s wrong length received. Expected %d received %d\nUpdated aborted", 
				nvskey[rcv_keyval[i].idxkey].name,  rcv_keyval[i].len, rcv_keyval[i].rcvlen);
			ESP_LOGI(TAG, "%s", errrep->errmsg);
			if(rcv_keyval[i].recvb)
				{
				free(rcv_keyval[i].recvb);
				rcv_keyval[i].recvb = NULL;
				}
			memset(&rcv_keyval[i], 0, sizeof(rcv_keyval_t));
			rcv_keyval[i].state = UPDATE_COMPLETE;
			}
		else
			{
			ESP_LOGI(TAG, "%s progress. Expected %d received %d", 
				nvskey[rcv_keyval[i].idxkey].name,  rcv_keyval[i].len, rcv_keyval[i].rcvlen);
			rcv_keyval[i].state = UPDATE_INPROGRESS;
			ret = ESP_OK;
			}	
		}
	return ret;
	}
int init_update_key(int idxns, int idxkey, int ktype, size_t upd_len, errrep_t *errrep)
	{
	int ret = ESP_FAIL, i;
	for(i = 0; i < MAX_CONCURRENT_UPDATES; i++) // check if any update for this key is inprogress
		{
		if(rcv_keyval[i].idxkey == idxkey && 
			rcv_keyval[i].idxns == idxns && 
				rcv_keyval[i].type == ktype  && rcv_keyval[i].state == UPDATE_INPROGRESS)
			{
			rcv_keyval[i].state = UPDATE_COMPLETE;
			break;
			}
		}
	if(i == MAX_CONCURRENT_UPDATES) // look for first available rcv_keyval
		{
		for(i = 0; i < MAX_CONCURRENT_UPDATES; i++)
			{
			if(rcv_keyval[i].state == UPDATE_COMPLETE || rcv_keyval[i].state == UPDATE_NOTUSED)
				break;
			}
		}
	
	if(i < MAX_CONCURRENT_UPDATES)
		{
		if(rcv_keyval[i].recvb)
			free(rcv_keyval[i].recvb);
		void *b = calloc(1, upd_len);
		if(b)
			{
			rcv_keyval[i].idxkey = idxkey;
			rcv_keyval[i].idxns = idxns;
			rcv_keyval[i].state = UPDATE_READY;
			rcv_keyval[i].len = upd_len;
			rcv_keyval[i].recvb = b;
			rcv_keyval[i].rcvlen = 0;
			rcv_keyval[i].type = ktype;
			ret = ESP_OK;
			}
		else
			{
			snprintf(errrep->errmsg, sizeof(errrep->errmsg), "Cannot allocate memory for new key size: %d", upd_len);
			ESP_LOGI(TAG, "%s", errrep->errmsg);
			errrep->err = -3;
			ret = -3;
			}
		}
	else	// already MAX_CONCURRENT_UPDATES ongoing
		{
		snprintf(errrep->errmsg, sizeof(errrep->errmsg),"MAX_CONCURRENT_UPDATES already ongoing"); 
		ESP_LOGI(TAG, "%s", errrep->errmsg);
		errrep->err = -4;
		ret = -4;
		}
	ESP_LOGI(TAG, "init_update_key(): %d", ret);
	return ret;
	}

int nvs_update_key(int type, int nsidx, int keyidx, int len, void *val)
	{
	int ret = ESP_FAIL;
	nvs_handle_t handle;
	if(keyidx < nkeys && nvskey[keyidx].ns_idx == nsidx && type < NVS_TYPE_STR)
		{
		ret = nvs_open_from_partition(nvs_selpart, namespace[nsidx].name, NVS_READWRITE, &handle);
		if(ret == ESP_OK)
			{
			ret = nvs_set_val(type, handle, nvskey[keyidx].name, len, val);
			}
		}
	return ret;
	}

int nvs_set_val(int type, nvs_handle_t handle, char *name, int len, void *val)
	{
	int ret = ESP_FAIL;
	uint8_t swapb[8];
	/*
	if(type < NVS_TYPE_STR)
		{
		for(int i = 0; i < 8; i++)
			swapb[i] = *(uint8_t *)(val + 7 - i);
		}
	ESP_LOGI(TAG, "%s update %x %x %x %x %x %x %x %x ", name, 
				*(uint8_t *)val, *(uint8_t *)(val + 1), *(uint8_t *)(val + 2), *(uint8_t *)(val + 3), *(uint8_t *)(val + 4), *(uint8_t *)(val + 5), *(uint8_t *)(val + 6), *(uint8_t *)(val + 7));
	ESP_LOGI(TAG, "uint64_t %08llx", *(uint64_t *)swapb);
	*/
	memcpy(swapb, val, sizeof(swapb));
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
			*(char *)(val + len) = 0;
			ret = nvs_set_str(handle, name, val);
			break;
		case NVS_TYPE_BLOB:
			ret = nvs_set_blob(handle, name, val, len);
			break;
		}
	if(ret == ESP_OK)
		nvs_commit(handle);
	return ret;
	}

int erase_nvs_key(int nsID, int keyID)
	{
	int ret = ESP_FAIL, i, j;
	nvs_handle_t handle;
	for(i = 0; i < nns; i++)
		{
		if(i == nsID)
			{
			ret = nvs_open_from_partition(nvs_selpart, namespace[i].name, NVS_READWRITE, &handle);
			if(ret == ESP_OK)
				{
				ret = ESP_FAIL;
				if(keyID == -1)	//delete entire namespace
					ret =  nvs_erase_all(handle);
				else // delete single key
					{
					for(j = 0; j < nkeys; j++)
						{
						if(nvskey[j].ns_idx == i && j == keyID)
							{
							ret = nvs_erase_key(handle, nvskey[j].name);
							break;
							}
						}
					if(j == nkeys)
						ESP_LOGI(TAG, "key not found: %d / %d", j, i);
					}
				if(ret == ESP_OK)
					nvs_commit(handle);
				nvs_close(handle);
				}
			else
				ESP_LOGI(TAG, "Error open NVS namespace: %s / %d", namespace[i].name, i);
			break;
			}
		}
	if(i == nns)
		ESP_LOGI(TAG, "namespace not found: %d", i);
	return ret;
	} 
	
