const SEP = "\x01";
const MAX_PARAMS = 10;
const MAX_TOKENS = 32;
const PROTO_VERSION = 1;

class AppProto {
    constructor() 
        {
        this.version = PROTO_VERSION;
        this.hdr_fields = 0;
        this.payload_len = 0;
        this.timestamp = 0;

        this.command = "";

        this.nparams = 0;
        this.params = [];   // array of strings

        this.payload = null; // Uint8Array or null
        }
    }

//Parameters
const PAR_PROGRESS = "progress";
const PAR_ERROR    = "error";

//Operations
const OP_UPLOAD     = "upload";
const OP_DOWNLOAD	= "download";
const OP_UPDATEKEY	= "update key"

const CMD_SETBOOT       = "set boot";
const CMD_ERASEPART     = "erase partition";
const CMD_REQCONF       = "request confirmation";
const CMD_UPDATEKEYREQ  = "update key req";
const CMD_UPDATEKEYVAL  = "update key val";
const CMD_UPDATEKEY     = "update key";
const CMD_DELTENS       = "delete ns";
const CMD_DELETEKEY     = "delete key";
const CMD_CREATEKEY     = "create key";
const CMD_REBOOT		= "esp_reboot";

const RSP_CMD           = "rsp_cmd";
const RSP_CONFIRMATION  = "rsp_conf";

const URC_STATUS        = "urc_status";

/*
Frame format:
  header \x01 payload(optional)

Header fields:
version \1 hdr_fields \1 payload_len \1 timestamp \1 command \1 param1 \1 ... paramN \1 payload

Rules:
- hdr_fields = total number of header fields (>=5)
- payload_len = number of bytes after last separator
- timestamp = ms (Date.now() or device time)
- params = command-specific
- payload = raw binary (Uint8Array)
*/

function createMessage(command, params = [], payload = null) 
    {
    const msgOut = new AppProto();
    msgOut.timestamp = Date.now();
    msgOut.command = command;
    msgOut.params = params;
    msgOut.nparams = params.length;
    if (payload) 
        {
        msgOut.payload = payload;
        msgOut.payload_len = payload.length;
        }
    msgOut.hdr_fields = 5 + msgOut.nparams;
    return msgOut;
    }

function buildAppProto(msg) 
    {
    if (!msg || !msg.command)
        throw new Error("Invalid message");

    // -------------------------
    // VALIDATION
    // -------------------------
    const nparams = msg.params ? msg.params.length : 0;
    const expectedHdrFields = 5 + nparams;

    if (msg.hdr_fields !== expectedHdrFields) 
        throw new Error(`hdr_fields mismatch: got ${msg.hdr_fields}, expected ${expectedHdrFields}`);


    const payload = msg.payload || null;
    const payloadLen = payload ? payload.length : 0;

    if (msg.payload_len !== payloadLen)
        throw new Error(`payload_len mismatch: got ${msg.payload_len}, actual ${payloadLen}`);

    // -------------------------
    // BUILD HEADER STRING
    // -------------------------
    let parts = [];
    parts.push(String(msg.version));
    parts.push(String(msg.hdr_fields));
    parts.push(String(msg.payload_len));
    parts.push(String(msg.timestamp));
    parts.push(msg.command);

    for (let i = 0; i < nparams; i++) 
        parts.push(msg.params[i]);

    // IMPORTANT: add trailing separator before payload
    let headerStr = parts.join(SEP) + SEP;

    // -------------------------
    // ENCODE HEADER
    // -------------------------
    const encoder = new TextEncoder(); // UTF-8
    const headerBytes = encoder.encode(headerStr);

    // -------------------------
    // FINAL BUFFER
    // -------------------------
    const totalLen = headerBytes.length + payloadLen;
    const buffer = new Uint8Array(totalLen);

    // copy header
    buffer.set(headerBytes, 0);
    // copy payload
    if (payloadLen > 0)
        {
        let payloadBytes;
        if(payload instanceof Uint8Array)
            payloadBytes = payload;
        else if(typeof payload === "string")
            {
            const encoder = new TextEncoder();
            payloadBytes = encoder.encode(payload);
            }
        else
            throw new Error("Unsupported payload type");
    
        buffer.set(payloadBytes, headerBytes.byteLength);
        }
    return buffer;
    }

