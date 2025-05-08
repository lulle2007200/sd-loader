#ifndef _MODCHIP_H
#define _MODCHIP_H

#include <utils/types.h>
#include <libs/fatfs/ff.h>

// last 64kb of boot0
#define MODCHIP_BL_START_SECTOR   0x1f80
// fw descriptor and config live on last sector
#define MODCHIP_BL_MAX_SIZE       (0x10000 - 0x200)

// last 128kgb of boot0
#define MODCHIP_FW_START_SECTOR   0x1f00
#define MODCHIP_FW_MAX_SIZE       0x20000

#define MODCHIP_RP_BL_START_SECTOR   0x1f00
#define MODCHIP_RP_BL_MAX_SIZE       0x20000

#define MODCHIP_CMD_SECTOR        0x1
#define MODCHIP_CMD_OFFSET        0x0

#define MODCHIP_DESC_SECTOR       0x1fff
#define MODCHIP_CMD_SECTOR        0x1
#define MODCHIP_CFG_SECTOR        0x1fff

#define MODCHIP_DESC_OFFSET       0x0
#define MODCHIP_CMD_OFFSET        0x0
#define MODCHIP_CFG_OFFSET        0x100

#define MODCHIP_DESC_SIGNATURE    0x9cabe959

typedef enum{
	MODCHIP_CMD_FW_UPDATE = 0x6db92148,
	MODCHIP_CMD_RST       = 0x515205c5,
	MODCHIP_CMD_BL_UPDATE = 0x7a21cae1,
}modchip_cmd;

typedef struct{
	u32 cmd;
	union{
		struct{
			u32 fw_sector_start;
			u32 fw_sector_cnt;
		}fw_update_info;
	};
}modchip_cmd_t;

typedef struct{
	u32 signature;
	u32 fw_major;
	u32 fw_minor;
	u32 sdloader_hash;
	u32 firmware_hash;
	u32 fuse_cnt;
}modchip_desc_t;

typedef struct{
	u32 magic1;
	u32 magic2;
	u8 default_payload_vol:3;
	u8 default_action:2;
	u8 disable_ofw_btn_combo:1;
	u8 disable_menu_btn_combo:1; // DO NOT USE, menu can't be forced to show otherwise
}sd_loader_cfg_t;

typedef enum{
	MODCHIP_DEFAULT_ACTION_PAYLOAD = 0x0,
	MODCHIP_DEFAULT_ACTION_OFW     = 0x1,
	MODCHIP_DEFAULT_ACTION_MENU    = 0x2,
}modchip_default_action;

typedef enum{
	MODCHIP_PAYLOAD_VOL_AUTO      = 0x0,
	MODCHIP_PAYLOAD_VOL_SD        = 0x1,
	MODCHIP_PAYLOAD_VOL_BOOT1_1MB = 0x2,
	MODCHIP_PAYLOAD_VOL_BOOT1     = 0x3,
	MODCHIP_PAYLOAD_VOL_GPP       = 0x4,
}modchip_payload_vol;

bool modchip_get_cfg(sd_loader_cfg_t *cfg);
void modchip_get_cfg_or_default(sd_loader_cfg_t *cfg);
void modchip_get_cfg_default(sd_loader_cfg_t *cfg);
bool modchip_set_cfg(sd_loader_cfg_t *cfg);
bool modchip_is_cfg_valid(sd_loader_cfg_t *cfg);
bool modchip_clear_cfg();
void modchip_confirm_execution();
void modchip_send(unsigned char *buf);

bool modchip_write_rst_cmd();
bool modchip_write_fw_update_cmd(u32 sector_start, u32 sector_cnt);
bool modchip_write_rollback_cmd();
bool modchip_read_desc(modchip_desc_t *desc);
bool modchip_is_desc_valid(modchip_desc_t *desc);
bool modchip_write_fw_update(u8 *buf, u32 size);
bool modchip_write_fw_update_from_file(FIL *f);
bool modchip_write_ipl_update(u8 *buf, u32 size);
bool modchip_write_ipl_update_from_file(FIL *f);
bool modchip_write_bl_update(u8 *buf, u32 size);
bool modchip_write_bl_update_from_file(FIL *f);
bool modchip_write_bl_update_cmd(u32 sector_start, u32 sector_cnt);

#endif