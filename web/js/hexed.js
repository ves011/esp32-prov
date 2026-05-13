
// --------------------------------------------------
// Simple overwrite-style hex editor
// Backing store = Uint8Array
// --------------------------------------------------
function parseHexText(txt)
    {
    const hex = txt
        .replace(/[\s-]+/g, " ")
        .trim()
        .split(" ")
        .filter(Boolean);

    return Uint8Array.from(hex.map(v => parseInt(v, 16)));
    }

function createHexEditor(textarea, bytes)
    {
    textarea.blobData = new Uint8Array(bytes);
    renderHexEditor(textarea);
    textarea.addEventListener("keydown", hexKeyDown);
    textarea.addEventListener("paste", hexPaste);
    textarea.spellcheck = false;
    textarea.wrap = "off";
    }


// --------------------------------------------------
// Render
// --------------------------------------------------

function renderHexEditor(ta)
    {
    const b = ta.blobData;
    let out = "";
    for(let i = 0; i < b.length; i++)
        {
        out += b[i].toString(16).padStart(2, "0").toUpperCase();
        out += " ";
        // separator every 8 bytes
        if((i + 1) % 8 === 0 &&
           (i + 1) % 16 !== 0)
            out += "- ";
        // newline every 16 bytes
        if((i + 1) % 16 === 0)
            out += "\n";
        }
    ta.value = out.trimEnd();
    }


// --------------------------------------------------
// Helpers
// --------------------------------------------------

function isHexChar(c)
    {
    return /^[0-9a-fA-F]$/.test(c);
    }

function cursorToNibble(ta, cursor)
    {
    let nibble = 0;
    for(let i = 0; i < cursor; i++)
        {
        if(isHexChar(ta.value[i]))
            nibble++;
        }
    return nibble;
    }

function nibbleToCursor(ta, nibbleTarget)
    {
    let nibble = 0;
    for(let i = 0; i < ta.value.length; i++)
        {
        if(isHexChar(ta.value[i]))
            {
            if(nibble === nibbleTarget)
                return i;
            nibble++;
            }
        }
    return ta.value.length;
    }


// --------------------------------------------------
// Key editing
// --------------------------------------------------

function hexKeyDown(e)
    {
    const ta = e.target;

    // allow navigation keys
    if(["ArrowLeft", "ArrowRight", "ArrowUp", "ArrowDown", "Tab", "Home", "End"].includes(e.key))
        return;

    e.preventDefault();

    // overwrite with 00
    if(e.key === "Backspace" ||
       e.key === "Delete")
        {
        let nibblePos = cursorToNibble(ta, ta.selectionStart);
        if(e.key == "Backspace")
            {
            if(nibblePos >=2) nibblePos = nibblePos - 2;
            else return;
            }
        const bytePos = Math.floor(nibblePos / 2);
        if(bytePos >= ta.blobData.length)
            return;

        ta.blobData[bytePos] = 0;
        renderHexEditor(ta);
        const c = nibbleToCursor(ta, bytePos * 2);
        ta.selectionStart = ta.selectionEnd = c;
        }
    else if(isHexChar(e.key))
        {
        const nibblePos = cursorToNibble(ta, ta.selectionStart);
        const bytePos = Math.floor(nibblePos / 2);
        if(bytePos >= ta.blobData.length)
            return;

        let v = ta.blobData[bytePos];
        const nibble = parseInt(e.key, 16);
        if((nibblePos % 2) === 0) // high nibble
            v = (v & 0x0F) | (nibble << 4);
        else // low nibble
            v = (v & 0xF0) | nibble;
        ta.blobData[bytePos] = v;
        renderHexEditor(ta);
        const nextCursor = nibbleToCursor(ta, nibblePos + 1);
        ta.selectionStart = ta.selectionEnd = nextCursor;
        }
    ta.style.color = "rgb(255,0,0)";
    document.getElementById("commit_ch").disabled = false;
    pushupdate(ta.id);
    }


// --------------------------------------------------
// Paste
// --------------------------------------------------

function hexPaste(e)
    {
    e.preventDefault();
    const ta = e.target;
    const text = (e.clipboardData || window.clipboardData).getData("text");

    // remove everything except hex
    const clean = text.replace(/[^0-9a-fA-F]/g, "");
    if(clean.length === 0)
        return;

    let nibblePos = cursorToNibble(ta, ta.selectionStart);
    for(let i = 0; i < clean.length; i++)
        {
        const bytePos = Math.floor(nibblePos / 2);
        if(bytePos >= ta.blobData.length)
            break;
        let v = ta.blobData[bytePos];
        const nibble = parseInt(clean[i], 16);
        if((nibblePos % 2) === 0) {v = (v & 0x0F) | (nibble << 4);}
        else {v = (v & 0xF0) | nibble;}
        ta.blobData[bytePos] = v;
        nibblePos++;
        }
    renderHexEditor(ta);
    const c = nibbleToCursor(ta, nibblePos);
    ta.selectionStart = ta.selectionEnd = c;
    ta.style.color = "rgb(255,0,0)";
    document.getElementById("commit_ch").disabled = false;
    pushupdate(ta.id);
    }
