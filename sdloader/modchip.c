#include "modchip.h"
#include "memory_map.h"
#include <libs/fatfs/ff.h>
#include <soc/timer.h>
#include <storage/emmc.h>
#include <storage/sdmmc.h>
#include <storage/mmc.h>
#include <storage/sdmmc_driver.h>
#include <string.h>
#include <libs/fatfs/diskio.h>

#define MODCHIP_MAGIC 0xAA5458BA

static sd_loader_cfg_t default_cfg = {
	.magic1 = MODCHIP_MAGIC,
	.magic2 = MODCHIP_MAGIC,
	.default_payload_vol = MODCHIP_PAYLOAD_VOL_AUTO,
	.default_action = MODCHIP_DEFAULT_ACTION_PAYLOAD,
	.disable_menu_btn_combo = false,
	.disable_ofw_btn_combo = false,
};

static void _init_mmc(){
	u32 start = get_tmr_us();
	sdmmc_init(&emmc_sdmmc, SDMMC_4, SDMMC_POWER_1_8, SDMMC_BUS_WIDTH_1, SDHCI_TIMING_MMC_ID);
	u32 now = get_tmr_us();
	if(now - start < 40000){
		usleep(start + 40000 - now);
	}
}

void modchip_confirm_execution(){
	// modchip waits for IDLE_CMD with magic argument to confirm the payload is running
	_init_mmc();
	sdmmc_cmd_t cmdbuf;
	sdmmc_init_cmd(&cmdbuf, MMC_GO_IDLE_STATE, MODCHIP_MAGIC, SDMMC_RSP_TYPE_0, 0);
	sdmmc_execute_cmd(&emmc_sdmmc, &cmdbuf, NULL, NULL);
	sdmmc_end(&emmc_sdmmc);
}

void modchip_send(u8 *buf){
	// emmc_sdmmc must be initialized at this point with SDMMC_BUS_WIDTH_1 and SDHCI_TIMING_MMC_ID
	// not supported by picofly fw
	sdmmc_cmd_t cmdbuf;
	sdmmc_req_t req;
	sdmmc_init_cmd(&cmdbuf, MMC_GO_IDLE_STATE, MODCHIP_MAGIC, SDMMC_RSP_TYPE_1, 0);

	req.blksize = 0x200;
	req.num_sectors = 1;
	req.is_write = 1;
	req.is_multi_block = 0;
	req.is_auto_stop_trn = 0;
	req.buf = buf;

	sdmmc_execute_cmd(&emmc_sdmmc, &cmdbuf, &req, NULL);
}

bool modchip_get_cfg(sd_loader_cfg_t *cfg){
	u8 *buf = (u8*)SDMMC_UPPER_BUFFER;
	DRESULT res = disk_read(DEV_BOOT0, buf, MODCHIP_CFG_SECTOR, 1);
	if(res == RES_OK){
		memcpy(cfg, buf + MODCHIP_CFG_OFFSET, sizeof(*cfg));
		return true;
	}

	return false;
}

void modchip_get_cfg_or_default(sd_loader_cfg_t *cfg){
	if(!(modchip_get_cfg(cfg) && modchip_is_cfg_valid(cfg))){
		memcpy(cfg, &default_cfg, sizeof(*cfg));
	}
}

bool modchip_set_cfg(sd_loader_cfg_t *cfg){
	u8 *buf = (u8*)SDMMC_UPPER_BUFFER;
	if(disk_read(DEV_BOOT0, buf, MODCHIP_CFG_SECTOR, 1) != RES_OK){
		return false;
	}
	memcpy(buf + MODCHIP_CFG_OFFSET, cfg, sizeof(*cfg));
	return disk_write(DEV_BOOT0, buf, MODCHIP_CFG_SECTOR, 1) == RES_OK;
}

bool modchip_is_cfg_valid(sd_loader_cfg_t *cfg){
	if(cfg->magic1 != MODCHIP_MAGIC || cfg->magic2 != MODCHIP_MAGIC){
		return false;
	}
	return true;
}

bool modchip_clear_cfg(){
	return modchip_set_cfg(&default_cfg);
}

void modchip_get_cfg_default(sd_loader_cfg_t *cfg){
	memcpy(cfg, &default_cfg, sizeof(*cfg));
}

static bool modchip_write_cmd(modchip_cmd_t *cmd){
	u8 *buf = (u8 *)SDMMC_UPPER_BUFFER;

	if(disk_read(DEV_BOOT0, buf, MODCHIP_CMD_SECTOR, 1) != RES_OK){
		return false;
	}

	memset(buf + MODCHIP_CMD_OFFSET, 0, 0x10);
	memcpy(buf + MODCHIP_CMD_OFFSET, cmd, sizeof(*cmd));

	return disk_write(DEV_BOOT0, buf, MODCHIP_CMD_SECTOR, 1) == RES_OK;
}

// buf must be multiple of 512
bool modchip_write_fw_update(u8 *buf, u32 size){
	u32 sec_cnt = (size + 0x1ff) / 0x200;

	// modchip will overwrite our config and fw descriptor when applying update/resetting
	DRESULT res = disk_write(DEV_BOOT0, buf, MODCHIP_FW_START_SECTOR, sec_cnt);

	if(res == RES_OK){
		// fw image written, now issue fw update command. if this fails, not much we can do
		return modchip_write_fw_update_cmd(MODCHIP_FW_START_SECTOR, sec_cnt);
	}else{
		// fw image write failed, issue reset command so modchip rewrites sdloader atleast, if this fails too, not much we can do
		modchip_write_rst_cmd();
		return false;
	}
}

