#include <gfx.h>
#include <mem/mc.h>
#include <soc/hw_init.h>
#include <storage/emmc.h>
#include <storage/sd.h>
#include <mem/heap.h>
#include <memory_map.h>
#include <storage/sdmmc.h>
#include <storage/mbr_gpt.h>
#include <string.h>
#include <soc/timer.h>
#include <usb/usbd.h>
#include <utils/btn.h>
#include <utils/types.h>
#include <utils/util.h>
#include <utils/sprintf.h>
#include <soc/t210.h>
#include <libs/fatfs/ff.h>

#include <display/di.h>
#include <gfx_utils.h>
#include <tui.h>

#define MEMLOADER_NO_MOUNT              0
#define MEMLOADER_RO                    1
#define MEMLOADER_RW                    2

#define MEMLOADER_SD                    0
#define MEMLOADER_EMMC_GPP              1
#define MEMLOADER_EMMC_BOOT0            2
#define MEMLOADER_EMMC_BOOT1            3

#define MEMLOADER_AUTOSTART_MASK        0x01
#define MEMLOADER_AUTOSTART_YES         0x01
#define MEMLOADER_AUTOSTART_NO          0x00

#define MEMLOADER_STOP_ACTION_MASK      0x06
#define MEMLOADER_STOP_ACTION_MENU      0x00  
#define MEMLOADER_STOP_ACTION_OFF       0x02
#define MEMLOADER_STOP_ACTION_RCM       0x04 

#define MEMLOADER_ERROR_SD              0x01
#define MEMLOADER_ERROR_EMMC            0x02

#define MEMLOADER_SUBSTORAGE_BY_PART   0
#define MEMLOADER_SUBSTORAGE_BY_OFFSET 1

void system_maintenance(bool refresh){
	tui_print_battery_icon(false);
	tui_dim_on_timeout(0);
}

extern void excp_reset(void);

typedef struct part_entry{
	u32 offset;
	u32 size;
}part_entry_t;

typedef struct part_table{
	u8 n_parts;
	part_entry_t parts[30];
}part_table_t;

typedef struct ums_volume_cfg_t{
	u8 mount_mode;
	u32 offset;
	u32 size;
	u8 mode;
	u8 part;
	u32 phys_size;
	u8 device;
	part_table_t *part_table;
}ums_volume_cfg_t;

typedef struct ums_loader_ums_cfg_t{
	union{
		ums_volume_cfg_t volume_cfgs[4];
		struct{
			ums_volume_cfg_t sd_cfg;
			ums_volume_cfg_t gpp_cfg;
			ums_volume_cfg_t boot0_cfg;
			ums_volume_cfg_t boot1_cfg;
		};
	};

	u32 storage_state;
	u32 stop_action;
}ums_loader_ums_cfg_t;

typedef struct ums_toggle_cb_data_t{
	tui_entry_t *entry;
	ums_loader_ums_cfg_t *config;
	u8 volume;
}ums_toggle_cb_data_t;

typedef struct{
	ums_loader_ums_cfg_t *ums_cfg;
	u8 volume;
}config_volume_data_t;

typedef struct{
	config_volume_data_t vol_data;
	tui_entry_menu_t *main_menu;
}config_volume_cfg_data_t;

void config_u32_selector_print(u8 x_pos, u8 y_pos, char *str, u32 val, u8 cur_idx, const tui_color_scheme_t *colors){
	gfx_con_setpos_rot(x_pos, y_pos);
	gfx_con_setcol(colors->fg_active, true, colors->bg_active);
	gfx_printf_rot("%s 0x", str);
	for(u8 i = 3; i != (u8)-1; i--){
		if(i == cur_idx){
			gfx_con_setcol(colors->bg_active, true, colors->fg_active);
		}else{
			gfx_con_setcol(colors->fg_active, true, colors->bg_active);
		}
		gfx_printf_rot("%02x", (val >> (i * 8)) &0xff);
	}
}

