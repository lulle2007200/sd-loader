**Picofly initial payload**

**Features:**
- Works even with defective RAM
- Supports Booting from a FAT32 partition on SD card, EMMC, BOOT1, or BOOT1 at 1MB offset
- Integrated UMS tool for accessing storage via USB
- Can launch payloads up to size of 128KB
- Integrated toolbox to update modchip firmware, update sdloader, rollback firmware
- Persistent configuration of the boot action (boot to menu by default, or boot payload by default), the boot storage (SD, EMMC, BOOT1 or automatic), the button combination to boot Stock (enable or disable)

**How to use:**
1) Put sdloader.enc on root of SD card
2) Boot sdloader.bin (e.g. by loading it via hekate) while holding VOL+ button
3) Using Vol+/-, In the menu go to More -> Toolbox -> Update IPL, hit POWER to confirm
4) Done.

Hold VOL+ while booting to open the sdloader menu, hold VOL+ and VOL- while booting to boot Stock.
The button combination to boot stock can be disabled in the menu.
The payload is loaded from payload.bin in the root directory of the FAT32 partition on the selected boot storage.



NOTE: To support loading payloads bigger than 64kB, a part of the framebuffer is (ab)used to sotre the payload.
When loading large payloads, the display might appear corrupted for a moment.
