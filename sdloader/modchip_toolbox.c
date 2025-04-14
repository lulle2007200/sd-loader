#include "files.h"
#include "modchip.h"
#include <libs/fatfs/ff.h>
#include <soc/timer.h>
#include <storage/emmc.h>
#include <string.h>
#include <tui.h>
#include <gfx.h>
#include <utils/sprintf.h>
#include <utils/types.h>
#include "memory_map.h"

static bool command_pending = false;

typedef enum{
	TOOLBOX_INVALID_UPDATE_SIZE,
}toolbox_status;

typedef struct{
	tui_entry_menu_t *menu;
	void *data;
}confirm_menu_data_t;

typedef struct{
	FIL *f;
	const char *path;
	u8 drive;
}fw_update_info;

static void handle_file_error(const char *path, u8 drive, FRESULT res){
	char msg[48];
	if(res == FR_NO_FILE){
		s_printf(&msg[0], "No %s found!", path);
	}else if(res != FR_OK){
		s_printf(&msg[0], "Error reading %s from %s!", path, drive_friendly_names[drive]);
	}else{
		return;
	}
	tui_print_status(COL_ORANGE, msg);
}

static void handle_toolbox_error(toolbox_status res, void *extra_info){
	char msg[48];
	if(res == TOOLBOX_INVALID_UPDATE_SIZE){
		fw_update_info * update_info = (fw_update_info*)extra_info; 
		s_printf(&msg[0], "%s on %s too large!", update_info->path, drive_friendly_names[update_info->drive]);
	}else{
		return;
	}
	tui_print_status(COL_ORANGE, msg);
}

void info_cb(void *data, tui_entry_t *entry, tui_entry_menu_t *menu){
	char sig_str[25]      = "Signature : [Invalid]";
	char ver_str[25]      = "FW Version: --";
	char fw_hash_str[25]  = "FW Hash   : --";
	char ipl_hash_str[25] = "IPL Hash  : --";
	char fuse_str[25]     = "Fuse Count: --";

	modchip_desc_t desc = {0};
	if(!modchip_read_desc(&desc)){
		tui_print_status(COL_ORANGE, "Failed to read FW info!");
		return;
	}

	menu->colors = &TUI_COLOR_SCHEME_SHADOW;
	tui_print_menu(menu);

	if(modchip_is_desc_valid(&desc)){
		s_printf(&sig_str[0],      "Signature : 0x%08x", desc.signature);
		s_printf(&ver_str[0],      "FW Version: %d.%d", desc.fw_major > 9 ? 9 : desc.fw_major, desc.fw_minor > 99 ? 99 : desc.fw_minor);
		s_printf(&fw_hash_str[0],  "FW Hash   : 0x%08x", desc.firmware_hash);
		s_printf(&ipl_hash_str[0], "IPL Hash  : 0x%08x", desc.sdloader_hash);
		s_printf(&fuse_str[0],     "Fuse Count: %d", desc.fuse_cnt > 999 ? 999 : desc.fuse_cnt);
	}


	tui_entry_t menu_entries[] = {
		[0] = TUI_ENTRY_TEXT_DISABLED(sig_str,      &menu_entries[1]),
		[1] = TUI_ENTRY_TEXT_DISABLED(ver_str,      &menu_entries[2]),
		[2] = TUI_ENTRY_TEXT_DISABLED(fuse_str,     &menu_entries[3]),
		[3] = TUI_ENTRY_TEXT_DISABLED(fw_hash_str,  &menu_entries[4]),
		[4] = TUI_ENTRY_TEXT_DISABLED(ipl_hash_str, &menu_entries[5]),
		[5] = TUI_ENTRY_TEXT("\n", &menu_entries[6]),
		[6] = TUI_ENTRY_BACK(NULL),
	};

	tui_entry_menu_t info_menu = {
		.entries    = menu_entries,
		.title      = {
			.text = "FW Info"
		},
		.pos_x      = menu->pos_x + (menu->width * 8 + 8),
		.pos_y      = menu->pos_y,
		.pad        = 22,
		.height     = ARRAY_SIZE(menu_entries) + 2,
		.width      = 22,
		.colors     = &TUI_COLOR_SCHEME_DEFAULT,
		.timeout_ms = 0,
		.show_title = true,
	};

	tui_menu_start_rot(&info_menu);

	menu->colors = &TUI_COLOR_SCHEME_DEFAULT;
}