u32 config_u32_selector(u8 x_pos, u8 y_pos, char *str, u32 initial, u32 max, const tui_color_scheme_t *colors){
	u32 val = initial;
	for(u8 i = 3; i != (u8)-1; i--){
		config_u32_selector_print(x_pos, y_pos, str, val, i, colors);
		
		u32 step = 0x1 << (8 * i);
		u32 mask = 0xff << (8 * i);
		u8 btn;

		while((btn = btn_wait_for_single()) != BTN_POWER){
			u32 start_time = get_tmr_ms();
			while(btn_read() == btn){
				u32 now = get_tmr_ms();
				u32 time_out = MAX(10, 200 / (((now - start_time) / 250) + 1));

				u32 temp = 0;
				if(btn & BTN_VOL_DOWN){
					temp = (val & ~mask) | ((val - step) & mask);
					while(temp > max){
						temp -= step;
					}
				}else if(btn & BTN_VOL_UP){
					temp = (val & ~mask) | ((val + step) & mask);
					if(temp > max){
						temp = 0;
					}
				}
				val = (val & ~mask) | (mask & temp);

				config_u32_selector_print(x_pos, y_pos, str, val, i, colors);

				btn_wait_for_change_timeout(time_out, btn);
			}
		}
	}
	return val;
}

void volume_load_part_table(ums_volume_cfg_t *vol_cfg){
	sdmmc_storage_t *storage;
	memset(vol_cfg->part_table, 0, sizeof(part_table_t));
	vol_cfg->phys_size = 0;

	switch(vol_cfg->device){
	case MEMLOADER_SD:
		storage = &sd_storage;
		if(!sd_initialize(false)){
			return;
		};
		break;
	case MEMLOADER_EMMC_BOOT0:
	case MEMLOADER_EMMC_BOOT1:
		vol_cfg->phys_size = 0x2000;
		vol_cfg->size      = 0x2000;
		return;
	case MEMLOADER_EMMC_GPP:
		storage = &emmc_storage;
		if(!emmc_initialize(false)){
			return;
		}
		emmc_set_partition(EMMC_GPP);
		break;
	default:
		return;
	}


	gpt_t *gpt = (gpt_t*)SDMMC_UPPER_BUFFER;

	if(!sdmmc_storage_read(storage, 1, (sizeof(gpt_t) + 511) / 512, gpt)){
		goto error;
	}

	if(gpt->header.signature == EFI_PART){

		for(u8 i = 0; i < MIN(30, gpt->header.num_part_ents); i++){
			vol_cfg->part_table->parts[i].offset = gpt->entries[i].lba_start;
			vol_cfg->part_table->parts[i].size   = gpt->entries[i].lba_end - gpt->entries[i].lba_start + 1;
		}
		vol_cfg->part_table->n_parts = MIN(30, gpt->header.num_part_ents);

		goto out;
	}

	mbr_t *mbr = (mbr_t*)SDMMC_UPPER_BUFFER;

	if(!sdmmc_storage_read(storage, 0, 1, mbr)){
		goto error;
	}
	
	for(u8 i = 0; i < 4; i++){
		if(mbr->partitions[i].size_sct == 0){
			break;
		}
		vol_cfg->part_table->parts[i].offset = mbr->partitions[i].start_sct;
		vol_cfg->part_table->parts[i].size = mbr->partitions[i].size_sct;
		vol_cfg->part_table->n_parts = i;
	}

	
	out:
	vol_cfg->phys_size = storage->sec_cnt;
	vol_cfg->size = vol_cfg->phys_size;

	error:
	sdmmc_storage_end(storage);
}

void config_offset_update(ums_volume_cfg_t *vol_cfg, tui_entry_t *entry, tui_entry_menu_t *menu){
	if(vol_cfg->mode != MEMLOADER_SUBSTORAGE_BY_OFFSET || vol_cfg->mount_mode == MEMLOADER_NO_MOUNT){
		entry->disabled = true;
	}else{
		entry->disabled = false;
	}
	s_printf((char*)entry->title.text, "Offset 0x%08x   ", vol_cfg->offset);
}

void config_size_update(ums_volume_cfg_t *vol_cfg, tui_entry_t *entry, tui_entry_menu_t *menu){
	if(vol_cfg->mode != MEMLOADER_SUBSTORAGE_BY_OFFSET || vol_cfg->mount_mode == MEMLOADER_NO_MOUNT){
		entry->disabled = true;
	}else{
		entry->disabled = false;
	}
	vol_cfg->size = MIN(vol_cfg->size, vol_cfg->phys_size - vol_cfg->offset);
	if(vol_cfg->size == 0){
		vol_cfg->size = vol_cfg->phys_size - vol_cfg->offset;
	}
	s_printf((char*)entry->title.text, "Size   0x%08x   ", vol_cfg->size);
}

