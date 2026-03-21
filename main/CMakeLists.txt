# See the build system documentation in IDF programming guide
# for more information about component CMakeLists.txt files.

idf_component_register(
    SRCS 
		"main.c"
		"../http/file_server.c"
		"../http/handlers.c"
		"../http/keep_alive.c"
		"../http/ws_client_handler.c"
		"../ota-http/nvs_editor.c"
		"../ota-http/nvsop.c"
		"../../esp32_common/utils/utils.c"
		"../../esp32_common/cmds/cmd_wifi.c"
		"../../esp32_common/cmds/cmd_system.c"
    INCLUDE_DIRS
	    "."										# optional, add here public include directories
    	"../../esp32_common"
		"../../esp32_common/utils"
		"../../esp32_common/cmds"
		"../../esp32_common/tcp"
		"../ota-http"
		"../http"
	EMBED_FILES
		"../ota-http/favicon.ico" 
		"../ota-http/main.html"
		"../ota-http/nvseditor.html"
		)