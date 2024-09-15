# ShadowPlay Patcher

This program injects into the Nvidia ShadowPlay

Normally, ShadowPlay will disable itself under two conditions:

1. A window which has requested to be hidden from screen captures exists
   - For example password managers like KeePassXC, but also some helper windows like the Zoom "You are currently sharing your screen" overlay
2. DRM-Protected (Widevine) Content is playing
   - For example watching Netflix

Which will get around the issue of the instant replay being interrupted because one of these events just happened to occur 
(ShadowPlay does not automatically re-enable the instant replay or continue a recording once it has gotten interrupted, 
which is slightly irritating).

## How to use
1. Download the executable `ShadowPlay_Patcher.exe` from the Releases tab
2. Run the executable either by itself or from a terminal/console
3. You can optionally also run it with the argument `--no-wait-for-keypress` to surpress it waiting for user input before exiting. Like this: \
    `.\ShadowPlay_Patcher.exe --no-wait-for-keypress`

The patch only modifies the running process and does not persist any changes to disk, meaning that it will have to be re-applied after every system reboot.  
You may want to auto-start the patcher, for example by dropping the exe (or a shortcut to it) into  
`%appdata%\Microsoft\Windows\Start Menu\Programs\Startup`

## How to compile
1. Open the solution file in VS
2. Make sure to have installed:
    - Windows SDK Version: 10.0 (latest installed version)
    - Platform Toolset: Visual Studio 2022 (v143) \
    _(though using older versions will probably still work fine)_
3. Build

## How does it work?
[Read about it here](./re_research.md)

## Todo
There is a lot of room for improvement. The below listed things would be nice to have

- [ ] Detect if the targetted ShadowPlay process has already been patched
- [ ] Add option to be able to undo applied patches/ hooks
- [ ] Use a memory editing library to simplify doing things like
    - enumerate processes
    - enumerating a processes loaded modules
    - doing a basic trampoline hook
- [ ] Add option to copy the executable into the startup folder