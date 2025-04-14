#include "ums.h"

#include <libs/fatfs/ff.h>
#include <memory_map.h>
#include <gfx.h>
#include <soc/timer.h>
#include <storage/emmc.h>
#include <storage/mmc.h>
#include <storage/sd.h>
#include <storage/sdmmc.h>
#include <string.h>
#include <tui.h>
#include <utils/btn.h>
#include <utils/sprintf.h>
#include <utils/types.h>
#include <utils/util.h>
#include <soc/t210.h>
#include <soc/hw_init.h>
#include <mem/mc.h>
#include <mem/heap.h>
#include <display/di.h>
#include "logo.bmp.h"
#include <power/bq24193.h>
#include <storage/sdmmc_driver.h>
#include "modchip.h"
#include <soc/i2c.h>
#include <power/max77620.h>
#include "loader.h"
#include "modchip_toolbox.h"
#include "files.h"
#include <soc/bpmp.h>

typedef struct{
	void *addr;
	u32 size;
}payload_ctx_t;

typedef enum{
	SD_LOADER_OK = 0,
	SD_LOADER_INV_PAYLOAD_SZ,    // found payload, but too large
	SD_LOADER_NO_PAYLOAD_ON_DEV, // no payload found on active drive
	SD_LOADER_NO_PAYLOAD,        // no payload found on any drive
	SD_LOADER_ERR_PAYLOAD,       // found payload, but read error
	SD_LOADER_FORCE_MENU,        // forced menu by button combo
	SD_LOADER_ERROR              // other error
} SD_LOADER_STATUS;

extern void pivot_stack(u32 stack_top);
extern void excp_reset(void);

static bool display_init_done = false;
static sd_loader_cfg_t sdloader_cfg;
static payload_ctx_t payload_ctx = {0};


static SD_LOADER_STATUS read_payload(FIL *f){
	FSIZE_t sz = f_size(f);
	FRESULT res;
	SD_LOADER_STATUS sd_res = SD_LOADER_OK;

 	if(sz > PAYLOAD_SIZE_MAX){
 		return SD_LOADER_INV_PAYLOAD_SZ;
 	}

 	void *buf = (void*)PAYLOAD_BUF_ADDR;

 	u32 br;
 	res = f_read(f, (void*)buf, sz, &br);

 	if(res != FR_OK || br != sz){
 		return SD_LOADER_ERR_PAYLOAD;
 	}

 	payload_ctx.addr = (void *)PAYLOAD_BUF_ADDR;
 	payload_ctx.size = sz;

	return sd_res;
}

static void display_logo(){
	u32 x_pos = (gfx_ctxt.height - logo_width) / 2;
	gfx_render_bmp_2bit_rot(logo_arr, logo_width, logo_height, x_pos, 25);
}

static void init_display(){
	if(!display_init_done){
		display_init();
		u8 *fb = (u8*)display_init_window_a_pitch_small_palette(logo_lut, sizeof(logo_lut) / 4);
		gfx_init_ctxt(fb, 180, 320, 192);
		gfx_con_init();
		gfx_con_set_origin_rot(0, 0);
		gfx_con_setpos_rot(0, 0);
		display_init_done = true;
		display_backlight_pwm_init();
		display_backlight_brightness(128, 1000);
		display_logo();
	}
}

static void handle_sdloader_status(SD_LOADER_STATUS res, u32 extra_info){
	char msg[48];
	u8 col = COL_ORANGE;

	switch(res){
	case SD_LOADER_INV_PAYLOAD_SZ:
		s_printf(msg, "Payload on %s too large!", drive_friendly_names[extra_info]);
		break;
	case SD_LOADER_FORCE_MENU:
		s_printf(msg, "User forced menu!");
		col = COL_TEAL;
		break;
	case SD_LOADER_OK:
		return;
	default:
		s_printf(msg, "An error occured!");
		break;
	}

	init_display();
	tui_print_status(col, msg);
}

__attribute__((noreturn)) static void launch_payload(){
	emmc_end();
	sd_end();
	hw_deinit(false, 0);
	/* payloads (may) expect to be loaded at 0x40010000, relocate before jumping to payload */
	reloc_and_start_payload(payload_ctx.addr, payload_ctx.size);
	while(1){}
}

static void handle_file_error(const char *path, u8 drive, FRESULT res){
	char msg[48];
	if(res == FR_NO_FILE){
		s_printf(&msg[0], "No %s found!", path);
	}else if(res != FR_OK){
		s_printf(&msg[0], "Error reading %s from %s!", path, drive_friendly_names[drive]);
	}else{
		return;
	}
	init_display();
	tui_print_status(COL_ORANGE, msg);
}

