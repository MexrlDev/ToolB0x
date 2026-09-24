# ToolB0x
* Project By MexrlDev

---

## How to send?
* For mobile use either Pythonica for iPhone or PyCode for Android.
* go to the release and get the zip file and unzip it
* make sure to add the bin in the same folder as the python and lua.

**IOS SetUp**
* Download Pythonica on iOS. And open it to auto create the DOC files, after that just open files in your iPhones and extract the zip folder and take that folder after extracting zip and put it in.. in my phone > pythonica > root > here. And just open pythonica > Files and folders find the folder you got the Lua Sender in and find the python and go to it then run the python..

**Android SetUp**
* Install an app called PyCode you can get it as APK nor Play Store.. anyways after you get it, just make sure to extract the zip file I’ve set in this release in download or any folder you want, then in the app click on the 3 lines up left, Open, choose the folder and open the path you extract the zip in, after this just make sure to click on the python file and just open it, Click that run icon. 

---

## Build
* Use the YML and bound it and you can see that zip file and take it and use it.

---

## Functions

### Controller
- Lightbar color changer (static presets + custom RGB + rainbow)
- Lightbar brightness dimmer (Dim 25% / Medium 50% / High 75% / Max 100%)
- Vibration presets (Weak / Medium / Strong / Left / Right motor)
- Animated vibration pulse
- Trigger effects (Feedback / Weapon / Vibration / Slope, per-trigger or both)
- DualSense speaker test (440 Hz / 660 Hz / 880 Hz / melody sweep)
- Live pad state viewer (per-button chips, raw bytes dump, press history)

### System
- System info panel (user ID, handles, DMEM size, FW raw, uptime, frames)
- Modules & kernel dumper (firmware version, module list with base addresses)
- Kernel symbol resolver
- Notification sender (custom text via keyboard, or quick presets)

### Memory
- Memory editor (hex viewer with cursor, edit bytes inline)
- Memory dump to UDP log
- Memory dump to file (`/savedata0/memdump.bin`)
- Jump to address via on-screen keyboard
- Write value via on-screen keyboard (u8 / u16 / u32 / u64)

### Video & Audio
- Video output tester (solid color clear: black, white, red, green, blue, gray)
- Audio output tester (220 Hz / 440 Hz / 880 Hz / 1760 Hz tones)
- Speaker output tester (DualSense speaker, per-port probe)

### Payloads
- Code launcher — type x86_64 hex shellcode, execute, get return value
- UDP keyboard injection (send text from PC to the on-screen keyboard)
- Optical drive eject / reload (probes all known FreeBSD device paths)

### Networking
- Local IP display
- UDP log test
- Debug log forwarding to PC (port 9027)
- Remote keyboard input listener (port 9031 while keyboard is open)

### UI
- Full 1920x1080 rendered menu
- 3-page virtual keyboard (letters / numbers / extended symbols)
- Hold-to-repeat navigation (D-Pad, Cross, Square, Triangle, L1, R1)
- Cursor-based text editing (L1 / R1 to move)
- Screen toggle mapping (L2 = SHIFT cycle)
- Breadcrumb navigation
- Toast notifications for action results

### And more
- Developer debug panel (FPS, frame timings, draw/flip ms, pad counters)
- Credits screen
- Exit to LuaC0re with automatic controller reset (default lightbar color restored)
- L1+R1 escape hatch from any screen back to the main menu
- R1 exits only from the main menu

---

## Credits
* Mexrldev - Project, debugging, etc

**Special Thanks To**
  - Egycnq for emuc0re
  - Deepseek for development
