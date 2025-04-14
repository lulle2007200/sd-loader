/*-----------------------------------------------------------------------*/
/* Low level disk I/O module SKELETON for FatFs     (C)ChaN, 2019        */
/*-----------------------------------------------------------------------*/
/* If a working storage control module is available, it should be        */
/* attached to the FatFs via a glue function rather than modifying it.   */
/* This is an example of glue functions to attach various exsisting      */
/* storage control modules to the FatFs module with a defined API.       */
/*-----------------------------------------------------------------------*/

#include <libs/fatfs/ff.h>
#include <libs/fatfs/diskio.h>		/* Declarations of disk functions */
#include <storage/emmc.h>
#include <storage/sd.h>
#include <storage/sdmmc.h>

/* Definitions of physical drive number for each drive */



/*-----------------------------------------------------------------------*/
/* Get Drive Status                                                      */
/*-----------------------------------------------------------------------*/

static bool ensure_partition(BYTE pdrv){
	u8 part;
	switch (pdrv) {
	case DEV_BOOT1:
	case DEV_BOOT1_1MB:
		part = EMMC_BOOT1;
		break;
	case DEV_GPP:
		part = EMMC_GPP;
		break;
	case DEV_BOOT0:
		part = EMMC_BOOT0;
		break;
	case DEV_SD:
		return true;
	}

	if(emmc_storage.partition != part){
		return emmc_set_partition(part) != 0;
	}
	return true;
}

DSTATUS disk_status (
	BYTE pdrv		/* Physical drive nmuber to identify the drive */
)
{
	return 0;
}



/*-----------------------------------------------------------------------*/
/* Inidialize a Drive                                                    */
/*-----------------------------------------------------------------------*/

DSTATUS disk_initialize (
	BYTE pdrv				/* Physical drive nmuber to identify the drive */
)
{
	bool res = true;
	switch (pdrv) {
	case DEV_SD:
		res &= sd_initialize(false);
		break;
	case DEV_BOOT1:
	case DEV_BOOT1_1MB:
		res &= emmc_initialize(false);
		res &= emmc_set_partition(EMMC_BOOT1);
		break;
	case DEV_BOOT0:
		res &= emmc_initialize(false);
		res &= emmc_set_partition(EMMC_BOOT0);
		break;
	case DEV_GPP:
		res &= emmc_initialize(false);
		res &= emmc_set_partition(0);
		break;
	}

	return res ? 0 : STA_NOINIT;
}



/*-----------------------------------------------------------------------*/
/* Read Sector(s)                                                        */
/*-----------------------------------------------------------------------*/

DRESULT disk_read (
	BYTE pdrv,		/* Physical drive nmuber to identify the drive */
	BYTE *buff,		/* Data buffer to store read data */
	LBA_t sector,	/* Start sector in LBA */
	UINT count		/* Number of sectors to read */
)
{
	sdmmc_storage_t *storage = &sd_storage;
	u32 actual_sector = sector;

	switch (pdrv) {
	case DEV_BOOT0:
	case DEV_BOOT1:
	case DEV_GPP:
		storage = &emmc_storage;
		break;
	case DEV_BOOT1_1MB:
		storage = &emmc_storage;
		actual_sector = sector + (0x100000 / 512);
		break;
	}

	ensure_partition(pdrv);

	return sdmmc_storage_read(storage, actual_sector, count, buff) ? RES_OK : RES_ERROR;
}

DRESULT disk_write (
	BYTE pdrv,		/* Physical drive nmuber to identify the drive */
	const BYTE *buff,		/* Data buffer to store read data */
	LBA_t sector,	/* Start sector in LBA */
	UINT count		/* Number of sectors to read */
)
{
	sdmmc_storage_t *storage = &sd_storage;
	u32 actual_sector = sector;

	switch (pdrv) {
	case DEV_BOOT0:
	case DEV_BOOT1:
	case DEV_GPP:
		storage = &emmc_storage;
		break;
	case DEV_BOOT1_1MB:
		storage = &emmc_storage;
		actual_sector = sector + (0x100000 / 512);
		break;
	}

	ensure_partition(pdrv);

	// we only ever want to write to boot0, return error if trying to write to anyting else
	if(pdrv == DEV_GPP || pdrv == DEV_BOOT1 || pdrv == DEV_BOOT1_1MB || pdrv == DEV_SD){
		return RES_ERROR;
	}

	return sdmmc_storage_write(storage, actual_sector, count, (u8*)buff) ? RES_OK : RES_ERROR;
}

/*-----------------------------------------------------------------------*/
/* Miscellaneous Functions                                               */
/*-----------------------------------------------------------------------*/

DRESULT disk_ioctl (
	BYTE pdrv,		/* Physical drive nmuber (0..) */
	BYTE cmd,		/* Control code */
	void *buff		/* Buffer to send/receive control data */
)
{
	return RES_OK;
}