void config_part_update(ums_volume_cfg_t *vol_cfg, tui_entry_t *entry, tui_entry_menu_t *menu){
	if(vol_cfg->part > 99){
		vol_cfg->part = 99;
	}

	if(vol_cfg->mode != MEMLOADER_SUBSTORAGE_BY_PART || vol_cfg->mount_mode == MEMLOADER_NO_MOUNT){
		entry->disabled = true;
	}else{
		entry->disabled = false;
		vol_cfg->offset = vol_cfg->part_table->parts[vol_cfg->part].offset;
		vol_cfg->size = vol_cfg->part_table->parts[vol_cfg->part].size;
	}

	s_printf((char*)entry->title.text, "Part.  %02d", vol_cfg->part);

	config_offset_update(vol_cfg, &menu->entries[3], menu);
	config_size_update(vol_cfg, &menu->entries[4], menu);
}


void config_mode_update(ums_volume_cfg_t *vol_cfg, tui_entry_t *entry, tui_entry_menu_t *menu){
	const char *mode_titles[] = {
		[MEMLOADER_SUBSTORAGE_BY_OFFSET] = "Offset + Size",
		[MEMLOADER_SUBSTORAGE_BY_PART] = "Part",
	};

	if(vol_cfg->part_table->n_parts == 0 || vol_cfg->mount_mode == MEMLOADER_NO_MOUNT){
		if(vol_cfg->part_table->n_parts == 0){
			vol_cfg->mode = MEMLOADER_SUBSTORAGE_BY_OFFSET;
		}
		entry->disabled = true;
	}else{
		entry->disabled = false;
	}

	s_printf((char*)entry->title.text, "Mode   %s", mode_titles[vol_cfg->mode]);

	config_part_update(vol_cfg, &menu->entries[2], menu);
	config_offset_update(vol_cfg, &menu->entries[3], menu);
	config_size_update(vol_cfg, &menu->entries[4], menu);
}

void config_volume_update(ums_loader_ums_cfg_t *ums_cfg, tui_entry_t *entry, tui_entry_menu_t *menu){
	bool no_volume = true;
	for(u32 i = 0; i < 4; i++){
		no_volume &= ums_cfg->volume_cfgs[i].mount_mode == MEMLOADER_NO_MOUNT;
	}
	if(no_volume){
		menu->entries[5].disabled = true;
	}else{
		menu->entries[5].disabled = false;
	}
	bool emmc_disable = ums_cfg->storage_state & MEMLOADER_ERROR_EMMC;
	menu->entries[1].disabled = emmc_disable;
	menu->entries[2].disabled = emmc_disable;
	menu->entries[3].disabled = emmc_disable;

	bool sd_disable = ums_cfg->storage_state & MEMLOADER_ERROR_SD;
	menu->entries[0].disabled = sd_disable;
}

static const char* mount_mode_titles[] = {
	[MEMLOADER_RO] = "RO",
	[MEMLOADER_RW] = "RW",
	[MEMLOADER_NO_MOUNT] = "--",
};

void config_mount_mode_update(config_volume_cfg_data_t *cfg_data, tui_entry_t *entry, tui_entry_menu_t *menu){
	config_volume_data_t *vol_data = &cfg_data->vol_data;
	ums_volume_cfg_t *vol_cfg = &vol_data->ums_cfg->volume_cfgs[vol_data->volume];

	s_printf((char*)entry->title.text, "Mount  %s", mount_mode_titles[vol_cfg->mount_mode]);
	config_volume_update(vol_data->ums_cfg, NULL, cfg_data->main_menu);
	config_mode_update(vol_cfg, &menu->entries[1], menu);
	tui_print_menu(cfg_data->main_menu);
}

void config_tot_sz_update(ums_volume_cfg_t *vol_cfg, tui_entry_t *entry, tui_entry_menu_t *menu){
	s_printf((char*)entry->title.text, "TSize  0x%08x", vol_cfg->phys_size);
}