static SD_LOADER_STATUS load_payload(){
	FIL f;
	FRESULT res;
	u8 drive;

	const char *path = "payload.bin";

	res = open_file_on_any(path, &f, &drive);

	if(res != FR_OK){
		handle_file_error(path, drive, res);
		return SD_LOADER_ERROR;
	}

	SD_LOADER_STATUS sd_res = read_payload(&f);

	if(sd_res != SD_LOADER_OK){
		handle_sdloader_status(sd_res, drive);
	}

	f_close(&f);

	return sd_res;
}

static void clear_screen_except_logo_and_status(){
	gfx_clear_rect_rot(COL_BLACK, 0, 88, gfx_ctxt.height, gfx_ctxt.width - 80);
}

static void start_ums(){
	gfx_con_setpos_rot(0, 0);
	clear_screen_except_logo_and_status();
	ums(0, 88);
}

static void power_off_cb(void *data){
	power_set_state(POWER_OFF);
}

static void rcm_cb(void *data){
	rcm_if_t210_or_off();
}

static void ums_cb(void *data){
	start_ums();
}

static void ofw_cb(void *data){
	power_set_state(REBOOT_BYPASS_FUSES);
}

static void try_launch_payload(){
	SD_LOADER_STATUS res = SD_LOADER_ERROR;

	//TODO: probably should disable backlight if payload is big enough to overwrite frame buffer
	res = load_payload();

	if(res == SD_LOADER_OK){
		launch_payload();
	}
}

static void retry_cb(void *data){
	try_launch_payload();
}

static void start_toolbox(){
	gfx_con_setpos_rot(0, 0);
	clear_screen_except_logo_and_status();
	toolbox(0, 88);
}

static void toolbox_cb(){
	start_toolbox();
}

static void do_menu(){
	bool t210 = is_t210();
	tui_entry_menu_t menu = {
		.colors = &TUI_COLOR_SCHEME_DEFAULT,
		.height = 5,
		.pad = 10,
		.width = 10,
		.pos_x = (gfx_ctxt.height - 10 * 8) / 2,
		.pos_y = 88,
		.title = {
			.text = NULL,
		},
		.timeout_ms = 60 * 1000, // return and power off if no action for 1min
	};

	tui_entry_t menu_more[] = {
		[0] = TUI_ENTRY_ACTION_NO_BLANK("Reboot RCM", rcm_cb,     NULL, false, &menu_more[1]),
		[1] = TUI_ENTRY_ACTION_NO_BLANK("UMS",        ums_cb,     NULL, false, &menu_more[2]),
		[2] = TUI_ENTRY_ACTION_NO_BLANK("Retry",      retry_cb,   NULL, false, &menu_more[3]),
		[3] = TUI_ENTRY_ACTION_NO_BLANK("Toolbox",    toolbox_cb,  NULL, false, &menu_more[4]),
		[4] = TUI_ENTRY_BACK(NULL),
	};

	tui_entry_t menu_entries[] = {
		[0] = TUI_ENTRY_ACTION_NO_BLANK("Power  Off", power_off_cb, NULL, false, &menu_entries[1]),
		[1] = TUI_ENTRY_ACTION_NO_BLANK("Reboot OFW", ofw_cb,       NULL, false, &menu_entries[2]),
		[2] = TUI_ENTRY_MENU_EX        ("More", false, menu.pos_x, menu.pos_y, menu.pad, menu.colors, menu.width, 5, 0, t210 ? &menu_more[0] : &menu_more[1], false, NULL),
	};

	menu.entries = menu_entries;

	tui_menu_start_rot(&menu);
}

static void low_battery_shutdown(){
	u8 intr = i2c_recv_byte(I2C_5, MAX77620_I2C_ADDR, MAX77620_REG_IRQTOP);

	if(intr & MAX77620_IRQ_TOP_GLBL){
		/* battery too low */
		power_set_state(POWER_OFF);
	}
}

static void get_cfg(){
	emmc_initialize(false);
	modchip_get_cfg_or_default(&sdloader_cfg);
	emmc_end();
}

void main(){
	modchip_confirm_execution();
	low_battery_shutdown();

	bpmp_clk_rate_set(is_t210() ? BPMP_CLK_LOWER_BOOST : BPMP_CLK_DEFAULT_BOOST);

	get_cfg();

	// u8 btn = btn_read_vol();

	// boot straight to ofw if vol+/vol- held
	// if(btn & BTN_VOL_DOWN && btn & BTN_VOL_UP){
	// 	power_set_state(REBOOT_BYPASS_FUSES);
	// }


	// if(!(btn & BTN_VOL_UP)){
	// 	try_launch_payload();
	// }else{
		handle_sdloader_status(SD_LOADER_FORCE_MENU, 0);
	// }

	bq24193_enable_charger();

	do_menu();

	gfx_clear_color(COL_BLACK);
}