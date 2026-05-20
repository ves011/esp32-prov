#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <esp_log.h>
#include <esp_system.h>
#include <nvs_flash.h>
#include <sys/param.h>
#include <esp_console.h>
#include "esp_err.h"
#include "project_specific.h"
#include "common_defines.h"
#include "utils.h"
#include "ntp_sync.h"
#include "cmd_system.h"
#include "cmd_wifi.h"
#include "file_server.h"


static const char *TAG = "OTA-main";
#define PROMPT_STR	"ESP32-prov"

#define CONFIG_EXAMPLE_ENABLE_HTTPS_USER_CALLBACK	1

int restart_in_progress;

static void initialize_nvs(void)
	{
	esp_err_t err = nvs_flash_init();
	if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
		ESP_LOGI(TAG, "NVS partition issue %s / %d", esp_err_to_name(err), err);
	}
	
void app_main(void)
	{
	initialize_nvs();
	get_nvs_conf();
	initialise_wifi(false);
	esp_console_register_help_command();
	/*
	 * optional component 
	 * if commented out then comment out this line in CMakeList.txt
	 * "../../esp32_common/cmds/cmd_system.c"
	 * this will save ~100kB of flash
	*/
	register_system();
	/*-----------------------------------------------------*/
	register_wifi();

	/* Set local timezone (used by UI and logging) */
	setenv("TZ","EET-2EEST,M3.4.0/03,M10.4.0/04",1);
	start_file_server(BASE_PATH);
	wifi_join(NULL, NULL, JOIN_TIMEOUT_MS, true);
	sync_NTP_time();

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