static void confirm_menu(const char *title, tui_action_modifying_cb_t cb, tui_entry_menu_t *menu, void *data){
	menu->colors = &TUI_COLOR_SCHEME_SHADOW;
	tui_print_menu(menu);

	confirm_menu_data_t confirm_data = {
		.menu = menu,
		.data = data,
	};

	tui_entry_t menu_entries[] = {
		[0] = TUI_ENTRY_ACTION_MODIFYING_NO_BLANK("Confirm", cb, &confirm_data, false, &menu_entries[1]),
		[1] = TUI_ENTRY_TEXT("\n\n\n\n\n", &menu_entries[2]),
		[2] = TUI_ENTRY_BACK(NULL),
	};

	u8 pad = strlen(title);
	pad = pad < 4 ? 4 : pad;

	tui_entry_menu_t confirm_menu = {
		.entries    = menu_entries,
		.title      = {
			.text = title
		},
		.pos_x      = menu->pos_x + (menu->width * 8 + 8),
		.pos_y      = menu->pos_y,
		.pad        = pad,
		.height     = ARRAY_SIZE(menu_entries) + 7,
		.width      = pad,
		.colors     = &TUI_COLOR_SCHEME_DEFAULT,
		.timeout_ms = 0,
		.show_title = true,
	};

	tui_menu_start_rot(&confirm_menu);

	menu->colors = &TUI_COLOR_SCHEME_DEFAULT;
}

static void print_cmd_fail(){
	tui_print_status(COL_ORANGE, "Failed to send command!");
}

static void disable_all_cmds(tui_entry_menu_t *menu){
	menu->entries[1].disabled = true;
	menu->entries[2].disabled = true;
	menu->entries[3].disabled = true;
	menu->entries[4].disabled = true;
}

static void reset(void *data, tui_entry_t *entry, tui_entry_menu_t *menu){
	tui_entry_menu_t *top_menu = ((confirm_menu_data_t*)data)->menu;

	u32 start = get_tmr_ms();
	tui_print_status(COL_TEAL, "Writing reset command...");

	bool res = modchip_write_rst_cmd();

	if(get_tmr_ms() - start < 1000){
		msleep(1000 - (get_tmr_ms() - start));
	}

	if(!res){
		print_cmd_fail();
	}else{
		tui_print_status(COL_TEAL, "Reset command sent. Reboot Console!");
		command_pending = true;
		disable_all_cmds(top_menu);
		tui_print_menu(top_menu);
		entry->disabled = true;
	}

}

static void rollback(void *data, tui_entry_t *entry, tui_entry_menu_t *menu){
	tui_entry_menu_t *top_menu = ((confirm_menu_data_t*)data)->menu;

	u32 start = get_tmr_ms();
	tui_print_status(COL_TEAL, "Writing rollback command...");

	bool res = modchip_write_rollback_cmd();

	if(get_tmr_ms() - start < 1000){
		msleep(1000 - (get_tmr_ms() - start));
	}

	if(!res){
		print_cmd_fail();
	}else{
		tui_print_status(COL_TEAL, "Rollback command sent. Reboot console!");
		command_pending = true;
		disable_all_cmds(top_menu);
		tui_print_menu(top_menu);
		entry->disabled = true;
	}
}

static void reset_cb(void *data, tui_entry_t *entry, tui_entry_menu_t *menu){
	confirm_menu("Reset Modchip", reset, menu, NULL);
}

static void update_fw(void *data, tui_entry_t *entry, tui_entry_menu_t *menu){
	confirm_menu_data_t *confirm_data = (confirm_menu_data_t*)data;
	tui_entry_menu_t *top_menu = confirm_data->menu;
	fw_update_info *update_info = (fw_update_info*)confirm_data->data;

	u32 start = get_tmr_ms();
	tui_print_status(COL_TEAL, "Writing FW update command...");

	bool res = modchip_write_fw_update_from_file(update_info->f);

	if(get_tmr_ms() - start < 1000){
		msleep(1000 - (get_tmr_ms() - start));
	}

	if(!res){
		tui_print_status(COL_ORANGE, "FW update command fail. Reboot console!");
	}else{
		tui_print_status(COL_TEAL, "FW update command sent. Reboot console!");
	}
	command_pending = true;
	disable_all_cmds(top_menu);
	tui_print_menu(top_menu);
	entry->disabled = true;
}

