# See the build system documentation in IDF programming guide
# for more information about component CMakeLists.txt files.

idf_component_register(
    SRCS 
		"main.c"			# list the source files of this component
		"file_server.c"
		"handlers.c"
		"nvs_editor.c"
		"keep_alive.c"
		"ws_client_handler.c"
		"nvsop.c"
		"../../esp32_common/utils/utils.c"
		"../../esp32_common/cmds/cmd_wifi.c"
		"../../esp32_common/cmds/cmd_system.c"
    INCLUDE_DIRS        
	    	"."					# optional, add here public include directories
    	"../../esp32_common"
		"../../esp32_common/utils"
		"../../esp32_common/cmds"
		"../../esp32_common/tcp"
	EMBED_FILES 
		"favicon.ico" 
		"main.html"
		"nvseditor.html"
    PRIV_INCLUDE_DIRS   # optional, add here private include directories
    REQUIRES            # optional, list the public requirements (component names)
    PRIV_REQUIRES       # optional, list the private requirements
)
