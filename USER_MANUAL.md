# LINK VAULT — User Manual

> A PIN-locked, BLE-enabled credential vault for the Waveshare ESP32-S3-Touch-LCD-3.49.
> Stores URLs and login credentials. Types them directly into any connected device.

---

## Table of Contents

1. [Overview](#1-overview)
2. [First Boot & Default Credentials](#2-first-boot--default-credentials)
3. [The Lock Screen](#3-the-lock-screen)
4. [Proximity Unlock](#4-proximity-unlock)
5. [The Main Screen](#5-the-main-screen)
6. [Adding & Editing Links](#6-adding--editing-links)
7. [Typing Credentials via BLE](#7-typing-credentials-via-ble)
8. [Categories](#8-categories)
9. [Search](#9-search)
10. [The Panic-Close Gesture](#10-the-panic-close-gesture)
11. [The Decoy Vault](#11-the-decoy-vault)
12. [Settings & Config](#12-settings--config)
13. [WiFi Backup & Restore](#13-wifi-backup--restore)
14. [Security & Lockout System](#14-security--lockout-system)
15. [Changing PINs & Passwords](#15-changing-pins--passwords)
16. [Complete Gesture Reference](#16-complete-gesture-reference)

---

## 1. Overview

Link Vault is a physical password and URL vault that lives on your wrist or desk. It stores credentials locally on the device — nothing is ever transmitted to a cloud service — and types them into your phone or PC over Bluetooth when you tap a link.

**Key concepts:**

| Concept | What it means |
|---|---|
| **Vault** | A PIN-protected container. Up to 4 vaults on one device. Each is completely separate. |
| **Link** | A stored entry: a URL plus optional username, password, and category. |
| **BLE HID** | The device appears as a Bluetooth keyboard. It types your credentials for you. |
| **Proximity unlock** | The device checks that your bonded phone is nearby before showing the PIN screen. |
| **Decoy vault** | A vault that looks real but contains dummy data. For plausible deniability. |

---

## 2. First Boot & Default Credentials

On first boot the device creates four vaults and seeds them with default PINs. **Change these before storing real data.**

### Default PINs

| Vault | Default PIN | Notes |
|---|---|---|
| `personal` | `1234` | General personal use |
| `work` | `2580` | Work credentials |
| `research` | `1111` | Research / reading |
| `bookmarks` | `7851` | This is the **decoy vault** — see Section 11 |

### Default Master Password

```
Esoteric97!
```

The master password is used for vault recovery and admin access. Change it immediately via the WiFi config page (Section 13).

### Default WiFi AP (for backup/restore)

| Setting | Value |
|---|---|
| Network name | `LinkVault-Setup` |
| Password | `vaultopen` |
| Web interface | `192.168.4.1` |

---

## 3. The Lock Screen

Every boot and every auto-lock returns you here.

```
  // LINK VAULT

  > enter pin

  -  -  -  -        [1] [2] [3]
                    [4] [5] [6]
                    [7] [8] [9]
                    [DEL][0][OK]
```

**How to unlock:**
1. Tap the digits of your vault's PIN.
2. Tap **OK**.
3. If correct, the vault opens immediately.

### Wrong PIN behaviour

The device does not tell you which vault a PIN belongs to — it tries all vaults silently.

| Wrong attempts | Lockout |
|---|---|
| 1 – 2 | No lockout |
| 3 | 30 seconds |
| 4 | 1 minute |
| 5 | 5 minutes |
| 6 | 15 minutes |
| 7 | 30 minutes |
| 8 | 1 hour |
| 9 | 6 hours |
| 10+ | 24 hours per additional attempt |

Lockout times persist across reboots. Powering off does not reset the counter.

### Forgot your PIN? → Recovery

1. **Long-press the "// LINK VAULT" header** on the lock screen.
2. Enter the **master password** on the QWERTY screen that appears.
3. Type your **vault name** (e.g. `personal`) and tap OK.
4. Set a new PIN — you will be asked to enter it **twice** to confirm.

---

## 4. Proximity Unlock

Before the PIN screen appears, the device scans for your bonded phone over BLE. If your phone is not detected nearby, the device waits on the "awaiting trusted device" screen.

**This prevents someone from PIN-guessing if they steal the device without also having your phone.**

### Pairing your phone

1. Open **Settings → Config** on the device.
2. Tap **PAIR PHONE**.
3. Make sure Bluetooth is on and your phone is within arm's reach.
4. The device saves your phone's BLE address and will recognise it on future boots.

### Bypassing proximity

If your phone is unavailable (dead battery, etc.):

1. On the "awaiting trusted device" screen, tap **MASTER PW**.
2. Enter the master password to skip the proximity check.

> This bypass can be disabled entirely in the admin settings if you want stricter security.

---

## 5. The Main Screen

Once unlocked, you land on the main list screen.

```
 ___________________________________________________________
| // VAULT :: PERSONAL    [tamper] •  72%  [+][SRCH][CFG][LOCK] |
|___________________________________________________________|
| all | daily | reading | tools |  +                        |
|-----------------------------------------------------------|
| [01] [daily]  GitHub             >>                       |
| [02] [daily]  Gmail              >>                       |
| [03] [reading] Hacker News       >>                       |
|___________________________________________________________|
```

### Topbar elements

| Element | Location | Meaning |
|---|---|---|
| `// VAULT :: NAME` | Far left | The active vault's name |
| Tamper indicator | Centre-left | "last: 2h ago \| 3 fails" — shows activity since last unlock |
| Coloured dot | Right side | Green = BLE device connected, Red = no BLE connection |
| Battery % | Right side | Current battery level (turns orange below 20%) |
| `+` | Right buttons | Create a new link |
| `SRCH` | Right buttons | Open search |
| `CFG` | Right buttons | Open settings |
| `LOCK` | Right buttons | Lock the vault immediately |

### Tab strip

The coloured tabs below the topbar are your **categories**. Tap one to filter the list. The `+` tab at the end creates a new category.

### Link rows

Each row shows the link's category pill, name, and a `>>` indicator.

- **Tap a row** → types the URL to your connected device over BLE
- **Long-press a row** (hold ~0.6 s) → opens the edit overlay for that link

---

## 6. Adding & Editing Links

### Adding a new link

Tap the **`+` button** in the topbar. An edit overlay opens with an empty link. Fill in the fields and tap **SAVE** or press **OK** on the keyboard. If you tap **CANCEL** or **X CLOSE**, the empty link is discarded — nothing is saved.

### The edit overlay

```
 _______________________________________________ ________________________
| // EDIT :: GitHub                    X CLOSE ||                        |
|-----------------------------------------------|   Q W E R T Y U I O P |
| URL  [https://github.com         ]            ||   A S D F G H J K L   |
| NAME [GitHub                     ]            || SHIFT Z X C V B N M < |
| USER [myusername                  ]            ||  SYM  [___________] OK |
| PASS [**********                  ]            ||                        |
|                                               ||                        |
| CATEGORY                                      ||                        |
| [daily] [reading] [none]                      ||________________________|
|_______________________________________________|
| CANCEL | DELETE | TRANSMIT |  LOGIN  |  SAVE  |
```

**Tapping a field** (URL, NAME, USER, PASS) focuses it — the border turns amber and the keyboard routes to that field. The password field always displays as asterisks.

**Keyboard keys:**
| Key | Action |
|---|---|
| `SHIFT` | Toggle uppercase |
| `SYM` | Switch to symbols/numbers |
| `ABC` | Return to letters |
| `<` (BACK) | Delete last character |
| `___` (space bar) | Insert a space |
| `OK` | Save and close |

**Bottom buttons:**
| Button | Action |
|---|---|
| `CANCEL` | Discard changes and close |
| `DELETE` | Delete this link permanently (asks for confirmation) |
| `TRANSMIT` | Type the URL to the connected device, then close |
| `LOGIN` | Type username → Tab → password → Enter to the connected device |
| `SAVE` | Save all changes and close |

---

## 7. Typing Credentials via BLE

The device works as a Bluetooth HID keyboard. When you transmit, it types directly into whatever text field is active on the connected device — as if you typed it yourself.

### Transmit URL

**Tap any link row** from the main list, or tap **TRANSMIT** inside the edit overlay.

The screen flashes `>> TRANSMITTING <<` and the URL is typed. If **Auto-Enter** is on in settings, the device also presses Enter automatically.

### Transmit login credentials

Open the edit overlay (long-press the link row) and tap **LOGIN**.

The device types:
```
[username]  Tab  [password]  Enter*
```
*Enter is only sent if Auto-Enter is enabled in settings.

> **Tip:** Focus the username field on the login page before tapping LOGIN so the cursor is in the right place.

### BLE connection requirements

- The target device must have Bluetooth on and must have previously paired with Link Vault (the device shows as **"Link Vault"** in Bluetooth settings).
- The red dot in the topbar means no device is connected. The transmission will be blocked with an `!! BLE OFFLINE !!` warning.

---

## 8. Categories

Categories are coloured labels you assign to links to keep them organised. Each vault has its own independent set of categories.

### Creating a category

Tap the **`+` tab** at the right end of the tab strip. Type a name and tap OK or SAVE. A colour is assigned automatically.

### Editing or deleting a category

**Long-press a category tab** (hold ~0.6 s) to open the edit screen. You can rename it or delete it. If you delete a category, the links that used it become uncategorised — they are not deleted.

### Filtering by category

Tap any category tab to show only links in that category. Tap **all** to see everything. Tapping a category pill on a link row also switches the filter.

---

## 9. Search

Tap **SRCH** in the topbar to open the full-screen search overlay.

- Searches both **name** and **URL** simultaneously
- Case-insensitive
- Results update as you type
- Tap a result to transmit its URL immediately

---

## 10. The Panic-Close Gesture

**Double-tap the vault title text** in the top-left of the screen (the `// VAULT :: NAME` area) to instantly close the incognito/private tab on your connected device.

```
 _______________________________________________
| // VAULT :: PERSONAL  ← double-tap here      |
```

The device sends `Ctrl+W` via BLE keyboard, which:
- **Windows** (Chrome / Edge): closes the current tab. With one tab open it closes the window.
- **Android** (Chrome): closes the current incognito tab.

A green `// INCOGNITO CLOSED` toast confirms the action. If Bluetooth is not connected, an `!! BLE OFFLINE !!` warning appears instead.

**Notes:**
- Only works when the vault is unlocked (not on the lock screen)
- Two taps must occur within 350 ms of each other
- The title zone has no buttons, so accidental triggers during normal use are not possible

---

## 11. The Decoy Vault

One vault (named `bookmarks` by default, PIN `7851`) is designated as a **decoy**. It contains believable but harmless links (Wikipedia, YouTube, etc.) and behaves identically to a real vault.

**Purpose:** If you are compelled to unlock the device, you can hand over the decoy PIN. The person sees a plausible-looking list of bookmarks. Your real vaults are never revealed.

**How to identify the decoy (admin only):**
In the config screen's hidden admin section, decoy vaults are labelled with a `[DECOY]` badge when you long-press the tamper indicator area.

**Important:** Unlocking the decoy vault resets the failed-attempts counter — it counts as a successful unlock. Use this to your advantage.

> Change the decoy PIN to something you can plausibly claim is your "main" PIN.

---

## 12. Settings & Config

Tap **CFG** in the topbar to open the settings screen.

### Standard settings

| Setting | Options | What it does |
|---|---|---|
| **Auto-lock** | 1 / 5 / 15 / 30 min | Locks the vault after this much idle time |
| **Auto-Enter** | On / Off | Automatically presses Enter after typing a URL or password |
| **Sleep timer** | 1 / 5 / 15 / 30 min | Same as Auto-lock (tap to cycle through values) |

### Pairing

| Action | What it does |
|---|---|
| **PAIR PHONE** | Starts a BLE pairing scan so your phone is recognised for proximity unlock |
| **CLEAR BONDS** | Removes all paired phones (you will need to re-pair) |

### Hidden admin section

**Long-press the tamper report text** (the small dim text in the topbar, e.g. "last: 2h ago") to reveal the admin section. You will be asked for the master password.

Admin options include:

| Option | What it does |
|---|---|
| **WiFi Backup** | Starts the WiFi AP for backup/restore (see Section 13) |
| **Wipe threshold** | Set how many wrong PIN attempts trigger an automatic vault wipe (0 = disabled) |
| **Proximity override** | Allow/disallow master password to bypass the phone proximity check |
| **Change master password** | Change the master password (requires current password) |
| **Factory reset** | Wipes everything and re-seeds defaults |

---

## 13. WiFi Backup & Restore

The device can host a small web interface over WiFi for backing up or restoring your vault data.

> **BLE is paused while WiFi is active.** It restarts automatically when you exit config mode.

### Starting the WiFi interface

1. Tap **CFG** → enter the admin section (long-press tamper text → master password).
2. Tap **WiFi Backup**.
3. The device broadcasts:
   - **Network:** `LinkVault-Setup`
   - **Password:** `vaultopen`
4. On your phone or PC, connect to that network.
5. Open a browser and go to **`192.168.4.1`**.

### What you can do

| Action | Endpoint / button |
|---|---|
| Download vault backup | `/vaults.json` — save this file somewhere safe |
| Restore from backup | Upload a previously downloaded `.json` file |
| Change master password | Enter current password + new password |

### Stopping WiFi

Tap **STOP WIFI** on the device, or navigate away from the config screen.

---

## 14. Security & Lockout System

### Brute-force protection

Wrong PIN attempts trigger progressively longer lockouts (see the table in Section 3). The counter and lockout end-time are stored in flash memory — they survive reboots.

Entering any **correct** PIN (including the decoy) resets the counter to zero.

### Auto-wipe

If you enable a **wipe threshold** in the admin settings, the real vaults are automatically deleted after that many wrong attempts. The decoy vault always survives a wipe. This is disabled by default.

### Tamper indicator

After unlocking, the small dim text in the topbar shows:

```
last: 2h ago | 3 fails
```

This tells you:
- How long ago the vault was last opened
- How many failed PIN attempts occurred since then

If the device rebooted since the last unlock, it shows `last: since last boot`. Use this to notice if someone has been attempting to break in while you were away.

---

## 15. Changing PINs & Passwords

### Change your vault PIN

If you know your current PIN:
1. The PIN entry screen does not have a direct "change PIN" option.
2. Use the **recovery flow** instead: long-press the lock screen header → enter master password → type your vault name → enter and confirm a new PIN.

### Change the master password

1. Connect to the WiFi interface (Section 13).
2. Go to **Change Master Password**.
3. Enter the current master password, then the new one.

Alternatively, in the admin section of the config screen, tap **Change Master PW**.

> The current password is always required to change the master password. There is no way to change it without knowing the existing one.

---

## 16. Complete Gesture Reference

### Touch gestures

| Gesture | Where | Action |
|---|---|---|
| **Tap** | Link row | Transmit URL to connected device |
| **Long-press** (~0.6 s) | Link row | Open link edit overlay |
| **Tap** | Category tab | Filter list by that category |
| **Long-press** (~0.6 s) | Category tab | Open category edit/delete screen |
| **Tap** | `+` tab (tab strip) | Create a new category |
| **Tap** | `+` button (topbar) | Create a new link |
| **Tap** | `SRCH` | Open search overlay |
| **Tap** | `CFG` | Open settings |
| **Tap** | `LOCK` | Lock vault immediately |
| **Double-tap** (~350 ms) | Vault title (top-left) | Send Ctrl+W to close incognito tab |
| **Long-press** (~0.6 s) | Tamper text (topbar) | Enter admin section (requires master password) |
| **Long-press** | Lock screen header | Enter recovery flow |

### BLE keyboard shortcuts sent to connected device

| Action | Keys sent | Effect |
|---|---|---|
| Transmit URL | Types URL text | Fills URL or text field |
| Transmit URL + Auto-Enter | Types URL text + `Enter` | Fills and submits |
| LOGIN | `username` + `Tab` + `password` | Fills login form |
| LOGIN + Auto-Enter | `username` + `Tab` + `password` + `Enter` | Fills and submits login |
| Panic close | `Ctrl+W` | Closes current tab (incognito or otherwise) |

---

## Quick Reference Card

```
 BOOT          Power on → proximity check → PIN screen
 UNLOCK        Enter 4-digit PIN → OK
 TRANSMIT URL  Tap any link row
 LOGIN         Long-press row → EDIT → LOGIN button
 NEW LINK      Tap [+] in topbar
 EDIT LINK     Long-press row → edit fields → SAVE
 SEARCH        Tap [SRCH]
 LOCK NOW      Tap [LOCK]
 PANIC CLOSE   Double-tap vault title (top-left)
 SETTINGS      Tap [CFG]
 ADMIN         Tap [CFG] → long-press tamper text
 BACKUP        CFG → Admin → WiFi Backup → 192.168.4.1
 RECOVERY      Long-press lock screen title → master pw → vault name
```

---

*Link Vault — all data stored locally, never transmitted to any server.*
