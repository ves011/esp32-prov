var c_connected = false;
var websocket  = null;
var updid = [];
var chunk_size = 512;
var max_blob_size = 4096;
const NVS_TYPE_U8    = 0x01;  /*!< Type uint8_t */
const NVS_TYPE_I8    = 0x11;  /*!< Type int8_t */
const NVS_TYPE_U16   = 0x02;  /*!< Type uint16_t */
const NVS_TYPE_I16   = 0x12;  /*!< Type int16_t */
const NVS_TYPE_U32   = 0x04;  /*!< Type uint32_t */
const NVS_TYPE_I32   = 0x14;  /*!< Type int32_t */
const NVS_TYPE_U64   = 0x08;  /*!< Type uint64_t */
const NVS_TYPE_I64   = 0x18;  /*!< Type int64_t */
const NVS_TYPE_STR   = 0x21;  /*!< Type string */
const NVS_TYPE_BLOB  = 0x42;  /*!< Type blob */

const NVSK_DOWNLOAD	    = "/keydump/";

function ws_send(msg)
	{
	if(websocket.readyState == WebSocket.OPEN)
		websocket.send(msg);
	else
		alert("not connected to websocket server");
	}
function ws_receive(msg)
	{
	console.log('RECEIVED:', msg.data);
    const parsed = parseAppProto(msg.data);
	if(parsed.ret != 0)
		return;
    if(parsed.msg.command == RSP_CMD)
        {
        if(parsed.msg.params[0] == CMD_CREATEKEY)
            {
            alert("Create NVS key \"" + parsed.msg.params[1] + "\": " + parsed.msg.params[2] + " - " + parsed.msg.params[3]);
            if(parsed.msg.params[2] == "0") //success
                location.reload();
            }
        if(parsed.msg.params[0] == CMD_DELTENS || parsed.msg.params[0] == CMD_DELETEKEY)
            {
            if(parsed.msg.params[2] == "0") //success
                location.reload();
            else
                alert("error deleting from NVS " + parsed.msg.params[2] + " - " + parsed.msg.params[3]);
            }
        else if(parsed.msg.params[0] == CMD_UPDATEKEY)
            {
            if(parsed.msg.params[2] == "0") //success
                removeupdate(parsed.msg.params[1]);
            }
        else if(parsed.msg.params[0] == CMD_UPDATEKEYREQ)
            {
            if(parsed.msg.params[2] == "0")
                send_key_chunks(parsed.msg);
            else
                alert("Update key request error: " + parsed.msg.params[2] + " - " + parsed.msg.params[3]);
            }
        }
    else if (parsed.msg.command == URC_STATUS)
        {
        if(parsed.msg.params[0] == OP_UPDATEKEY &&
            parsed.msg.params[1] == PAR_PROGRESS &&
                parsed.msg.params[2] == "1")
			{
            removeupdate(parsed.msg.params[3]);
            }
        else if(parsed.msg.params[0] == OP_CONFIG)
            {
            chunk_size = Number(params[1]);
            max_blob_size = Number(params[2]);
            }
        }
	}
function send_key_chunks(msg)
    {
    var len = Number(document.getElementById("l_" + msg.params[1]).innerHTML);
    type = Number(document.getElementById("t_" + msg.params[1]).attributes.name.value);
    var pos = 0, l = 0;
    if(type == NVS_TYPE_STR) // NVS_TYPE_STR)
        {
        var b = document.getElementById(msg.params[1]).value;
        while (pos < len)
            {
            if(pos + chunk_size < b.length)
                l = chunk_size;
            else 
                l = b.length - pos;
            var msgOut = createMessage(CMD_UPDATEKEYVAL, [msg.params[1], pos, l],  b.substring(pos, pos + l));
            var buffer = buildAppProto(msgOut);
            ws_send(buffer);
            pos += l;
            }
        }
    else if(type == NVS_TYPE_BLOB)
        {
        const bytes = document.getElementById(msg.params[1]).blobData;
        let offset = 0;
        while(offset < bytes.length)
            {
            const chunkLen =  Math.min(chunk_size, bytes.length - offset);
            const chunk = bytes.slice(offset, offset + chunkLen);
            const msgOut = createMessage(CMD_UPDATEKEYVAL, [msg.params[1], offset, chunkLen], chunk);
            const buffer = buildAppProto(msgOut);
            ws_send(buffer);
            offset += chunkLen;
            }
        }
    }
function pushupdate(id)
    {
    const idx = updid.indexOf(id);
    if(idx == -1)
        updid.push(id);
    }
function removeupdate(id)
    {
    document.getElementById(id).style.color = "black";
    const idx = updid.indexOf(id);
    if(idx != -1)
        {
        updid.splice(idx, 1);
        if(updid.length == 0)
            document.getElementById("commit_ch").disabled = true;
        }
    }
