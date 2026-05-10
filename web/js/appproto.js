export const SEP = "\x01";
export const MAX_PARAMS = 10;
export const MAX_TOKENS = 32;
export const PROTO_VERSION = 1;

export class AppProto {
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

export const PAR_PROGRESS = "progress";
export const PAR_ERROR    = "error";

export const CMD_SETBOOT       = "set boot";
export const CMD_ERASEPART     = "erase partition";
export const CMD_REQCONF       = "request confirmation";
export const CMD_UPDATEKEYREQ  = "update key req";
export const CMD_UPDATEKEY     = "update key";
export const CMD_DELTENS       = "delete ns";
export const CMD_DELETEKEY     = "delete key";
export const CMD_CREATEKEY     = "create key";

export const RSP_CMD           = "rsp_cmd";
export const RSP_CONFIRMATION  = "rsp_conf";

export const URC_STATUS = "urc_status";

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

export function createMessage(command, params = [], payload = null) 
    {
    const msg = new AppProto();
    msg.timestamp = Date.now();
    msg.command = command;
    msg.params = params;
    msg.nparams = params.length;
    if (payload) 
        {
        msg.payload = payload;
        msg.payload_len = payload.length;
        }
    msg.hdr_fields = 5 + msg.nparams;
    return msg;
    }

export function buildAppProto(msg) 
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
        buffer.set(payload, headerBytes.length);
    
    return buffer;
    }

function parseAppProto(buffer) 
    {
    let ret = -1;
    if (!buffer || buffer.byteLength === 0)
        return ret;

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