void config_mount_mode_cb(void *data, tui_entry_t *entry, tui_entry_menu_t *menu){
	config_volume_cfg_data_t *cfg_data = (config_volume_cfg_data_t*) data;
	config_volume_data_t *vol_data = &cfg_data->vol_data;
	ums_volume_cfg_t *vol_cfg = &vol_data->ums_cfg->volume_cfgs[vol_data->volume];

	vol_cfg->mount_mode++;
	vol_cfg->mount_mode %= 3;

	config_mount_mode_update(cfg_data, entry, menu);
}

void config_mode_cb(void *data, tui_entry_t *entry, tui_entry_menu_t *menu){
	config_volume_data_t *vol_data = (config_volume_data_t*)data;
	ums_volume_cfg_t *vol_cfg = &vol_data->ums_cfg->volume_cfgs[vol_data->volume];

	switch(vol_cfg->mode){
	case MEMLOADER_SUBSTORAGE_BY_OFFSET:
		if(vol_cfg->part_table->n_parts > 0){
			vol_cfg->mode = MEMLOADER_SUBSTORAGE_BY_PART;
		}
		break;
	case MEMLOADER_SUBSTORAGE_BY_PART:
		vol_cfg->mode = MEMLOADER_SUBSTORAGE_BY_OFFSET;
		break;
	}

	config_mode_update(vol_cfg, entry, menu);
}

void config_offset_cb(void *data, tui_entry_t *entry, tui_entry_menu_t *menu){
	config_volume_data_t *vol_data = (config_volume_data_t*)data;
	ums_volume_cfg_t *vol_cfg = &vol_data->ums_cfg->volume_cfgs[vol_data->volume];

	vol_cfg->offset = config_u32_selector(entry->action_modifying.x_pos, entry->action_modifying.y_pos, "Offset", vol_cfg->offset, vol_cfg->phys_size - 1, menu->colors);

	config_offset_update(vol_cfg, &menu->entries[3], menu);
	config_size_update(vol_cfg, &menu->entries[4], menu);
}

void config_part_cb(void *data, tui_entry_t *entry, tui_entry_menu_t *menu){
	config_volume_data_t *vol_data = (config_volume_data_t*)data;
	ums_volume_cfg_t *vol_cfg = &vol_data->ums_cfg->volume_cfgs[vol_data->volume];

	vol_cfg->part = (vol_cfg->part + 1) % vol_cfg->part_table->n_parts;

	config_part_update(vol_cfg, &menu->entries[2], menu);
}

void config_size_cb(void *data, tui_entry_t *entry, tui_entry_menu_t *menu){
	config_volume_data_t *vol_data = (config_volume_data_t*)data;
	ums_volume_cfg_t *vol_cfg = &vol_data->ums_cfg->volume_cfgs[vol_data->volume];

	u32 max = vol_cfg->phys_size - vol_cfg->offset;
	vol_cfg->size = config_u32_selector(entry->action_modifying.x_pos, entry->action_modifying.y_pos, "Size  ", vol_cfg->size, max, menu->colors);

	config_size_update(vol_cfg, &menu->entries[4], menu);
}