function commitc()
    {
    for(i = 0; i < updid.length; i++)
        {
        var idc = updid[i].split("_");
        var type = document.getElementById("t_" + idc[0] + "_" + idc[1]).attributes.name.value;
        var typen = Number(type);
        len = Number(document.getElementById("l_" + idc[0] + "_" + idc[1]).innerHTML);
        if(type > NVS_TYPE_I64)
            var msgOut = createMessage(CMD_UPDATEKEYREQ, [updid[i], type, len]);
        else
            {
            const buf = new ArrayBuffer(8);
            const view = new DataView(buf);
            var val = BigInt(document.getElementById(updid[i]).value);
            view.setBigUint64(buf, val, true);
            var msgOut = createMessage(CMD_UPDATEKEY, [updid[i], type],  new Uint8Array(buf));
            }

        var buffer = buildAppProto(msgOut);
        ws_send(buffer);
        }
    }
function keysDecHandler(event)
    {
    const e = event.target;
    const typed = String(event.data);
    if(e.classList.contains("ued") ||
        e.classList.contains("ied") ||
            e.classList.contains("sed"))
        {
        var start = e.selectionStart;
        var upd = false;
        if(e.className == "sed")
            {
            upd = true;
            document.getElementById("nk_len").value = e.value.length;
            }
        else if((event.inputType == "deleteContentBackward" || event.inputType == "deleteContentForward") ||
            ('0123456789'.indexOf(typed) >= 0))
            upd = true;
        else if(e.className == "ied")
            {
            if(typed == "-" && start == 0 && target.value.indexOf("-") == -1)
                upd = true;
            }
        if(upd)
            {
            e.style.color = "rgb(255, 0, 0)";
            document.getElementById("commit_ch").disabled = false; 
            //pushupdate(target.id);     
            }
        else {event.stopPropagation(); event.preventDefault();}
        }
    }
function update_len(event)
    {
    var v = Number(document.getElementsByName("types")[0].value);
    if(event.target.id == "phv" &&
        Number(v) == NVS_TYPE_STR)
        {
        document.getElementById("nk_len").value = document.getElementById(event.target.id).value.length;
        }
    }
function pageload()
    {
    document.addEventListener("beforeinput", keysDecHandler);
    document.addEventListener("input", update_len);
    var inp = document.getElementsByClassName("hed");
    for (i = 0; i < inp.length; i++)
        {
        inp[i].blobData = parseHexText(inp[i].value);
        inp[i].addEventListener("keydown", hexKeyDown);
        inp[i].addEventListener("paste", hexPaste);
        inp[i].spellcheck = false;
        inp[i].wrap = "off";
        }
    inp = document.querySelectorAll(".sel2del, .sel2delns");
    for (i = 0; i < inp.length; i++)
        {
        inp[i].addEventListener("click", setSel);
        }

    const wsUri = window.location.origin + "/ws";
	websocket = new WebSocket(wsUri);
	websocket.addEventListener("open", () => {console.log("CONNECTED"); c_connected = true;});
	websocket.addEventListener("close", (event) => {console.log("DISCONNECTED: " + event.data); c_connected = false;});
	websocket.onmessage = (msg) => {ws_receive(msg);};
    window.addEventListener('beforeunload', function (event) 
        {
        if(document.getElementById("commit_ch").disabled == false)
            {
            event.preventDefault();
            event.returnValue = "Are you sure you want to leave this page?"; //anyway this prompt is ignored
            }
        else
            return;
        });
    }
function setSel(e)
    {
    const name = this.name;
    const ch = this.checked;
    var cb = document.getElementsByName(name);
    if(this.className == "sel2delns")
        {
        for (i = 0; i < cb.length; i++)
            cb[i].checked = ch;
        }
    else if(this.className == "sel2del")
        {
        cb = document.getElementsByClassName("sel2delns");
        for (i = 0; i < cb.length; i++)
            {
            if(cb[i].name == name)
                {
                var pcb = cb[i];
                break;
                }
            }
        cb = document.getElementsByClassName("sel2del");
        for (i = 0; i < cb.length; i++)
            {
            if(cb[i].name == name && cb[i].checked != ch)
                {
                pcb.indeterminate = true;
                break;
                }
            }
        if(i >= cb.length)
            {
            pcb.indeterminate = false;
            pcb.checked = ch;
            }
        }
    }

function updatelen(e) 
    {
    var target = e.target;
    var l = target.value.length;
    document.getElementById("l_" + e.target.id).innerHTML = l; 
    }

function showhide(ns)
    {
    var collection = document.getElementsByClassName(ns);
    if(anch = document.getElementById(ns))
        {
        if(anch.innerHTML == "+") {disp = ""; anch.innerHTML = "-";}
        else {disp = "none"; anch.innerHTML = "+";}
        for(var i = 0; i < collection.length; ++i)
            collection[i].style.display = disp;
        }
    }

