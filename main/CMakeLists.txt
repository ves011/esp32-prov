# See the build system documentation in IDF programming guide
# for more information about component CMakeLists.txt files.

idf_component_register(
    SRCS 
		"main.c"
		"../http/file_server.c"
		"../http/keep_alive.c"
		"../http/ws_client_handler.c"
		"../http/html_builder.c"
		"../handlers/part_editor.c"
		"../handlers/nvs_editor.c"
		"../handlers/nvsop.c"
		"../handlers/spiffsop.c"
		"../../esp32_common/ws_mqtt_proto/app_proto.c"
		"../../esp32_common/utils/utils.c"
		"../../esp32_common/cmds/cmd_wifi.c"
		# optional component
		"../../esp32_common/cmds/cmd_system.c"
		
		"../../esp32_common/tcp/tcp_log.c"
		"../../esp32_common/tcp/ntp_sync.c"
    INCLUDE_DIRS
	    "."
    	"../../esp32_common"
		"../../esp32_common/utils"
		"../../esp32_common/cmds"
		"../../esp32_common/tcp"
		"../../esp32_common/ws_mqtt_proto/"
		"../handlers"
		"../http"
	EMBED_FILES
		"../web/favicon.ico" 
		"../web/html/main.html"
		"../web/html/nvseditor.html"
		"../web/html/spiffsed.html"
		"../web/js/main.js"
		"../web/js/appprotolegacy.js"
		"../web/js/nvs.js"
		"../web/js/hexed.js"
		"../../esp32_common/ws_mqtt_proto/protocoldef.js"
		)