static void update_ipl(void *data, tui_entry_t *entry, tui_entry_menu_t *menu){
	confirm_menu_data_t *confirm_data = (confirm_menu_data_t*)data;
	tui_entry_menu_t *top_menu = confirm_data->menu;
	fw_update_info *update_info = (fw_update_info*)confirm_data->data;

	u32 start = get_tmr_ms();
	tui_print_status(COL_TEAL, "Writing IPL update command...");

	bool res = modchip_write_ipl_update_from_file(update_info->f);

	if(get_tmr_ms() - start < 1000){
		msleep(1000 - (get_tmr_ms() - start));
	}

	if(!res){
		tui_print_status(COL_ORANGE, "IPL update command fail. Reboot console!");
	}else{
		tui_print_status(COL_TEAL, "IPL update command sent. Reboot console!");
	}
	command_pending = true;
	disable_all_cmds(top_menu);
	tui_print_menu(top_menu);
	entry->disabled = true;
}

static void update_cb(const char *path, u32 size_max, tui_action_modifying_cb_t cb, tui_entry_menu_t *menu){
	u8 drive;
	FIL f;
	FRESULT res;

	res = open_file_on_any(path, &f, &drive);

	if(res != FR_OK){
		handle_file_error(path, drive, res);
		return;
	}

	u32 size = f_size(&f);

	fw_update_info update_info = {
		.drive = drive,
		.f = &f,
		.path = path,
	};

	if(size > size_max){
		handle_toolbox_error(TOOLBOX_INVALID_UPDATE_SIZE, &update_info);
		goto out;
	}


	char title[25];

	s_printf(&title[0], "Apply update from %s", drive_friendly_names[drive]);

	confirm_menu(title, cb, menu, &update_info);

	out:
	f_close(&f);
}

static void fw_update_cb(void *data, tui_entry_t *entry, tui_entry_menu_t *menu){
	const char *path = "update.bin"; 
	update_cb(path, MODCHIP_FW_MAX_SIZE, update_fw, menu);
}

static void fw_rollback_cb(void *data, tui_entry_t *entry, tui_entry_menu_t *menu){
	confirm_menu("FW Rollback", rollback, menu, NULL);
}

static void ipl_update_cb(void *data, tui_entry_t *entry, tui_entry_menu_t *menu){
	const char *path = "sdloader.enc";
	update_cb(path, MODCHIP_BL_MAX_SIZE, update_ipl, menu);
}

static void ipl_settings_cb(void *data, tui_entry_t *entry, tui_entry_menu_t *menu){
}

void toolbox(u32 x, u32 y){
	emmc_initialize(false);

	tui_entry_t menu_entries[] = {
		[0] = TUI_ENTRY_ACTION_MODIFYING_NO_BLANK("FW  Info", info_cb, NULL, false, &menu_entries[1]),
		[1] = TUI_ENTRY_ACTION_MODIFYING_NO_BLANK("FW  Reset", reset_cb, NULL, command_pending, &menu_entries[2]),
		[2] = TUI_ENTRY_ACTION_MODIFYING_NO_BLANK("FW  Update", fw_update_cb, NULL, command_pending, &menu_entries[3]),
		[3] = TUI_ENTRY_ACTION_MODIFYING_NO_BLANK("FW  Rollback", fw_rollback_cb, NULL, command_pending, &menu_entries[4]),
		[4] = TUI_ENTRY_ACTION_MODIFYING_NO_BLANK("IPL Update", ipl_update_cb, NULL, command_pending, &menu_entries[6]),
		[5] = TUI_ENTRY_ACTION_MODIFYING_NO_BLANK("IPL Settings", ipl_settings_cb, NULL, false, &menu_entries[6]),
		[6] = TUI_ENTRY_TEXT("\n", &menu_entries[7]),
		[7] = TUI_ENTRY_BACK(NULL)
	};

	tui_entry_menu_t menu = {
		.entries    = menu_entries,
		.title      = {
			.text = "Toolbox"
		},
		.pos_x      = x,
		.pos_y      = y,
		.pad        = 12,
		.height     = ARRAY_SIZE(menu_entries) + 2,
		.width      = 12,
		.colors     = &TUI_COLOR_SCHEME_DEFAULT,
		.timeout_ms = 0,
		.show_title = true,
	};

	tui_menu_start_rot(&menu);
}