function loadf(tarea, event)
	{
    const file = event.target.files[0];
    if(!file)
        return;
    const label = document.getElementById("u_" + tarea);
    if(label)
        label.innerHTML = file.name;

    const textarea = document.getElementById(tarea);
    if(!textarea)
		{
        console.log("textarea not found:", tarea);
        return;
		}
    const lenLabel = document.getElementById("l_" + tarea);
    const reader = new FileReader();
    document.body.style.cursor = "wait";
    reader.onload = function(e)
		{
        try
			{
            const arrayBuffer = e.target.result;
            const bytes = new Uint8Array(arrayBuffer);
            let out = "";
            for(let i = 0; i < bytes.length; i++)
				{
                out += bytes[i].toString(16).padStart(2, "0") + " ";

                // group every 8 bytes
                if((i + 1) % 8 === 0 &&
                   (i + 1) % 16 !== 0)
                    out += "- ";

                // newline every 16 bytes
                if((i + 1) % 16 === 0)
                    out += "\n";
            }
            textarea.value = out.trimEnd();

            // auto-adjust rows
            //textarea.rows =
            //    Math.max(1, Math.ceil(bytes.length / 16));

            textarea.style.color = "rgb(255, 0, 0)";

            if(lenLabel)
                lenLabel.innerHTML = bytes.length;

            document.getElementById("commit_ch").disabled = false;
            pushupdate(tarea);
            console.log("read " + bytes.length + " bytes");
			}
        finally{document.body.style.cursor = "default";}
		};
    reader.onerror = function()
		{
        document.body.style.cursor = "default";
        alert("Error reading file");
		};

    reader.readAsArrayBuffer(file);
	}
function seltypes()
    {
    var v = Number(document.getElementsByName("types")[0].value);
    var l = document.getElementById("nk_len");
    var ta = document.getElementById("phv");
    ta.value = "";
    switch(Number(v))
        {
        case 1: case 17:
            l.value = 1; l.readOnly = true; break;
        case 2: case 18:
            l.value = 2; l.readOnly = true; break;
        case 4: case 20:
            l.value = 4; l.readOnly = true; break;
        case 8: case 24:
            l.value = 8; l.readOnly = true; break;
        default:
            l.value = 0; l.readOnly = false; break;
        }
    ta.classList.remove("ied", "ued", "sed", "hed");
    switch(Number(v))
        {
        case 1: case 2: case 4: case 8:
            ta.classList.add("ued");
            break;
        case 17: case 18: case 20: case 24:
            ta.classList.add("ied");
            break;
        case 33:
            ta.classList.add("sed");
            break;
        case 66:
            ta.classList.add("hed");
            break;
        }
    }
function createnk()
    {
    var alertstr = "";
    var newns = document.getElementById("newns").value;
    var newkey = document.getElementById("newkey").value;
    var type = Number(document.getElementById("type").value);
    var kl = Number(document.getElementById("nk_len").value);
    var val = document.getElementById("phv").value;
    if(newns.length > 16 || newns.length == 0)
        alertstr += "namespace length is 0 or exceeds NVS_NS_NAME_MAX_SIZE (16)"  + "\n";
    if(newkey.length > 16 || newkey.length == 0)
        alertstr += "key name length is 0 or exceeds NVS_KEY_NAME_MAX_SIZE (16)" + "\n";
    if(Number(kl) <= 0)
        alertstr += "length must be 1 or larger" + "\n";
    if(type == NVS_TYPE_STR && kl >= 4000)
        alertstr += "for NVS_TYPE_STR key length must less than 4000" + "\n";
    if(val.length > chunk_size)
        alertstr += "value(placeholder) length is limited by application to max " + chunk_size + " bytes\n";
    if(alertstr.length == 0)
        {
        var msgOut = createMessage(CMD_CREATEKEY, [newkey, newns, type, kl], val);
        var buffer = buildAppProto(msgOut);
        ws_send(buffer);
        }
    else
         alert("ERROR(s):" + "\n" + alertstr);
    }
function delsel()
    {
    var nsc = document.getElementsByClassName("sel2delns");
    var ns;
    for(var i = 0; i < nsc.length; i++)
        {
        ns = nsc[i].name.split("ns_")[1];
        if(nsc[i].checked == true)
            {
            //delete all keys in the namespace
            var msgOut = createMessage(CMD_DELTENS, [ns]);
            var buffer = buildAppProto(msgOut);
            ws_send(buffer);
            }
        else if(nsc[i].indeterminate == true)
            {
            //only part of keys are marked for deletion
            var kc = document.getElementsByClassName("sel2del");
            for(var j = 0; j < kc.length; j++)
                {
                if(kc[j].name == nsc[i].name && kc[j].checked == true)
                    {
                    id = (kc[j].id.split("sd_")[1]).split("_");;
                    var msgOut = createMessage(CMD_DELETEKEY, [id[0], id[1]]);
                    var buffer = buildAppProto(msgOut);
                    ws_send(buffer);
                    }
                }
            }
        }
    }
function dump(id)
    {
    const name = document.getElementById("kn_" + id).innerHTML;
    const a = document.createElement("a");
    a.href = location.origin + NVSK_DOWNLOAD + encodeURIComponent(id);
    a.download = name + ".bin";
    a.click();
    }