void config_volume_cb(void *data, tui_entry_t *entry, tui_entry_menu_t *menu){
	menu->colors = &TUI_COLOR_SCHEME_SHADOW;
	tui_print_menu(menu);

	config_volume_data_t *vol_data = (config_volume_data_t*) data;
	u8 vol = vol_data->volume;
	ums_loader_ums_cfg_t *ums_cfg = vol_data->ums_cfg;
	ums_volume_cfg_t *vol_cfg = &ums_cfg->volume_cfgs[vol];

	config_volume_cfg_data_t cfg_data;
	cfg_data.vol_data = *vol_data;
	cfg_data.main_menu = menu;

	part_table_t part_table;
	vol_cfg->part_table = &part_table;

	static char vol_offset_str[25]     = "";
	static char vol_sz_str[25]         = "";
	static char vol_part_str[25]       = "";
	static char vol_tot_sz_str[25]     = "";
	static char vol_mount_mode_str[25] = "";
	static char vol_mode_str[25]       = "";

	tui_entry_t volume_menu_entries[] = {
		[0] = TUI_ENTRY_ACTION_MODIFYING_NO_BLANK(vol_mount_mode_str, config_mount_mode_cb, &cfg_data, false, &volume_menu_entries[1]),
		[1] = TUI_ENTRY_ACTION_MODIFYING_NO_BLANK(vol_mode_str,       config_mode_cb,       &cfg_data, false, &volume_menu_entries[2]),
		[2] = TUI_ENTRY_ACTION_MODIFYING_NO_BLANK(vol_part_str,       config_part_cb,       &cfg_data, false, &volume_menu_entries[3]),
		[3] = TUI_ENTRY_ACTION_MODIFYING_NO_BLANK(vol_offset_str,     config_offset_cb,     &cfg_data, false, &volume_menu_entries[4]),
		[4] = TUI_ENTRY_ACTION_MODIFYING_NO_BLANK(vol_sz_str,         config_size_cb,       &cfg_data, false, &volume_menu_entries[5]),
		[5] = TUI_ENTRY_ACTION_NO_BLANK          (vol_tot_sz_str,     NULL,                 &cfg_data, true,  &volume_menu_entries[6]),
		[6] = TUI_ENTRY_TEXT("\n", &volume_menu_entries[7]),
		[7] = TUI_ENTRY_BACK(NULL),
	};

	tui_entry_menu_t volume_menu = {
		.entries = volume_menu_entries,
		.title = {
			.text = entry->title.text
		},
		.pos_x = menu->pos_x + menu->width * 8 + 8,
		.pos_y = menu->pos_y,
		.pad   = 20,
		.height = menu->height,
		.width = 20,
		.colors = &TUI_COLOR_SCHEME_DEFAULT,
		.timeout_ms = 0,
		.show_title = true,
	};

	volume_load_part_table(vol_cfg);
	config_tot_sz_update(vol_cfg, &volume_menu_entries[5], &volume_menu);
	config_mount_mode_update(&cfg_data, &volume_menu_entries[0], &volume_menu);

	tui_menu_start_rot(&volume_menu);

	vol_cfg->part_table = NULL;

	config_volume_update(ums_cfg, entry, menu);
	menu->colors = &TUI_COLOR_SCHEME_DEFAULT;
}

void ums_set_text(void *data, const char *text){
	tui_color_scheme_t *colors = (tui_color_scheme_t*)data;

	u32 x;
	u32 y;
	gfx_con_getpos_rot(&x, &y);
	gfx_clear_rect_rot(colors->bg, x, y, gfx_ctxt.height - x, gfx_ctxt.width - y);
	gfx_printf_rot("%s", text);
	gfx_con_setpos_rot(x, y);
}

