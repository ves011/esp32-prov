#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <esp_log.h>
#include <esp_system.h>
#include <nvs_flash.h>
#include <sys/param.h>
#include <esp_console.h>
#include "project_specific.h"
#include "common_defines.h"
#include "utils.h"
#include "cmd_system.h"
#include "cmd_wifi.h"
#include "file_server.h"
#include "nvsop.h"

static const char *TAG = "OTA-main";
#define PROMPT_STR	"OTA-https"

#define CONFIG_EXAMPLE_ENABLE_HTTPS_USER_CALLBACK	1

int restart_in_progress;

static void initialize_nvs(void)
	{
	esp_err_t err = nvs_flash_init();
	if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
		{
		ESP_ERROR_CHECK(nvs_flash_erase());
		err = nvs_flash_init();
		}
	ESP_ERROR_CHECK(err);
	}
	
void app_main(void)
	{
	spiffs_storage_check();
	initialize_nvs();
	rw_dev_config(PARAM_READ);

	esp_console_register_help_command();
	register_system();
	register_wifi();
	register_nvsop();
	if(!wifi_join("gnetR1-24", "Perdita98", JOIN_TIMEOUT_MS))
		{
		ESP_LOGI(TAG, "Failed to connect to %s", dev_conf.sta_ssid);
		}
	else
		{
		ESP_LOGI(TAG, "Connected to %s", dev_conf.sta_ssid);
		start_file_server(BASE_PATH);
		}
#ifdef WITH_CONSOLE
	esp_console_repl_t *repl = NULL;
    esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
    /* Prompt to be printed before each line.
     * This can be customized, made dynamic, etc.
     */
    repl_config.prompt = PROMPT_STR ">";
    repl_config.max_cmdline_length = CONFIG_CONSOLE_MAX_COMMAND_LINE_LENGTH;
#if CONFIG_STORE_HISTORY
	repl_config.history_save_path = BASE_PATH HISTORY_FILE;
#endif

#if defined(CONFIG_ESP_CONSOLE_UART_DEFAULT) || defined(CONFIG_ESP_CONSOLE_UART_CUSTOM)
    esp_console_dev_uart_config_t hw_config = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_console_new_repl_uart(&hw_config, &repl_config, &repl));

#elif defined(CONFIG_ESP_CONSOLE_USB_CDC)
    esp_console_dev_usb_cdc_config_t hw_config = ESP_CONSOLE_DEV_CDC_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_console_new_repl_usb_cdc(&hw_config, &repl_config, &repl));

#elif defined(CONFIG_ESP_CONSOLE_USB_SERIAL_JTAG)
    esp_console_dev_usb_serial_jtag_config_t hw_config = ESP_CONSOLE_DEV_USB_SERIAL_JTAG_CONFIG_DEFAULT();
    repl_config.task_stack_size = 6020;
    ESP_ERROR_CHECK(esp_console_new_repl_usb_serial_jtag(&hw_config, &repl_config, &repl));
    //ESP_LOGI(TAG, "console stack: %d", repl_config.task_stack_size);
#else
	#error Unsupported console type
#endif
	ESP_ERROR_CHECK(esp_console_start_repl(repl));
#endif	
	}
