# Link Vault — A Beginner's Guide

A handheld bookmark device with a touch screen. You tap a saved link, and it types the URL into whatever device you've paired it with — your phone, laptop, tablet, anything. It's like a tiny secure clipboard with a PIN.

**This guide assumes you've never built anything like this before.** Every step is explained. If you've done Arduino projects before, you can skip ahead — but the early sections are intentionally beginner-friendly.

---

## Table of Contents

1. [What you're building and why](#1-what-youre-building-and-why)
2. [What you'll need](#2-what-youll-need)
3. [Understanding the parts](#3-understanding-the-parts)
4. [Setting up your computer](#4-setting-up-your-computer)
5. [Getting the Waveshare demo working first](#5-getting-the-waveshare-demo-working-first)
6. [Installing the firmware libraries](#6-installing-the-firmware-libraries)
7. [Setting up the Link Vault project](#7-setting-up-the-link-vault-project)
8. [Configuring the build settings](#8-configuring-the-build-settings)
9. [Setting your default PINs and password](#9-setting-your-default-pins-and-password)
10. [Compiling and flashing](#10-compiling-and-flashing)
11. [First-run walkthrough](#11-first-run-walkthrough)
12. [Using the device every day](#12-using-the-device-every-day)
13. [Using with a laptop or desktop](#13-using-with-a-laptop-or-desktop)
14. [Hidden gestures](#14-hidden-gestures)
15. [Backup and restore](#15-backup-and-restore)
16. [Security model](#16-security-model)
17. [Troubleshooting](#17-troubleshooting)
18. [Glossary of terms](#18-glossary-of-terms)
19. [If something goes really wrong](#19-if-something-goes-really-wrong)

---

## 1. What you're building and why

### The problem this solves

When you bookmark a link in your browser, it's stored in lots of places — your browser's sync server, your phone's backup, your search history. If you want a link that doesn't show up anywhere networked, your options are limited.

### The solution

A small handheld device, about the size of a thick credit card, with a touchscreen. It holds your links offline. When you want to visit one, you unlock the device with a PIN, tap the link, and it types the URL onto your phone or computer over Bluetooth — like a tiny remote keyboard. Nothing gets synced. Nothing leaves the device unless you explicitly back it up yourself.

### Features (in plain English)

- **Multiple "vaults"** — up to 4 separate lists of links, each with its own PIN. Useful for keeping work links separate from personal links.
- **A decoy vault** — if someone forces you to unlock the device, you can enter a special PIN that opens a fake vault with throwaway links. They think they got in. They didn't.
- **A master password** — used to recover a forgotten PIN, but in a clever way: typing the wrong vault name silently fails. So even watching you use recovery tells an attacker nothing.
- **Proximity gate** — the device won't accept input until your trusted phone (or laptop) is nearby over Bluetooth. Like a car key fob.
- **Lockout after wrong PINs** — try 3 wrong PINs in a row, you get locked out for 30 seconds. 5 wrong, locked out for 5 minutes. Eventually 24 hours per attempt. Brute-forcing the PIN becomes practically impossible.
- **Cypherpunk visual style** — amber-on-black terminal aesthetic with scanlines, glitch text, animated boot sequence. Looks cool. Functions clearly.

### Why this is genuinely useful (not just a toy)

Once you have it, you'll find yourself using it for:

- Login pages for anything sensitive — typing your password manager URL into a fresh incognito tab without that URL ever appearing in browser history
- Sharing a long URL between devices without using clipboard sync
- Carrying personal links separately from work links so you can hand a colleague your laptop without your stuff being there
- Quick access to private bookmarks (medical resources, support communities, whatever) without those showing up in family-shared browser history

---

## 2. What you'll need

### Hardware (the physical things to buy)

| Item | Approximate cost | Where to get it | Notes |
|------|------|---|---|
| Waveshare ESP32-S3-Touch-LCD-3.49 board | $35-45 | Waveshare website, Amazon, AliExpress | This is the one specific component this guide is built around. Don't substitute. |
| USB-C cable (data, not just charging) | $5-15 | Anywhere | Must support data, not just power. Most USB-C cables that come with phones work. |
| Optional: 3.7V LiPo battery with MX1.25 connector | $5-15 | Adafruit, Amazon | If you want it portable. The board has a built-in charger and connector for this. |
| Optional: 3D-printed case | $0-30 | Print yourself, or order from Etsy | Not required. The board works fine bare. |

**You do NOT need:**
- A soldering iron (everything plugs in)
- Any electronics knowledge
- Any prior Arduino experience
- An Anthropic account, Google account, or any cloud service

### Software (free downloads)

- **Arduino IDE 2.x** — the program you'll use to send code to the device. Free. Available for Windows, Mac, Linux.
- **A web browser** — for downloading libraries and the Waveshare demo project.
- **A computer** — Windows, Mac, or Linux. Any reasonably modern one works.

### Time investment

- **Buying the parts**: 5 minutes online; 1-7 days to ship
- **Setting up Arduino IDE**: 15-30 minutes
- **Getting Waveshare demo working**: 30 minutes to 2 hours (this is the most variable step)
- **Flashing Link Vault firmware**: 15-30 minutes once the demo works
- **First-run setup**: 30 minutes

Plan on a full afternoon for the build the first time, less if you've done embedded work before.

---

## 3. Understanding the parts

This section explains what things are if you've never worked with this stuff before. Skip it if you have.

### What's an ESP32?

ESP32 is a family of small, cheap, capable microcontrollers — basically tiny computers without a screen, the size of a postage stamp. They have built-in WiFi and Bluetooth. They're what's inside countless smart-home gadgets, IoT sensors, and DIY electronics projects.

**ESP32-S3** is a newer, more powerful variant in that family. The "S3" specifically has more memory, faster processors, and a feature called PSRAM (extra memory) that lets it handle graphical interfaces.

### What's the Waveshare board?

Waveshare is a Chinese electronics company that takes ESP32 chips and builds them into ready-to-use development boards with extra hardware attached — in this case, a wide touch LCD screen, a battery connector, and a USB-C port. You don't have to wire anything up; it's all on one circuit board.

The specific board this project uses (`ESP32-S3-Touch-LCD-3.49`) has a unique 640×172 wide aspect-ratio touchscreen — long and short, like a tiny letterbox. This shape is why the UI we're building looks the way it does.

### What's "flashing"?

Putting code onto a microcontroller is called "flashing" because the code goes into a kind of memory called flash storage. You connect the device via USB, run a program (Arduino IDE), and it sends compiled code over the cable. Takes 15-30 seconds.

### What's a library?

In programming, a library is pre-written code that does something specific so you don't have to write it yourself. This project uses libraries for:
- Drawing the user interface (LVGL)
- Talking Bluetooth (NimBLE-Arduino, ESP32-BLE-Keyboard)
- Storing data on the device (LittleFS, ArduinoJson)
- Running a small webserver in config mode (ESPAsyncWebServer)

You install these once via Arduino IDE's library manager, and they're available to any project.

### What's "BLE" or "Bluetooth HID"?

BLE = Bluetooth Low Energy, the modern flavor of Bluetooth used for keyboards, headphones, fitness trackers, etc. HID = Human Interface Device — the standard category for keyboards, mice, and similar input devices. When this device acts as a "Bluetooth HID keyboard," it's pretending to be a regular wireless keyboard from the perspective of your phone or laptop.

### What's LittleFS?

A small filesystem that lives on the device itself. It's how the device remembers your links, PINs, and settings between reboots. Think of it like a tiny SD card built into the chip.

---

## 4. Setting up your computer

### Step 4.1: Install Arduino IDE 2.x

1. Go to **https://www.arduino.cc/en/software**
2. Download the version for your operating system (Windows / Mac / Linux)
3. Install it like any other application
4. Open it once to make sure it works. You'll see a code editor window.

The Arduino IDE is the program you'll spend most of your time in. It's where you write/paste code, where you click "compile" and "upload," and where error messages appear.

### Step 4.2: Add the ESP32 board manager URL

The Arduino IDE doesn't know about the ESP32 by default. You have to tell it where to find ESP32 support:

1. In Arduino IDE: **File → Preferences** (on Mac: **Arduino IDE → Settings**)
2. Find the field labeled **"Additional boards manager URLs"**
3. Paste this URL into that field:
   ```
   https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
   ```
4. Click OK

### Step 4.3: Install the ESP32 board package

1. In Arduino IDE: **Tools → Board → Boards Manager** (or click the boards icon in the sidebar)
2. Search for **"esp32"**
3. Find the entry from **Espressif Systems**
4. Make sure version is **3.0.0 or higher**
5. Click **Install**. This downloads about 200MB and takes a few minutes.

When done, you'll have access to dozens of ESP32-related boards in the Tools → Board menu. You won't pick one yet — that comes later.

### Step 4.4: Install USB drivers (Windows only)

If you're on Mac or Linux, skip this — USB drivers are built in.

If you're on Windows, you may need to install a USB driver so the IDE can see the device. The board uses a chip called CH343 or similar for USB. If when you plug it in, Windows says "device not recognized" or it doesn't show up as a COM port:

1. Go to **https://www.wch.cn/downloads/CH343SER_EXE.html**
2. Download and install the driver
3. Reboot Windows
4. Plug in the device — it should now show up as a COM port

---

## 5. Getting the Waveshare demo working first

**This is the most important step.** Before flashing Link Vault, you need to confirm the screen and touch work using Waveshare's own example code. This isolates "is the hardware working" from "is the Link Vault code working" — so if something breaks later, you know which one to blame.

The Waveshare board uses a specialized display chip called AXS15231B that no standalone Arduino library supports. You **must** start from Waveshare's demo project, which contains the driver code for this specific chip.

### Step 5.1: Download the Waveshare demo

1. Go to **https://www.waveshare.com/wiki/ESP32-S3-Touch-LCD-3.49**
2. Scroll down to the **"Resources"** or **"Demo"** section
3. Find the demo download link (usually labeled "Demo codes" or "Example programs")
4. Download the ZIP file (a few hundred MB)
5. Extract it somewhere you can find again — your Desktop is fine

### Step 5.2: Find the LVGL demo

Inside the extracted folder, look for one of these subfolders (the exact name varies by demo version):
- `Arduino` → `LVGL_Arduino`
- `Arduino` → `04_LVGL_Demo`
- `Arduino` → `LVGL_Test`

You want the one that uses **LVGL** (not the basic display test). LVGL is the graphics library this project depends on.

### Step 5.3: Copy the demo to your Arduino sketches folder

Arduino IDE expects projects to live in a specific place:

- **Windows**: `Documents\Arduino\`
- **Mac**: `~/Documents/Arduino/`
- **Linux**: `~/Arduino/`

Copy the entire LVGL demo folder into that location. The folder name will become the project name.

### Step 5.4: Open the demo in Arduino IDE

1. In Arduino IDE: **File → Open**
2. Navigate to the demo folder you just copied
3. Open the `.ino` file inside it (there's usually one, named the same as the folder)

You'll see lots of code. Don't worry about understanding it yet.

### Step 5.5: Set the board to ESP32-S3

In Arduino IDE: **Tools → Board → esp32 → ESP32S3 Dev Module**

### Step 5.6: Configure board settings

These settings are critical. In the **Tools** menu, set each one:

| Setting | Value |
|---------|-------|
| Board | ESP32S3 Dev Module |
| USB CDC On Boot | Enabled |
| USB Mode | Hardware CDC and JTAG |
| PSRAM | OPI PSRAM |
| Flash Size | 16MB (128Mb) |
| Partition Scheme | 16M Flash (3MB APP/9.9MB FATFS) |
| Upload Speed | 921600 |
| CPU Frequency | 240MHz (WiFi/BT) |

Some of these may be slightly named differently depending on Arduino IDE version. Pick the closest match. The PSRAM and Partition Scheme settings are the most critical — without them, the project won't fit.

### Step 5.7: Plug in the board and select the port

1. Plug the board into your computer with the USB-C cable
2. The screen may light up briefly (that's normal)
3. In Arduino IDE: **Tools → Port** — you should see a new entry. On Windows it'll be `COM3`, `COM4`, etc. On Mac/Linux it'll be `/dev/cu.usbmodem...` or `/dev/ttyUSB0`.
4. Select that port

If you don't see any port: the USB cable might be charge-only (try a different one), or — on Windows — you may need the CH343 USB driver from **https://www.wch.cn/downloads/CH343SER_EXE.html** (see Step 4.4 for full instructions), or the board isn't getting power (try a different USB port on your computer).

### Step 5.8: Compile the demo

Click the **checkmark icon** in the top-left (compile only, no upload). This translates the code from human-readable to machine code.

The first compile takes 5-10 minutes — it's compiling LVGL and all its dependencies for the first time. Subsequent compiles are much faster (10-30 seconds).

If it fails with errors:
- Check that you set all the Tools menu settings from Step 5.6
- Check that you installed the ESP32 board package from Step 4.3
- Read the error carefully — it usually tells you what's wrong (e.g., "Library X not found" means you need to install Library X)

If it succeeds, you'll see "Done compiling" at the bottom.

### Step 5.9: Upload the demo

Click the **right-arrow icon** (compile + upload). Compiles again (faster this time), then uploads to the device. Upload takes about 30 seconds.

If it fails to upload:
- Hold the **BOOT** button on the device while the IDE says "Connecting..." then release. Some boards need this to enter flash mode.
- Make sure you selected the right port

### Step 5.10: Verify the demo works

If everything worked, the screen should now show the LVGL demo — animated graphics, buttons, etc. Touch should respond.

**Don't proceed past this step until the demo works.** If you can't get the demo working, you won't be able to get Link Vault working — and the demo is supported by Waveshare, so check their wiki/forums for help if needed.

---

## 6. Installing the firmware libraries

Link Vault depends on several libraries. Some are installed via Arduino's Library Manager (click & install), others have to be downloaded from GitHub manually.

### Step 6.1: Library Manager installs

1. In Arduino IDE: **Tools → Manage Libraries** (or click the books icon in the sidebar)
2. Search for and install each of these:

| Library | Version | Notes |
|---------|---------|-------|
| **LVGL** | **8.3.x** | NOT 9.x — this project uses the v8 API |
| **ArduinoJson** | 6.x | |
| **ESPAsyncWebServer** | latest by **ESP32Async** | There are multiple forks; pick the ESP32Async one |
| **AsyncTCP** | latest by **ESP32Async** | Required by ESPAsyncWebServer |
| **NimBLE-Arduino** | 1.4.x or later | |

For each library: search → click on it → click Install. If asked to install dependencies, say yes.

### Step 6.2: Manual install — ESP32-BLE-Keyboard

This one isn't in the Library Manager — you have to download it from GitHub:

1. Go to **https://github.com/T-vK/ESP32-BLE-Keyboard**
2. Click the green **Code** button → **Download ZIP**
3. Save the ZIP somewhere you'll find it
4. In Arduino IDE: **Sketch → Include Library → Add .ZIP Library...**
5. Select the ZIP you just downloaded
6. You should see "Library added to your libraries"

---

## 7. Setting up the Link Vault project

Now you'll combine the Waveshare driver code (which works) with the Link Vault code.

### Step 7.1: Make a copy of the Waveshare demo folder

In your Arduino sketches folder (`Documents/Arduino/` etc.), find the Waveshare demo folder you got working in section 5. Right-click → Copy → Paste it as a new folder. Name the copy **`LinkVault`**.

You're keeping the original demo intact in case you ever want to start over.

### Step 7.2: Delete the demo's main `.ino` file

Inside the new `LinkVault/` folder, find the main `.ino` file (the one you opened in Arduino IDE). Delete it. **Keep all the other files** — those contain the display driver code you need.

### Step 7.3: Copy the Link Vault firmware files in

Take all the files from this project's `firmware/` directory (everything except the README) and copy them into the `LinkVault/` folder, alongside the Waveshare driver files that are already there.

After this, your `LinkVault/` folder should contain:
- The Link Vault `.ino` and `.cpp` and `.h` files (around 23 of them)
- The Waveshare display/touch driver files (around 5-10 of them, names like `AXS15231B.cpp`, `Touch_*.cpp`, `lv_conf.h`)

### Step 7.4: Open the new project

1. Close any open Arduino IDE windows
2. Open the `LinkVault.ino` file from the new folder
3. Arduino IDE will show all the files in the folder as tabs at the top

### Step 7.5: Hook up the Waveshare driver inits

This is the one step you have to do manually because it depends on what version of the Waveshare demo you downloaded.

1. In Arduino IDE, switch to the original Waveshare demo's `.ino` file (not Link Vault — open the original you set aside in 7.1)
2. Find its `setup()` function
3. Look for the lines that initialize the display, touch, and LVGL. They'll look something like:
   ```cpp
   axs15231_init();
   touch_init();
   lvgl_init();
   ```
   Names vary. Look for any function calls in `setup()` that mention display, touch, or LVGL.
4. Copy those lines

Now switch to Link Vault's `LinkVault.ino`:

5. Find the comment block that says:
   ```cpp
   // *** WAVESHARE DISPLAY/TOUCH/LVGL INITS GO HERE ***
   ```
6. Paste the lines you copied right above that comment (or right below — anywhere in that section)

### Step 7.6: Hook up the battery read

In `ui_list.cpp`, find the function `batteryRead()` near the bottom. It looks like:
```cpp
int batteryRead() {
  return 87;  // TODO
}
```

The original Waveshare demo has a battery example. Find their battery-read function (usually called something like `read_battery_voltage()` or `get_battery_percent()`). Replace the body of `batteryRead()` with their code, returning a number from 0 to 100.

If you can't find their battery example, leave the stub as-is. The device will always show 87% battery, but everything else works fine. You can fix this later.

---

## 8. Configuring the build settings

The Tools menu settings should still be the same as Step 5.6 (since you copied that project folder). Double-check:

| Setting | Value |
|---------|-------|
| Board | ESP32S3 Dev Module |
| USB CDC On Boot | Enabled |
| USB Mode | Hardware CDC and JTAG |
| PSRAM | OPI PSRAM |
| Flash Size | 16MB (128Mb) |
| Partition Scheme | 16M Flash (3MB APP/9.9MB FATFS) |
| Upload Speed | 921600 |

---

## 9. Setting your default PINs and password

Before you flash, you can set the starting values for your PINs and master password.

In Arduino IDE, click the **`config.h`** tab. Find these lines near the top:

```cpp
#define DEFAULT_PIN_PERSONAL   "1234"
#define DEFAULT_PIN_WORK       "2580"
#define DEFAULT_PIN_RESEARCH   "1111"
#define DEFAULT_PIN_DECOY      "7851"
#define MASTER_PASSWORD        "Esoteric97!"
```

Change any of these to whatever you want. Constraints:
- All PINs must be exactly **4 digits, all numbers**
- All PINs must be **different from each other** (no two vaults can share a PIN)
- The master password can be anything, any length — but you'll have to type it on a touch screen later, so don't make it 50 characters

**Important caveat:** these defaults are only used on the device's **very first boot**. Once you've flashed and powered it on once, the device saves these values to its flash storage and the defaults are ignored. If you change `config.h` later and re-flash, the device keeps using whatever was already saved. To reset, you'd need to do a factory reset (Section 19).

---

## 10. Compiling and flashing

You're ready to put the code on the device.

### Step 10.1: Plug in the device

USB-C cable from device to computer. The screen may flicker.

### Step 10.2: Select the port

**Tools → Port → (whatever your device shows up as)**

### Step 10.3: Compile

Click the **checkmark** icon (top-left). First compile of Link Vault takes 5-10 minutes. Watch the bottom panel for progress.

**If it fails**: read the error message. The most common errors:
- **"Library X.h: No such file or directory"** → you missed installing a library in Section 6. Go install it.
- **"Sketch too big"** → wrong Partition Scheme. Set it to 16M Flash (3MB APP/9.9MB FATFS).
- **Display init function name mismatch** → the function names you copied in Step 7.5 don't match. Check the original Waveshare demo for the exact names.

### Step 10.4: Upload

Click the **right-arrow** icon (compile + upload). It'll re-compile (faster), then send the binary to the device. Upload takes ~30 seconds.

If upload fails:
- Hold **BOOT** button on the device while clicking upload, release after "Connecting..." appears
- Try a different USB port or cable

### Step 10.5: Watch the device

Once upload finishes, the device automatically reboots. You should see:

1. A few seconds of black screen
2. The boot animation: "// LINK VAULT" title appears, glitches briefly, then a terminal log of `[OK]` messages prints out
3. Final state: **lock screen** with PIN keypad on the right and "// LINK VAULT :: LOCKED" header

If you see this — congratulations, the build worked. Most things are downhill from here.

If the screen stays black or shows garbage:
- Most common cause: Step 7.5 (Waveshare driver inits) wasn't done correctly
- Check the Serial Monitor (Tools → Serial Monitor, baud rate 115200) for error messages

---

## 11. First-run walkthrough

The device is now flashed with default content: 3 real vaults (`personal`, `work`, `research`), 1 decoy vault (`bookmarks`), and the PINs/password from `config.h`.

You haven't paired any phone or laptop yet, so the proximity gate is **disabled** until you pair something. This means everything works without your phone for the moment.

### Step 11.1: Unlock the personal vault

1. On the keypad, tap **1** **2** **3** **4** (or whatever you set as `DEFAULT_PIN_PERSONAL`)
2. Tap **OK**
3. The display should briefly show "GRANTED" in green, then transition to the list screen

You should see a tab strip at the top (`all` / `daily` / `reading`) and an empty list below it. The personal vault has no seeded links — it's a real vault, so you fill it yourself.

### Step 11.2: Add a test link

1. Tap the **CFG** button in the top-right
2. Tap **add new link**
3. The list screen will reappear briefly, then an editor overlay opens with a blank link
4. Type a name on the QWERTY keyboard (e.g., "Hacker News")
5. Tap one of the category pills (or "none")
6. Tap **SAVE**

You'll be back on the list. The new link is there. Note that the URL is "https://" by default — to change the URL, you'd need to use the WiFi config page (Section 15) since the on-device editor doesn't currently support URL editing. **This is intentional** — typing URLs on a touch screen is painful, and the WiFi config page is much faster.

For a more practical workflow, see Section 15 ("Backup and restore") to learn how to add many links at once via the web interface.

### Step 11.3: Pair your phone (or laptop)

You can do this any time. Until you do, the device works without proximity checks. Once you pair, the proximity gate activates.

1. Unlock any vault
2. Tap **CFG**
3. **Long-press** the **BACK** button (top-right) for about 1 second
4. The hidden admin section appears below the regular config items
5. Tap **pair new phone**
6. Master password prompt appears — type your master password (default `Esoteric97!`) and tap OK
7. The device shows **"PAIRING MODE"** with a 6-digit code

Now on your phone:

8. Open **Settings → Bluetooth** (Samsung: **Connections → Bluetooth**)
9. Make sure Bluetooth is on and your phone is scanning
10. Look for **"Link Vault"** in the device list
11. Tap it
12. Phone shows a 6-digit code — confirm it matches the device's code
13. Tap **Pair** (or "Confirm" / "Yes") on both
14. The device's pairing screen will close automatically once bonded

You can now tap **DONE** on the device's pair screen if it didn't auto-close.

### Step 11.4: Test the BLE typing

1. On your phone, open Chrome
2. **Long-press the Chrome icon** → tap **New incognito tab** (this is the workflow that keeps the URL out of your history)
3. Tap the address bar so the cursor is blinking
4. On the device, tap any link in your vault
5. The screen flashes "TRANSMITTING" and the URL types itself into Chrome
6. Press Enter (or wait for auto-Enter if enabled — see Section 12)

If nothing happens:
- Check the device's top-right corner. The dot next to the battery percentage should be **green** (BLE connected). If it's red, your phone isn't actively connected.
- Make sure the phone's Chrome address bar actually has cursor focus before you tap the link.

### Step 11.5: Add the rest of your real links

The most efficient way to add many links is via the WiFi config page (Section 15). For just a few, use the on-device editor.

### Step 11.6: Seed the decoy

The decoy vault (PIN `7851` by default) comes pre-seeded with 5 generic-looking links. **Add 10-15 more of your own** so it looks lived-in. Use links that fit your persona — recipe sites, sports scores, news outlets, whatever you might plausibly bookmark casually.

This step matters: an attacker who's seen your real bookmarking habits will spot a sparse, generic decoy as obviously fake. A rich, varied decoy with personal-looking content holds up.

### Step 11.7: Back up

After you've put real data in:

1. Tap CFG (in any vault)
2. Tap **start wifi config**
3. The device's screen shows SSID, password, URL
4. On your phone or laptop, connect to the WiFi network **`LinkVault-Setup`** (password from the device's screen, default `vaultopen`)
5. Open `http://192.168.4.1` in your browser
6. Click **DOWNLOAD BACKUP**
7. Save the `vaults.json` file somewhere safe

**Do this every time you make significant changes.** That file is the only way to recover your data if the device dies.

---

## 12. Using the device every day

### Unlocking

1. Pick up the device (or wake it from sleep)
2. If your trusted phone/laptop is in BT range, you'll see the lock screen immediately
3. If not, you'll see "AWAITING TRUSTED DEVICE" — make sure your phone is nearby with BT on
4. Enter your PIN, tap OK

### Sending a link

1. Make sure target device (phone Chrome incognito tab, laptop browser, etc.) has cursor focus on a text field
2. Tap a link on the device
3. URL types itself

### Switching categories

Tap any tab in the strip across the top. Tap **all** to see everything in the vault.

### Adding a link

CFG → add new link → fill in name, pick category, SAVE. Edit the URL via the WiFi config page after.

### Renaming a link or category

Long-press the row (for a link) or the tab (for a category). Edit overlay opens.

### Deleting a link or category

Long-press → DELETE → confirm.

### Searching

Tap the **SRCH** button in the top bar. Type. Results appear as you type. Tap one to transmit.

### Locking immediately

Tap **LOCK** in the top bar. Or just leave it idle for 45 seconds and it auto-locks.

### Switching vaults

Tap LOCK, then enter a different vault's PIN.

### Auto-Enter behavior

Each vault has an "auto-press enter after typing" setting. When ON, the device hits Enter after typing the URL — useful for browser address bars. When OFF, you press Enter manually — useful when you want to inspect or edit the URL before submitting.

Toggle it: CFG → **auto-press enter**.

---

## 13. Using with a laptop or desktop

The device pairs with anything that supports Bluetooth keyboards. Here's how:

### Pairing

The device-side flow is the same: CFG → long-press BACK → admin → pair new phone (the label is misleading — it works for laptops too).

On the laptop:

- **macOS**: System Settings → Bluetooth → "Link Vault" → Connect → confirm 6-digit code
- **Windows 10/11**: Settings → Bluetooth & devices → Add device → Bluetooth → "Link Vault" → match code
- **Linux (GUI)**: your distribution's Bluetooth manager → scan → "Link Vault" → confirm code
- **Linux (terminal)**: `bluetoothctl` → `scan on` → `pair <MAC>` → `trust <MAC>` → `connect <MAC>`
- **Chromebook**: Settings → Bluetooth → "Link Vault" → confirm

### Multiple paired devices

The proximity gate only needs **one** bonded device in range to satisfy. Pair both phone and laptop, and either being nearby unlocks the device.

### Single active connection

Bluetooth HID can only actively send keystrokes to one device at a time. If you have your phone and laptop both paired and connected:

- Your keystrokes will go to whichever was most recently active
- To switch, manually disconnect (not "forget") the other from its Bluetooth settings

Some macOS versions handle this transition automatically. Windows usually doesn't.

### Desktop Chrome incognito flow

On desktop, the incognito flow is faster than mobile:
- **Cmd+Shift+N** (Mac) or **Ctrl+Shift+N** (Windows/Linux) opens incognito instantly
- Address bar is already focused — no extra clicks
- Tap link on device → URL types → Enter

### Beyond browsers

Since the device is just a keyboard, it types anywhere a keyboard works:
- Login fields
- Terminal `ssh user@host` style commands
- Search boxes in apps
- Documents and notes apps

Useful trick: keep one vault with auto-Enter ON for browser links, another with auto-Enter OFF for things you need to edit before submitting.

---

## 14. Hidden gestures

The device has several intentional UI secrets — gestures that aren't visible anywhere on screen. This is the entire list:

| Where | Gesture | What happens |
|-------|---------|--------------|
| Lock screen | Long-press the "// LINK VAULT :: LOCKED" header | Master password prompt → silent name-based PIN recovery |
| "Awaiting trusted device" screen | Tap the OVERRIDE button | Master password prompt → bypass proximity (only if override enabled in admin) |
| Config screen | Long-press the "// CONFIG :: name" title | If decoy vault: flashes [DECOY] badge for 5 sec. If real vault: nothing happens (no feedback). |
| Config screen | Long-press the BACK button | Reveals hidden admin section |
| List screen | Long-press any row | Edit overlay (rename, change category, delete, transmit) |
| List screen | Tap any colored category pill on a row | Filter list to that category |
| List screen | Long-press any tab in the strip | Edit/delete that category |
| List screen | Tap the **+** tab on the right | Create new category |

**Why all the hiding?** Two reasons:

1. **Deniability.** A coercer who forces you to unlock the decoy vault should never see hints that other vaults exist. Hidden gestures stay hidden unless you know about them.
2. **Cleanliness.** Visible buttons for every feature would clutter a 640×172 screen. Long-press lets common actions stay simple while advanced ones are accessible.

You don't need to memorize every gesture immediately. The four most useful ones for daily use:

- **Long-press a row** to edit it
- **Long-press a tab** to rename a category
- **+ tab** to create a category
- **Long-press BACK in config** when you need an admin action

The others are situational (recovery, decoy reveal) and you'll only use them rarely.

---

## 15. Backup and restore

### Why this matters

The device's storage is reasonably reliable, but flash memory can fail. A backup file is your only recovery if it does.

### Making a backup

1. Unlock any vault
2. Tap CFG
3. Tap **start wifi config**
4. Note the SSID, password, URL shown on screen
5. On your phone or laptop:
   - Connect to WiFi network **LinkVault-Setup** (password `vaultopen` by default)
   - Open `http://192.168.4.1` in your browser
6. Click **DOWNLOAD BACKUP**
7. Save the `vaults.json` file somewhere safe

The backup includes:
- All vaults (real and decoy), with all PINs, links, and categories
- Settings (auto-enter, sleep timeout, etc.)

The backup does **not** include:
- Your master password
- Bonded BLE device list
- Security state (failed attempts, lockout)

### Adding many links at once via the web page

Same WiFi config page, same setup as backup:
- Use the form fields on the page to add links one at a time, but with a real keyboard
- Or download the backup, edit the JSON in a text editor on your computer (it's human-readable), then re-upload via the **RESTORE** form

The JSON format is straightforward:
```json
{
  "vaults": [
    {
      "name": "personal",
      "pin": "1234",
      "isDecoy": false,
      "categories": [{"name": "daily", "color": 16756736}],
      "links": [
        {"name": "Hacker News", "url": "https://news.ycombinator.com", "cat": "daily"}
      ]
    }
  ]
}
```

Editing this directly is the fastest way to seed a vault with many links. Just re-upload the modified file via RESTORE.

### Restoring from a backup

1. Open the WiFi config page (same as above)
2. Use the **RESTORE FROM FILE** form
3. Select your backup file
4. Click **UPLOAD AND RESTORE**
5. The device automatically reboots with the restored data

### Changing the master password

Same WiFi config page. Fill in the **CHANGE MASTER PASSWORD** field, click UPDATE. Done.

---

## 16. Security model

This section is honest about what the device does and doesn't protect against.

### What it protects against

- **Casual snoops**: someone picking up your device and trying random PINs gives up after the lockouts kick in
- **Attackers without your phone**: BLE bonding means random Bluetooth devices can't connect; the proximity gate means the touch screen refuses input without your trusted device nearby
- **Coerced unlocks**: the panic PIN opens a believable decoy vault; real vaults stay invisible
- **Brute-force PIN attempts**: exponential backoff makes 4-digit brute force take weeks instead of minutes
- **Watchers during recovery**: silent name-based recovery means a watcher sees you fail at recovery without learning anything about which vaults exist

### What it does NOT protect against

- **A determined attacker with the device, a soldering iron, and JTAG tools**: they can dump the flash directly and read your data in plaintext. The PIN is "lock the UI" not "encrypt the data."
- **An attacker who has your phone unlocked AND the device**: proximity gate is satisfied
- **Side-channel attacks**: power analysis, timing attacks, etc. — out of scope.
- **A compromised computer doing the flashing**: if your computer was malicious, it could've put a backdoor in. (This is paranoid territory but worth naming.)

If you need protection against the soldering-iron attacker, you'd want to add AES encryption keyed from the PIN. That's a significant codebase addition (~200 lines) and outside the current scope. Realistic threat models for personal devices don't usually require it.

### Tuning the security

You can adjust how aggressive the device is in `config.h`:

```cpp
// How long lockouts last (seconds)
static const uint32_t BACKOFF_TABLE[] = {
  0, 0, 0, 30, 60, 300, 900, 1800, 3600, 21600, 86400,
  // 0 1 2  3   4   5    6    7     8     9      10+
};
// Index = number of wrong attempts. 10+ adds 24 hours per additional.

// How close the phone has to be (more negative = farther allowed)
#define PROXIMITY_RSSI_MIN     -75    // -85 ≈ 10m, -65 ≈ arm's reach

// Auto-wipe trigger (0 = disabled)
#define DEFAULT_WIPE_THRESHOLD 0    // Set to e.g. 25 for "wipe real vaults after 25 wrong PINs"
```

Re-flash after editing — but remember, defaults only apply on first boot. To re-apply changes, factory reset first (Section 19).

---

## 17. Troubleshooting

### The screen stays black after upload

- Most likely cause: Step 7.5 (Waveshare driver inits) wasn't done. Re-check that you copied the `axs15231_init()` / `touch_init()` / `lvgl_init()` lines from the demo.
- Open Serial Monitor (Tools → Serial Monitor, 115200 baud) — error messages there usually tell you what's missing.

### The device boots but won't accept touches

- Touch driver init may not have run. Check Step 7.5.
- Try the original Waveshare demo again — if touch works there but not in Link Vault, it's a config issue.

### Pairing fails

- Your phone needs to be in active scan mode when the device is in pair mode
- Some Samsungs need you to keep the Bluetooth settings page open while pairing
- If the codes don't match, cancel and start over — sometimes the connection has stale state
- Try restarting Bluetooth on your phone (toggle off/on)

### "BLE OFFLINE" toast when tapping a link

- Phone isn't currently connected. Check phone's Bluetooth settings — "Link Vault" should show as Connected.
- If it shows as Paired but not Connected, tap it to reconnect.
- The green dot in the device's top bar should be green. If red, that confirms BLE isn't connected.

### Proximity gate keeps refusing

- Phone's Bluetooth must be on, advertising, and in range
- Your phone may not be advertising consistently — keep the BT settings page open while testing
- Lower `PROXIMITY_RSSI_MIN` in `config.h` (more negative number) if your phone's signal is weak

### Wifi config mode doesn't appear

- Wait a few seconds — the AP takes 2-3 seconds to come up
- Check that you tapped "start wifi config" (it's a toggle — tap again to confirm it shows "RUNNING")
- ESP32 shares one radio between BLE and WiFi. Sometimes the handoff gets confused — reboot the device.

### Boot loop (device reboots forever)

- Most common cause: wrong Partition Scheme. Set to "16M Flash (3MB APP/9.9MB FATFS)".
- Or: PSRAM not enabled. Set PSRAM to "OPI PSRAM".
- Or: not enough flash for the project. Make sure Flash Size is "16MB".

### URL types but with wrong characters

- Some Bluetooth keyboard layouts default to US English. If your phone is set to a different layout, characters can get mangled.
- Fix: on your phone, add a "US English (Hardware)" Bluetooth keyboard layout in Settings → Languages → Physical Keyboard.

### I forgot a vault PIN

Use the recovery flow:
1. Long-press the "// LINK VAULT :: LOCKED" header
2. Enter master password
3. Type the vault's name (lowercase, exact match)
4. Set a new 4-digit PIN

The decoy vault is **not** recoverable through this flow — by design.

### I forgot the master password

There's no remote recovery — that would be a security backdoor. Your options:

1. **You can still unlock individual vaults with their PINs**, you just can't access admin actions
2. **You can still back up** via the WiFi config page (it doesn't require master password)
3. **If you're truly locked out and need to reset**: Section 19 (factory reset)

Factory reset wipes everything, so do it only if you've backed up first.

---

## 18. Glossary of terms

| Term | What it means |
|------|---|
| **Arduino IDE** | The program you use to write and upload code to microcontrollers |
| **AXS15231B** | The specific display chip on this Waveshare board. Needs Waveshare's driver code. |
| **BLE** | Bluetooth Low Energy. Modern flavor of Bluetooth used for keyboards, etc. |
| **Bonding (BLE)** | A persistent secure pairing — devices remember each other across reboots |
| **Brute force** | An attack that tries every possible PIN until one works |
| **Compile** | Translate source code into machine code the chip can run |
| **Decoy** | A fake vault that opens with a special PIN, designed to look like the real thing |
| **ESP32** | A family of small WiFi+Bluetooth microcontrollers |
| **Flash storage** | The memory inside the chip where code and saved data live |
| **Flashing** | Uploading code to the device |
| **HID** | Human Interface Device — the standard for keyboards, mice, etc. |
| **JSON** | A text format for structured data. Backups use this. |
| **Library** | Pre-written code you include in your project to do common tasks |
| **LittleFS** | A filesystem used to store data on the chip |
| **LVGL** | Light and Versatile Graphics Library — used to draw the UI |
| **NimBLE** | A Bluetooth library for ESP32 |
| **Partition Scheme** | How the chip's storage is divided. Affects how much room you have for your code and data. |
| **PSRAM** | Pseudo-static RAM. Extra memory the ESP32-S3 has. Required for graphics. |
| **PWM** | Not relevant here, just a common acronym you might see |
| **RSSI** | Received Signal Strength Indicator. How close a Bluetooth device is. More negative number = farther. |
| **Sketch** | Arduino's term for a project |
| **Vault** | One of the 4 separate lists of links the device holds |

---

## 19. If something goes really wrong

### Full factory reset

If you need to wipe everything and start from scratch:

**Method 1: Through the device** (if you can get to admin):
1. Unlock any vault
2. CFG → long-press BACK → admin → factory reset → confirm
3. Device wipes and reboots

**Method 2: Erase via Arduino IDE** (if device is unresponsive or you forgot master password):
1. Plug in the device
2. In Arduino IDE: **Tools → Erase All Flash Before Sketch Upload → Enabled**
3. Re-flash the firmware (compile + upload)
4. After upload completes, set **Erase All Flash Before Sketch Upload → Disabled** (otherwise every future upload wipes data)

This nukes all vaults, settings, master password, and bonded devices. The device returns to first-boot state with the defaults from `config.h`.

### Restoring from a backup after a reset

After a factory reset:
1. Boot the device
2. Unlock with the default PIN (`1234` for personal vault, or whatever you set in `config.h`)
3. CFG → start wifi config
4. On phone/laptop, connect to the device's WiFi, browse to `http://192.168.4.1`
5. Use **RESTORE FROM FILE** to upload your backup `vaults.json`
6. Device reboots with all your data back

The master password and bonded devices won't restore (they're not in the backup). You'll need to re-enter your master password and re-pair your phone.

### Asking for help

If you're stuck:
1. Read the error message carefully. Most Arduino errors say exactly what's wrong.
2. Search the exact error message — others have likely hit it
3. Check Waveshare's wiki for hardware-specific issues
4. Use the Arduino Serial Monitor (115200 baud) to see what the device is logging during boot

Common places to ask for help:
- Arduino forum (forum.arduino.cc)
- Reddit's r/esp32
- Waveshare's own forum/wiki

---

## What's next

Once it's all working, ideas for improvements:
- 3D-print a case for it
- Add a wrist strap or keyring loop
- Add more vaults via admin (up to 4 total)
- Build elaborate decoy vaults with weeks of fake "history"
- Tweak the visual theme (`theme.h`) to your taste
- Adjust backoff aggression (`config.h`) for your threat model

Have fun with it. This is a genuinely useful little tool once you've got it working.