void ums_start_ums(ums_loader_ums_cfg_t *ums_cfg, const tui_color_scheme_t *colors){
	u32 x, y;
	gfx_con_get_origin_rot(&x, &y);
	u32 width = gfx_ctxt.height - x;
	u32 height = gfx_ctxt.width - y;

	static const char *vol_names[] = {
		[MEMLOADER_SD]         = "SD   ",
		[MEMLOADER_EMMC_BOOT0] = "BOOT0",
		[MEMLOADER_EMMC_BOOT1] = "BOOT1",
		[MEMLOADER_EMMC_GPP]   = "GPP  ",
	};

	gfx_clear_rect_rot(colors->bg, x, y, width, height);
	gfx_con_setpos_rot(x, y);
	gfx_con_setcol(colors->fg, 1, colors->bg);

	gfx_printf_rot("Running UMS\n");
	gfx_con_setcol(colors->fg_disabled, 1, colors->bg);
	for(u8 i = MEMLOADER_SD; i <= MEMLOADER_EMMC_BOOT1; i++){
		gfx_printf_rot("%s %s\n", vol_names[i], mount_mode_titles[ums_cfg->volume_cfgs[i].mount_mode]);
	}
	gfx_con_setcol(colors->fg, 1, colors->bg);
	gfx_printf_rot("\nTo stop, hold VOL+ and VOL-,\n or eject all volumes\n");
	gfx_con_setcol(colors->fg_disabled, 1, colors->bg);
	gfx_printf_rot("Status: ");

	usb_ctxt_vol_t volumes[4] = {0};
	u32 volumes_cnt = 0;

	ums_volume_cfg_t *vol_cfg = &ums_cfg->sd_cfg;
	if(vol_cfg->mount_mode != MEMLOADER_NO_MOUNT){
		if(!(ums_cfg->storage_state & MEMLOADER_ERROR_SD)){
			volumes[volumes_cnt].offset = vol_cfg->offset;
			volumes[volumes_cnt].partition = 0;
			volumes[volumes_cnt].sectors = vol_cfg->size;
			volumes[volumes_cnt].type = MMC_SD;
			volumes[volumes_cnt].ro = vol_cfg->mount_mode == MEMLOADER_RO;

			volumes_cnt++;

		}
	}

	vol_cfg = &ums_cfg->gpp_cfg;
	if(vol_cfg->mount_mode != MEMLOADER_NO_MOUNT){
		if(!(ums_cfg->storage_state & MEMLOADER_ERROR_EMMC)){
			volumes[volumes_cnt].offset = vol_cfg->offset;
			volumes[volumes_cnt].partition = EMMC_GPP + 1;
			volumes[volumes_cnt].sectors = vol_cfg->size;
			volumes[volumes_cnt].type = MMC_EMMC;
			volumes[volumes_cnt].ro = vol_cfg->mount_mode == MEMLOADER_RO;

			volumes_cnt++;
		}
	}

	vol_cfg = &ums_cfg->boot0_cfg;
	if(vol_cfg->mount_mode != MEMLOADER_NO_MOUNT){
		if(!(ums_cfg->storage_state & MEMLOADER_ERROR_EMMC)){
			volumes[volumes_cnt].offset = vol_cfg->offset;
			volumes[volumes_cnt].partition = EMMC_BOOT0 + 1;
			volumes[volumes_cnt].sectors = vol_cfg->size;
			volumes[volumes_cnt].type = MMC_EMMC;
			volumes[volumes_cnt].ro = vol_cfg->mount_mode == MEMLOADER_RO;

			volumes_cnt++;
		}
	}

	vol_cfg = &ums_cfg->boot1_cfg;
	if(vol_cfg->mount_mode != MEMLOADER_NO_MOUNT){
		if(!(ums_cfg->storage_state & MEMLOADER_ERROR_EMMC)){
			volumes[volumes_cnt].offset = vol_cfg->offset;
			volumes[volumes_cnt].partition = EMMC_BOOT1 + 1;
			volumes[volumes_cnt].sectors = vol_cfg->size;
			volumes[volumes_cnt].type = MMC_EMMC;
			volumes[volumes_cnt].ro = vol_cfg->mount_mode == MEMLOADER_RO;

			volumes_cnt++;
		}
	}

	usb_ctxt_t usbs;

	usbs.label = (void*)colors;
	usbs.set_text = &ums_set_text;
	usbs.system_maintenance = &system_maintenance;
	usbs.volumes_cnt = volumes_cnt;
	usbs.volumes = volumes;

	usb_device_gadget_ums(&usbs);

	msleep(1000);

	gfx_clear_rect_rot(colors->bg, x, y, width, height);
}

void ums_start_cb(void *data, tui_entry_t *entry, tui_entry_menu_t *menu){
	ums_loader_ums_cfg_t *ums_cfg = (ums_loader_ums_cfg_t*)data;

	const tui_color_scheme_t *colors = menu->colors;

	menu->colors = &TUI_COLOR_SCHEME_SHADOW;
	tui_print_menu(menu);

	u32 ox, oy;
	gfx_con_get_origin_rot(&ox, &oy);
	gfx_con_set_origin_rot(menu->pos_x + menu->width * 8 + 8, menu->pos_y);
	gfx_con_setcol(colors->fg, true, colors->bg);

	ums_start_ums(ums_cfg, colors);

	gfx_con_set_origin_rot(ox, oy);
	menu->colors = &TUI_COLOR_SCHEME_DEFAULT;
}

