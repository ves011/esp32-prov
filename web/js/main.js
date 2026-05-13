var c_connected = false;
var websocket  = null;
var xhttp;

window.addEventListener("DOMContentLoaded", pageload);

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
function update(fName) 
	{
	if(fName == "reboot")
		{
		document.getElementById(fName).method = "POST";
		document.getElementById(fName)[0].value = "1";
		document.getElementById(fName).action = "/a";
		}
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
function pageload()
    {
    const wsUri = window.location.origin + "/ws";
	websocket = new WebSocket(wsUri);
	websocket.addEventListener("open", () => {console.log("CONNECTED"); c_connected = true;});
	websocket.addEventListener("close", (event) => {console.log("DISCONNECTED: " + event.data); c_connected = false;});
	websocket.onmessage = (msg) => {ws_receive(msg);};
	}