function parseAppProto(buffer) 
    {
    let ret = -1;
    if (!buffer || buffer.byteLength === 0)
        return ret;
    let out = new AppProto;
    let params = buffer.split(SEP);
    if(params.length >= 5)
        {
        out.hdr_fields = parseInt(params[1], 10);
        out.payload_len = parseInt(params[2], 10);
        out.timestamp = BigInt(params[3]);
        out.command = params[4];
        out.nparams = out.hdr_fields - 5;
        for(let i = 0; i < out.nparams; i++)
            out.params[i] = params[i + 5];
        ret = 0;
        if(out.payload_len > 0)
            {
            let payloadStart = 0, sepCount = 0;
            while(payloadStart != -1 && payloadStart < buffer.length)
                {
                payloadStart = buffer.indexOf(SEP, payloadStart);
                if(payloadStart != -1)
                    {
                    sepCount++;
                    payloadStart++;
                    if(sepCount == out.hdr_fields)
                        break;
                    }
                }
            if(sepCount == out.hdr_fields && payloadStart + out.payload_len <= buffer.length)
                {
                const bytes = buffer instanceof Uint8Array ? buffer : new Uint8Array(buffer);
                out.payload = bytes.slice(payloadStart, payloadStart + out.payload_len);
                }
            else
                {
                ret = -1;
                console.log("websocket message malformated: payload");
                }
            }
        }
    else
        console.log("websocket message malformated: not enough fields in the header");

    return {ret, msg: out};
    }  

/*
    // Work with Uint8Array
    const bytes = buffer instanceof Uint8Array ? buffer : new Uint8Array(buffer);

    let tokens = [];
    let cur = 0;
    let hdrFields = -1;

    // --- STEP 1: parse header tokens ---
    while (cur < bytes.length && tokens.length < MAX_TOKENS) 
        {
        let start = cur;
        let sepIdx = -1;

        // find next SEP
        for (let i = cur; i < bytes.length; i++) {
            if (bytes[i] === 0x01) {
                sepIdx = i;
                break;
            }
        }

        if (sepIdx === -1) break;

        // extract token (ASCII)
        let token = new TextDecoder().decode(bytes.slice(start, sepIdx));
        tokens.push(token);

        cur = sepIdx + 1;

        // after 2nd token → we know hdr_fields
        if (tokens.length === 2) 
            {
            hdrFields = parseInt(tokens[1], 10);
            if (isNaN(hdrFields) || hdrFields < 5 || hdrFields > MAX_TOKENS)
                return { ret: -2 };
            }

        if (hdrFields > 0 && tokens.length === hdrFields)
            break;
        }

    // --- STEP 2: validate ---
    if (tokens.length < 5 || tokens.length !== hdrFields || (tokens.length - 5) > MAX_PARAMS)
        return -3;
    

    // --- STEP 3: fill structure ---
    const out = {
        version: parseInt(tokens[0], 10),
        hdr_fields: hdrFields,
        payload_len: parseInt(tokens[2], 10),
        timestamp: BigInt(tokens[3]),
        command: tokens[4],
        nparams: tokens.length - 5,
        params: tokens.slice(5),
        payload: null
        };

    // --- STEP 4: payload handling ---
    if (out.payload_len > 0) 
        {
        if (cur + out.payload_len <= bytes.length) 
            {
            out.payload = bytes.slice(cur, cur + out.payload_len);
            ret = 0;
            } 
        else
            ret = -4;
        } 
    else 
        {
        out.payload = null;
        ret = 0;
        }
    return { ret, msg: out };    
    }
*/