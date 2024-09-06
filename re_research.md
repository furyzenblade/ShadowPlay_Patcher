The shadowplay process in which the invisible window and protected content checks are contained is called
`nvcontainer.exe`, but multiple `nvcontainer` processes run at the same time.

The right one contains `SPUser` in its command line args, like this:
```bash
"C:\Program Files\NVIDIA Corporation\NvContainer\nvcontainer.exe" -f "C:\ProgramData\NVIDIA\NvContainerUser%dSPUser.log" -d "C:\Program Files\NVIDIA Corporation\NvContainer\plugins\SPUser" -r -l 3 -p 30000 -st "C:\Program Files\NVIDIA Corporation\NvContainer\NvContainerTelemetryApi.dll" -c
```
\
It also has the module `nvd3dumx.dll` loaded into its process.

The above described `nvcontainer.exe` process does the following checks which end up interrupting the recording:
1. Invisible window check
    - placeholder
    - placeholder

2. Protected content check
    - placeholder
    - placeholder