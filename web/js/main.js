var c_connected = false;
var websocket  = null;
var xhttp;

function unixToLocal(ts)
	{
    const d = new Date(ts * 1000);

    const yyyy = d.getFullYear();
    const mm   = String(d.getMonth() + 1).padStart(2, "0");
    const dd   = String(d.getDate()).padStart(2, "0");

    const hh   = String(d.getHours()).padStart(2, "0");
    const min  = String(d.getMinutes()).padStart(2, "0");
	const sec  = String(d.getSeconds()).padStart(2, "0");

    return `${yyyy}-${mm}-${dd} ${hh}:${min}`;
	}

function setBoot()
	{
	var pName = document.getElementById("parts").value;
	var msg = createMessage(CMD_SETBOOT, [pName], payload = null);
	var ws_string = buildAppProto(msg);
	ws_send(ws_string);
	}
function erase()
	{
	var pName = document.getElementById("parts").value;
	var msg = createMessage(CMD_ERASEPART, [pName], payload = null);
	var ws_string = buildAppProto(msg);
	ws_send(ws_string);
	}

function esp_reboot()
	{
	var msg = createMessage(CMD_REBOOT, [], payload = null);
	var ws_string = buildAppProto(msg);
	ws_send(ws_string);
	}
function ws_send(msg)
	{
	if(websocket.readyState == WebSocket.OPEN)
		websocket.send(msg);
	else
		alert("not connected to websocket server");
	}
function ws_receive(msg)
	{
	console.log('RECEIVED:', msg.data.slice(0, 40));
	const parsed = parseAppProto(msg.data);
	if(parsed.ret != 0)
		return;
	if(parsed.msg.command == CMD_REQCONF)
		{
		let c = confirm('Erasing ' + parsed.msg.params[1] + ' all data will be lost!\nPress "OK" to continue');
		if(c)
			{
			let msgOut = createMessage(RSP_CONFIRMATION, [parsed.msg.params[0], parsed.msg.params[1], "OK"], null);
			let buffer = buildAppProto(msgOut);
			ws_send(buffer);
			}
		}
	else if(parsed.msg.command == RSP_CMD)
		{
		if(parsed.msg.params[0] == CMD_SETBOOT)
			{
			if(parsed.msg.params[2] != "0") //error setting boot flag
				alert("Cannot set boot flag to " + parsed.msg.params[1] + "\n" + parsed.msg.params[3]);
			else
				window.location.reload(true);
			}
		else if(parsed.msg.params[0] == CMD_REBOOT)
			{
			window.location.reload(true);
			}
		}
	else if(parsed.msg.command == URC_STATUS)
		{
		if(parsed.msg.params[0] == OP_DOWNLOAD)
			{
			if(parsed.msg.params[1] == PAR_ERROR)
				alert("Download error: " + parsed.msg.params[2] + "\n" + parsed.msg.params[3]);
			}
		if(parsed.msg.params[0] == OP_UPLOAD)
			{
			if(parsed.msg.params[1] == PAR_PROGRESS)
				document.getElementById("ustatus").innerHTML = "flashing... " + parsed.msg.params[3] + "%";
			else if(parsed.msg.params[1] == PAR_ERROR)
				{
				alert("Flashing error: " + parsed.msg.params[2] + "\n" + parsed.msg.params[3]);
				document.getElementById("upload").disabled = false;
				}
			}
		}
	else if(parsed.msg.command == URC_DEVINFO)
		{
		if(parsed.msg.params[0] == PAR_DEVTIME)
			document.getElementById("devtime").innerHTML = unixToLocal(parsed.msg.params[1]);
		if(parsed.msg.params[0] == PAR_STAIP)
			document.getElementById("sta_ip").innerHTML = parsed.msg.params[1];
		if(parsed.msg.params[0] == PAR_APIP)
			document.getElementById("ap_ip").innerHTML = parsed.msg.params[1];
		if(parsed.msg.params[0] == PAR_STASSID)
			document.getElementById("sta_ssid").innerHTML = parsed.msg.params[1];
		if(parsed.msg.params[0] == PAR_STARSSI)
			document.getElementById("sta_rssi").innerHTML = parsed.msg.params[1];
		}
	}
