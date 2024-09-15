### Process
The shadowplay process which performs the invisible window and protected content checks is called
`nvcontainer.exe`.

Multiple `nvcontainer` processes run at the same time. The right one contains `SPUser` in its command line args, like this:
```bash
"C:\Program Files\NVIDIA Corporation\NvContainer\nvcontainer.exe" -f "C:\ProgramData\NVIDIA\NvContainerUser%dSPUser.log" -d "C:\Program Files\NVIDIA Corporation\NvContainer\plugins\SPUser" -r -l 3 -p 30000 -st "C:\Program Files\NVIDIA Corporation\NvContainer\NvContainerTelemetryApi.dll" -c
```
\
It also has the module `nvd3dumx.dll` loaded into its process.

### Checks
The above described `nvcontainer.exe` process does the following checks which end up interrupting the recording.
#### Invisible window check
- Iterate through all visible Windows
- If any of them have a [Window Display Affinity](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-getwindowdisplayaffinity) other than `WDA_NONE`, the check triggers

This check stops Instant Replay, including creating a "Instant Replay is now off" notification.
Instant replay does not get automatically re-enabled even after the check clears.

Relevant functions used:
- USER32 `GetWindowDisplayAffinity`

#### Protected content check

- Iterate through all processes
- Check if the executable name is any of the following:
  - `msedge.exe` `chrome.exe` `firefox.exe` `plugin-container.exe` `iexplore.exe` `opera.exe`
- If the executable name matches any of the above, iterate through all modules loaded into the process
- If any of the loaded modules are named `widevinecdm.dll`, the check triggers.

This check doesn't normally stop Instnat Replay. Instead, capturing is silently paused.  
The GeForce Experience overlay continues showing it as "On", but saving the replay is disabled and no frames are actually captured.  
After the check clears, capturing will silently resume. When saving a replay, the part where capturing was paused will appear to be cut out.  
This check works because browsers appear to only keep the the WidevineCDM (Content Decryption Module) loaded while it's actually needed to decrypt Widevine content.

Relevant functions used:
- KERNEL32 `CreateToolhelp32Snapsho`, `Process32FirstW`, `Process32NextW`, `Module32FirstW`, `Module32NextW`

### The Patch

By just finding the correct process and modifying its loaded instance of the
- USER32 `GetWindowDisplayAffinity` and
- KERNEL32 `Process32FirstW`

functions to immediately return, both checks will never trigger.  
This is also elegant because it's robust against Nvidia driver upgrades, and should work against all versions of the Nvidia driver that don't directly change how the checks work or have major differences in the process structure.

### Caveats
In testing, an issue was encountered where the Netflix Windows desktop app would fail to play video while the patch was injected, but this has disappeared when updating the Netflix app via the Microsoft store.
