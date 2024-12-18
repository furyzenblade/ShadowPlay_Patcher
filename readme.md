# ShadowPlay Patcher

This program injects into Nvidia ShadowPlay to prevent ShadowPlay Instant Replay from disabling itself.  
No more missing good clips because your computer is forbidding you from recording!

## The situation

Normally, ShadowPlay will disable Instant Replay when detecting "protected content", which can mean two things:

1. A window which has requested to be hidden from screen captures exists
   - For example password managers like KeePassXC, but also some helper windows like the Zoom "You are currently sharing your screen" overlay
2. DRM-Protected (Widevine) Content is playing
   - For example watching Netflix

This is annoying, especially as Instant Replay doesn't always get re-enabled even after "protected content" disappears.  
Users previously tried to remedy the issue by automatically trying to re-enable Instant Replay when it's disabled, see [AlwaysShadow](https://github.com/Verpous/AlwaysShadow/).

This project instead addresses the issue directly by disabling the checks for protected content so Instant Replay will always keep recording.

## How to use
1. Download the executable `ShadowPlay_Patcher.exe` from the Releases tab
2. Run the executable
3. Look at the output. If you see no lines starting with "Error", everything succeeded and Instant Replay should stop turning itself off automatically.

The patch only modifies the running process and does not persist any changes to disk, meaning that it will have to be re-applied after every system reboot.  
You may want to auto-start the patcher, for example by dropping the exe (or a shortcut to it) into  
`%appdata%\Microsoft\Windows\Start Menu\Programs\Startup`  
You can add the CLI argument `--no-wait-for-keypress` to the shortcut target to make the window automatically close after patching has finished.

## How to compile
1. Open the solution file in VS
2. Make sure to have installed:
    - Windows SDK Version: 10.0 (latest installed version)
    - Platform Toolset: Visual Studio 2022 (v143) \
    _(though using older versions will probably still work fine)_
3. Build

## How does it work?
[Read a more complex explanation here](./re_research.md)

## Todo
There is a lot of room for improvement. The below listed things would be nice to have

- [ ] Detect if the targetted ShadowPlay process has already been patched
- [ ] Add option to be able to undo applied patches/ hooks
- [ ] Use a memory editing library to simplify doing things like
    - enumerate processes
    - enumerating a processes loaded modules
    - doing a basic trampoline hook
- [ ] Add option to copy the executable into the startup folder