void init_ums_cfg(ums_loader_ums_cfg_t *ums_cfg){
	memset(ums_cfg, 0, sizeof(ums_loader_ums_cfg_t));

	if(!sd_initialize(false)){
		ums_cfg->storage_state |= MEMLOADER_ERROR_SD;
	}
	sd_end();
	if(!emmc_initialize(false)){
		ums_cfg->storage_state |= MEMLOADER_ERROR_EMMC;
	}
	sdmmc_storage_end(&emmc_storage);

	ums_cfg->boot0_cfg.device   = MEMLOADER_EMMC_BOOT0;
	ums_cfg->boot0_cfg.mode 	= MEMLOADER_SUBSTORAGE_BY_OFFSET;
	ums_cfg->boot1_cfg.device   = MEMLOADER_EMMC_BOOT1;
	ums_cfg->boot1_cfg.mode 	= MEMLOADER_SUBSTORAGE_BY_OFFSET;
	ums_cfg->gpp_cfg.device     = MEMLOADER_EMMC_GPP;
	ums_cfg->gpp_cfg.mode 	    = MEMLOADER_SUBSTORAGE_BY_OFFSET;
	ums_cfg->sd_cfg.device      = MEMLOADER_SD;
	ums_cfg->sd_cfg.mode 	    = MEMLOADER_SUBSTORAGE_BY_OFFSET;

	if(!(ums_cfg->storage_state & MEMLOADER_ERROR_SD)){
		ums_cfg->sd_cfg.mount_mode = MEMLOADER_RW;
	}
}

void ums_reload_cb(void *data, tui_entry_t *entry, tui_entry_menu_t *menu){
	ums_loader_ums_cfg_t *ums_cfg = (ums_loader_ums_cfg_t*)data;

	init_ums_cfg(ums_cfg);

	config_volume_update(ums_cfg, NULL, menu);
}

void ums(u32 pos_x, u32 pos_y){
	static ums_loader_ums_cfg_t ums_cfg = {0};

	init_ums_cfg(&ums_cfg);

	tui_entry_t ums_menu_entries[] = {
		[0] = TUI_ENTRY_ACTION_MODIFYING_NO_BLANK("SD    CFG",  config_volume_cb, (&(config_volume_data_t){.ums_cfg = &ums_cfg, .volume = MEMLOADER_SD}),         false, &ums_menu_entries[1]),
		[1] = TUI_ENTRY_ACTION_MODIFYING_NO_BLANK("GPP   CFG",  config_volume_cb, (&(config_volume_data_t){.ums_cfg = &ums_cfg, .volume = MEMLOADER_EMMC_GPP}),   false, &ums_menu_entries[2]),
		[2] = TUI_ENTRY_ACTION_MODIFYING_NO_BLANK("BOOT0 CFG",  config_volume_cb, (&(config_volume_data_t){.ums_cfg = &ums_cfg, .volume = MEMLOADER_EMMC_BOOT0}), false, &ums_menu_entries[3]),
		[3] = TUI_ENTRY_ACTION_MODIFYING_NO_BLANK("BOOT1 CFG",  config_volume_cb, (&(config_volume_data_t){.ums_cfg = &ums_cfg, .volume = MEMLOADER_EMMC_BOOT1}), false, &ums_menu_entries[4]),
		[4] = TUI_ENTRY_TEXT("\n", &ums_menu_entries[5]),
		[5] = TUI_ENTRY_ACTION_MODIFYING_NO_BLANK("Start",      ums_start_cb,      &ums_cfg,                                                                      false, &ums_menu_entries[6]),
		[6] = TUI_ENTRY_ACTION_MODIFYING_NO_BLANK("Reload",     ums_reload_cb,     &ums_cfg,                                                                      false, &ums_menu_entries[7]),
		[7] = TUI_ENTRY_BACK(NULL),
	};

	tui_entry_menu_t ums_menu = {
		.entries = ums_menu_entries,
		.title = {
			.text = "UMS"
		},
		.pos_x = pos_x,
		.pos_y = pos_y,
		.pad   = 9,
		.height = 11,
		.width = 9,
		.colors = &TUI_COLOR_SCHEME_DEFAULT,
		.timeout_ms = 0,
		.show_title = true,
	};

	config_volume_update(&ums_cfg, NULL, &ums_menu);

	tui_menu_start_rot(&ums_menu);
}