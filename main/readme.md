## Partition table operations

List of partitions, as they are defined in the .csv file used to generate partition table.
Selectable partition combo which includes only partitions whose content can be modified. This includes partitions which have the following subtype: NVS, OTA_xx, APP, SPIFFS, FAT

The operations are applied to the selected partition in the combo.
If partition subtype NVS is present in the list, it can be edited in detail in "nvs_editor" page.

## HTTP uri handlers
All the uri handlers are registered in function **start_file_server()** (file_server.c file)