bool modchip_write_fw_update_from_file(FIL *f){
	FRESULT f_res;
	bool res = true;
	u32 size = f_size(f);
	u8 *buf = (u8*)SDMMC_UPPER_BUFFER;
	u32 br;

	// we can read entire fw update into memory first
	if(size < SDMMC_UP_BUF_SZ){
		memset(buf + (size & ~(0x200 - 1)), 0, 0x200);
		f_res = f_read(f, buf, size, &br);
		if(f_res != FR_OK || br != size){
			return false;
		}

		return modchip_write_fw_update(buf, size);
	}

	u32 max_btr = SDMMC_UP_BUF_SZ & ~(0x200 - 1);
	for(u32 i = 0; i < size; i += max_btr){
		u32 btr = MIN((size - i), max_btr);

		if(btr < max_btr){
			memset(buf + (btr & ~(0x200 - 1)), 0, 0x200);

		}

		f_res = f_read(f, buf, btr, &br);
		if(f_res != FR_OK || br != btr){
			res = false;
			break;
		}

		if(disk_write(DEV_BOOT0, buf, MODCHIP_FW_START_SECTOR + (i / 0x200), (btr + (0x200 - 1)) / 0x200) != RES_OK){
			res = false;
			break;
		}
	}

	if(res){
		return modchip_write_fw_update_cmd(MODCHIP_FW_START_SECTOR, (size + (0x200 - 1) / 0x200));
	}else{
		modchip_write_rst_cmd();
		return false;
	}
}

// buf must be multiple of 512
bool modchip_write_ipl_update(u8 *buf, u32 size){
	u32 sec_cnt = (size + (0x200 - 1)) / 0x200;
	if(disk_write(DEV_BOOT0, buf, MODCHIP_BL_START_SECTOR, sec_cnt) != RES_OK){
		modchip_write_rst_cmd();
		return false;
	}
	return true;
}

bool modchip_write_ipl_update_from_file(FIL *f){
	u32 size = f_size(f);
	u8 *buf = (u8*)SDMMC_UPPER_BUFFER;
	u32 br;

	if(size > SDMMC_UP_BUF_SZ){
		return false;
	}

	memset(buf + (size & ~(0x200 - 1)), 0, 0x200);

	FRESULT res = f_read(f, buf, size, &br);

	if(res != FR_OK || br != size){
		return false;
	}

	return modchip_write_ipl_update(buf, size);
}

bool modchip_write_rst_cmd(){
	modchip_cmd_t cmd = {0};
	cmd.cmd = MODCHIP_CMD_RST;
	u8 *buf = (u8*)SDMMC_UPPER_BUFFER;
	if(!disk_read(DEV_BOOT0, buf, 1, 1)){
		return false;
	}
	memset(buf, 0, 256);
	return disk_write(DEV_BOOT0, buf, 1, 1) && modchip_write_cmd(&cmd);
}

bool modchip_write_fw_update_cmd(u32 sector_start, u32 sector_cnt){
	modchip_cmd_t cmd = {0};
	cmd.cmd = MODCHIP_CMD_FW_UPDATE;
	cmd.fw_update_info.fw_sector_cnt   = sector_cnt;
	cmd.fw_update_info.fw_sector_start = sector_start;
	return modchip_write_cmd(&cmd);
}

bool modchip_write_rollback_cmd(){
	return modchip_write_fw_update_cmd(0xffffffff, 0xffffffff);
}

bool modchip_read_desc(modchip_desc_t *desc){
	u8 *buf = (u8 *)SDMMC_UPPER_BUFFER;

	if(disk_read(DEV_BOOT0, buf, MODCHIP_DESC_SECTOR, 1) != RES_OK){
		return false;
	}

	memcpy(desc, buf + MODCHIP_DESC_OFFSET, sizeof(*desc));
	return true;
}

bool modchip_is_desc_valid(modchip_desc_t *desc){
	return desc->signature == MODCHIP_DESC_SIGNATURE;
}

bool modchip_write_bl_update_cmd(u32 sector_start, u32 sector_cnt){
	modchip_cmd_t cmd = {0};
	cmd.cmd = MODCHIP_CMD_BL_UPDATE;
	cmd.fw_update_info.fw_sector_cnt   = sector_cnt;
	cmd.fw_update_info.fw_sector_start = sector_start;
	return modchip_write_cmd(&cmd);
}

bool modchip_write_bl_update(u8 *buf, u32 size){
	u32 sec_cnt = (size + 0x1ff) / 0x200;

	// modchip will overwrite our config and fw descriptor when applying update/resetting
	DRESULT res = disk_write(DEV_BOOT0, buf, MODCHIP_RP_BL_START_SECTOR, sec_cnt);

	if(res == RES_OK){
		// fw image written, now issue fw update command. if this fails, not much we can do
		return modchip_write_bl_update_cmd(MODCHIP_RP_BL_START_SECTOR, sec_cnt);
	}else{
		// fw image write failed, issue reset command so modchip rewrites sdloader atleast, if this fails too, not much we can do
		modchip_write_rst_cmd();
		return false;
	}
}

bool modchip_write_bl_update_from_file(FIL *f){
	u32 size = f_size(f);
	u8 *buf = (u8*)SDMMC_UPPER_BUFFER;
	u32 br;

	if(size > SDMMC_UP_BUF_SZ){
		return false;
	}

	memset(buf + (size & ~(0x200 - 1)), 0xff, 0x200);

	FRESULT res = f_read(f, buf, size, &br);

	if(res != FR_OK || br != size){
		return false;
	}

	return modchip_write_bl_update(buf, size);
}