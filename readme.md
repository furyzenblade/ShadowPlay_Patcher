# ShadowPlay Patcher
This program can get rid of two pesky checks that one of the ShadowPlay processes does to stop the 
instant replay or active recording from stopping when the following happens:
1. An invisible window exists/ is currently on screen (for instance a password manager, that would set a certain window flag)
2. Protected content is viewed (when visiting the netflix website from a web browser for example)

Which will get around the issue of the instant replay being interrupted because one of these events just happened to occur 
(ShadowPlay does not automatically re-enable the instant replay or continue a recording once it has gotten interrupted, 
which is slightly irritating).

## How to use
1. Download the executable `ShadowPlay_Patcher.exe` from the Releases tab
2. Run the executable either by itself or from a terminal/ console
3. You can optionally also run it with the argument `--no-wait-for-keypress` to surpress it waiting for user input before exiting. Like this: \
    `.\ShadowPlay_Patcher.exe --no-wait-for-keypress`

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