#ifndef _FILES_H
#define _FILES_H

#include <libs/fatfs/ff.h>
#include <utils/types.h>

typedef enum{
	SDLOADER_DRIVE_SD = 0,
	SDLOADER_DRIVE_BOOT1_1MB,
	SDLOADER_DRIVE_BOOT1,
	SDLOADER_DRIVE_GPP,
	SDLOADER_DRIVE_INVALID,
}sdloader_drive;

extern const char* drive_friendly_names[4];

FRESULT open_file_on(const char *path, FIL *f, u8 drive);
FRESULT open_file_on_any(const char *path, FIL *f, u8 *drive);
FRESULT unmount_drive();


#endif