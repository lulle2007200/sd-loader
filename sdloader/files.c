#include "files.h"
#include <libs/fatfs/ff.h>
#include <utils/btn.h>
#include <utils/types.h>
#include <tui.h>

const char* drive_friendly_names[4] = {
	[SDLOADER_DRIVE_BOOT1]     = "BOOT 1",
	[SDLOADER_DRIVE_BOOT1_1MB] = "BOOT 1",
	[SDLOADER_DRIVE_SD]        = "SD",
	[SDLOADER_DRIVE_GPP]       = "GPP",
};

const char* drive_names[4] = {
	[SDLOADER_DRIVE_SD]        = "0:",
	[SDLOADER_DRIVE_BOOT1_1MB] = "2:",
	[SDLOADER_DRIVE_BOOT1]     = "1:",
	[SDLOADER_DRIVE_GPP]       = "3:",
};

static FATFS fs;
static u8 cur_drive = SDLOADER_DRIVE_INVALID;

FRESULT unmount_drive(){
	if(cur_drive != SDLOADER_DRIVE_INVALID){
		return f_mount(NULL, drive_names[cur_drive], 0);
	}
	return FR_OK;
}

static FRESULT mount_drive(u8 drive){
	if(cur_drive != drive){
		unmount_drive();
		cur_drive = SDLOADER_DRIVE_INVALID;
		FRESULT res = f_mount(&fs, drive_names[drive], 1);
		if(res == FR_OK){
			cur_drive = drive;
		}
		return res;
	}
	return FR_OK;
}

FRESULT open_file_on(const char *path, FIL *f, u8 drive){
	FRESULT res;

	if(drive > 3){
		return FR_INVALID_DRIVE;
	}

	res = mount_drive(drive);
	if(res != FR_OK){
		return res;
	}
	
	res = f_chdrive(drive_names[drive]);
	if(res != FR_OK){
		return res;
	}

	return f_open(f, path, FA_READ | FA_OPEN_EXISTING);
}

FRESULT open_file_on_any(const char *path, FIL *f, u8 *drive){
	FRESULT res;

	*drive = SDLOADER_DRIVE_INVALID;

	for(u32 i = 0; i < 4; i++){
		res = mount_drive(i);

		if(res != FR_OK){
			continue;
		}

		res = open_file_on(path, f, i);

		if(res == FR_NO_FILE){
			continue;
		}

		*drive = i;

		return res;
		break;
	}

	return FR_NO_FILE;
}