function selFile(event)
	{
	document.getElementById("ustatus").innerHTML = "";
	}
function upload() 
	{
    var upload_path = "/upload/" + document.getElementById("parts").value; //filePath;
    var fileInput = document.getElementById("newfile").files;
	if(fileInput.length == 0)
		{
		alert("No file chosen!!!");
		return;
		}
    var file = fileInput[0];

	const xhttp = new XMLHttpRequest();
	xhttp.onreadystatechange = function() 
		{
		if (xhttp.readyState == 4) 
			{
            if (xhttp.status == 200) 
				document.getElementById("ustatus").innerHTML = "Flashing complete";
			else 
                alert(xhttp.status + " Error!\n" + xhttp.responseText);
			document.getElementById("upload").disabled = false;
        	}
		};
	xhttp.onerror = function() {alert("Network error during upload");};
	xhttp.open("POST", upload_path, true);
	document.getElementById("upload").disabled = true;
    xhttp.send(file);   // ✅ send file directly
	}
function dump()
	{
	var name = document.getElementById("parts").value;
	window.location.href =
        "/download/" + encodeURIComponent(name);
	}
function ws_open()
	{
	console.log("CONNECTED"); 
	c_connected = true;
	document.getElementById("websock").innerHTML = "connected";
	}
function ws_close(event)
	{
	console.log("DISCONNECTED: " + event.data); 
	c_connected = false;
	document.getElementById("websock").innerHTML = "not connected";
	}

function syncTime()
	{
	var msg = createMessage(CMD_SYNCTIME, [], payload = null);
	var ws_string = buildAppProto(msg);
	ws_send(ws_string);
	}

// --------------------------------------------------
// Parse ESP-IDF esp_app_desc_t from firmware image
// Works directly in browser
// --------------------------------------------------

async function parseEsp32Firmware(file)
    {
    const buf = await file.arrayBuffer();
    const data = new Uint8Array(buf);

    // esp_app_desc_t magic word
    // uint32_t 0xABCD5432 little endian
    const magic = [0x32, 0x54, 0xCD, 0xAB];
    // search first ~64kB only
    // descriptor is always near image start
    let pos = -1;
    for(let i = 0; i < Math.min(data.length - 4, 65536); i++)
        {
        if(data[i] === magic[0] &&
           data[i + 1] === magic[1] &&
           data[i + 2] === magic[2] &&
           data[i + 3] === magic[3])
            {
            pos = i;
            break;
            }
        }

    if(pos < 0)
		{
        const info = {secure_version : 0, version : "N/A", project_name : "N/A", compile_time : "N/A", compile_date : "N/A"}
		return info;
		}

    // helper
    function readString(offset, len)
        {
        const slice = data.slice(pos + offset, pos + offset + len);
        // find first NULL
        let end = slice.indexOf(0);
        if(end < 0)
            end = slice.length;
        return new TextDecoder().decode(slice.slice(0, end));
        }

    // esp_app_desc_t layout
    // offset relative to magic word
    const info =
        {
        secure_version : new DataView(buf, pos + 4, 4).getUint32(0, true),
        version : readString(16, 32),
        project_name : readString(48, 32),
        compile_time : readString(80, 16),
        compile_date : readString(96, 16),
        idf_version : readString(112, 32)
        };
    return info;
    }
function pageload()
    {
    const wsUri = window.location.origin + "/ws";
	websocket = new WebSocket(wsUri);
	websocket.addEventListener("open", ws_open());// => {console.log("CONNECTED"); c_connected = true;});
	websocket.addEventListener("close", (event) => 
		{
		console.log("DISCONNECTED: " + event.data); 
		c_connected = false;
		document.getElementById("websock").innerHTML = "not connected";
		});
	websocket.onmessage = (msg) => {ws_receive(msg);};
	document.getElementById("newfile").addEventListener("change", async (e) =>
		{
		try
			{
			const info = await parseEsp32Firmware(e.target.files[0]);
			document.getElementById("projstr").innerHTML = "Project name: " + info.project_name;
			document.getElementById("verstr").innerHTML = "Version: " + info.version;
			document.getElementById("buildstr").innerHTML = "Build time: " + info.compile_date + " " + info.compile_time;
			console.log(info);
			}
		catch(err) {console.error(err);}
		});
	}