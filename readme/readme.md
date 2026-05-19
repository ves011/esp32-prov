# ESP32-prov
An extensive provisioning tool for ESP32 with web interface
It allows 
- partition operations: flash, dump, erase, set boot 
- NVS editor: create/delete keys, modify values, dump/load BLOB keys to/from file
- SPIFFS and LITTLEFS file system operations: [under development]

The tool is intended to be flashed in a dedicated partition (~1.1MB) which can be booted when needed, to update running applications, NVS content, or filesystem.

ESP32 run HTTP and websockets server and generates the pages. The client interface and interactions are done using a web browser.
HTML template pages and javascript files (which are plenty) are embedded in the tool image.

ESP32 device acts as a standalone WiFi AP with the default settings in *common_defines.h*:<br>
`#define DEFAULT_AP_SSID			"OTA-dev"`<br>
`#define DEFAULT_AP_PASS			"OTA-devpass"`<br>
`#define DEFAULT_AP_HOSTNAME	    	"ota-dev.local"`<br>
`#define DEFAULT_AP_IP				(1<<24) | (252 <<16) | (168 << 8) | 192 //192.168.252.1`<br>

This is primary interface and is always available.
It also starts the STA mode and looks into default NVS partition (**"nvs"**) for the namespace **nvs.net80211**. If found and if found the keys **sta.ssid** and **sta.paswd**, then try to connect in STA mode

In best case if connection succeeds, will espose a secondary IP interface, together with DEFAULT_AP_IP, whose address is provided by the connected AP. The device can be accessed on any exposed interface.

To preserve the NVS content WiFi on ESP32 is using WIFI_STORAGE_RAM.

## Partition operation
When accessed the device at the address http://ota-dev.local or http://ota-dev.local/part it opens partition operation page
![Partition table page](./part.png)
The page shows the existing partitions in device flash with their attributes and provides several buttons which allows the user to manipulate the content of the partitions.
**The tool does not allow repartitioning of the device flash.**
 The available options are applied to the selected partition from the dropdown box:
 - ***Set BOOT***
-- allows changing the boot partition
-- allowed only if selected partition contains an executable image
<br>
- ***Flash***
-- flash selected partition with the content of chosen file
-- if chosen file contains an ESP32 executable image, version and build date of the executable will be shown
<br>
- ***Dump***
 -- dump the content of the selected partition to a local file
<br>
 - ***Erase***
 --- erase the content of selected partition
 <br>
  - ***Reboot***
  --- reboots the connected target.

  If partition type is **DATA** and subtype is **NVS**, by clicking on the name, opens partition editor page.

  ## NVS editor
  This page can be accessed either directly http://ota-dev.local/nvs?[part-name], with [part-name] = name of the NVS partition, or by clicking on its name in the partition table.
![NVS editor page](./nvsed.png)

Top left table is populated with information about the NVS selected partition. These information are returned by `nvs_get_stats(const char* part_name, nvs_stats_t* nvs_stats)`
function.
On the top right are the controls used to create a new key.
While the fields **namespace**, **key name**, **type** and **length** are self-explanatory, will say few words about **Value(placeholder)** field. 
>If the key type is numeric (`NVS_TYPE_U8` to `NVS_TYPE_I64`) the value typed here is the numeric value of the key in decimal format. A small java-script function will prevent typing anything else than numbers or '-' sign on the first position. Also the length field is automatically populated and cannot be changed.
If the key type is string or blob user can type whatever whishes in this field, but it is taken as a placeholder. This means if the value in the **Length** field is larger than the size of string typed in **Value(placeholder)** field, then the strig is duplicated up to the length in the **Length** field. If smaller then it is truncated. Bottom line in NVS will be written a key of size = **Length** value. (if key is of type string, then the size will be +1 to include the null terminating)

Bottom half shows the list of keys grouped by namespaces.
For each key are shown the properties and the value. The value is shown in a **textarea** control which allows vertical resizing to accommodate larger keys.
Value can be changed following the below rules:
- numeric keys accepts only numbers or "-" sign (only on the first position and if key is NVS_TYPE_I[x] type)
- string keys accepts any character, while typing the length is automatically updated
- blob keys are using a minimal HEX editor
-- for blob keys user can dump or upload to/from local files.
> **HEX editor limitations**
Allows only overwriting. It cannot increase or decrease the length of the key. If change in size is required then use an external editor (hex or whatever), generate a file and then upload this file









