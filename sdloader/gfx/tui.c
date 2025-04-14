#include "./tui.h"

#include <gfx.h>
#include <gfx_utils.h>
#include <soc/timer.h>
#include <string.h>
#include <utils/btn.h>
#include <power/max17050.h>
#include <power/bq24193.h>

static bool tui_entry_is_selectable(tui_entry_t *entry){
	if(entry->disabled){
		return false;
	}
	switch(entry->type){
	case TUI_ENTRY_TYPE_MENU:
	case TUI_ENTRY_TYPE_BACK:
	case TUI_ENTRY_TYPE_ACTION:
	case TUI_ENTRY_TYPE_ACTION_MODIFYING:
	case TUI_ENTRY_TYPE_ACTION_NO_BLANK:
	case TUI_ENTRY_TYPE_ACTION_MODIFYING_NO_BLANK:
		return true;
	default:
		return false;
	}
}

void tui_print_battery_icon(bool force){
	static u32 last_update = 0;
	static u8 cnt = 0;

	if(!force){
		if(get_tmr_ms() - last_update < 1000){
			return;
		}
	}

	last_update = get_tmr_ms();

	u32 current_charge_status;
	bq24193_get_property(BQ24193_ChargeStatus, (int*)&current_charge_status);

	u32 bat_percent;
	max17050_get_property(MAX17050_RepSOC, (int *)&bat_percent);
	bat_percent = (bat_percent >> 8) & 0xff;

	u8 col = COL_DARK_GREEN;
	u8 level = (bat_percent + 10) / 10;
	level = level > 10 ? 10 : level;

	u8 width = level;
	if(current_charge_status){
		width = cnt;
		cnt++;
		cnt %= level + 1;
	}

	if(level < 7){
		col = COL_ORANGE;
	}
	if(level < 3){
		col = COL_DARK_RED;
	}

	gfx_clear_rect_rot(COL_GREY, 2, 2, 12, 6);
	gfx_clear_rect_rot(COL_BLACK, 3, 3, 10, 4);
	gfx_clear_rect_rot(col, 3, 3, width, 4);
	gfx_clear_rect_rot(COL_GREY, 14, 3, 1, 4);
}

tui_status_t tui_menu_start(tui_entry_menu_t *menu){
	u32 ox, oy;
	gfx_con_get_origin(&ox, &oy);
	gfx_con_set_origin(menu->pos_x, menu->pos_y);

	tui_entry_t *selected = NULL;
	for(tui_entry_t *current = menu->entries; current != NULL; current = current->next){
		if(tui_entry_is_selectable(current)){
			selected = current;
			break;
		}
	}

	if(!selected){
		return(TUI_ERR_NO_SELECTABLE_ENTRY);
	}

	gfx_clear_color(TUI_COLOR_SCHEME_DEFAULT.bg);

	while(true){

		gfx_con_setpos(menu->pos_x, menu->pos_y);
		gfx_con_setcol(TUI_COLOR_SCHEME_DEFAULT.fg, true, TUI_COLOR_SCHEME_DEFAULT.bg);
		gfx_printf("%s\n\n", menu->title.text);
		for(tui_entry_t *current = menu->entries; current != NULL; current = current->next){
			const char *title;
			switch(current->type){
				case TUI_ENTRY_TYPE_MENU:
				case TUI_ENTRY_TYPE_ACTION:
				case TUI_ENTRY_TYPE_ACTION_MODIFYING:
				case TUI_ENTRY_TYPE_TEXT:
				case TUI_ENTRY_TYPE_ACTION_NO_BLANK:
				case TUI_ENTRY_TYPE_ACTION_MODIFYING_NO_BLANK:
					title = current->title.text;
					break;
				case TUI_ENTRY_TYPE_BACK:
					title = "Back";
					break;
			}

			switch(current->type){
			case TUI_ENTRY_TYPE_ACTION:
			case TUI_ENTRY_TYPE_ACTION_MODIFYING:
			case TUI_ENTRY_TYPE_ACTION_NO_BLANK:
			case TUI_ENTRY_TYPE_ACTION_MODIFYING_NO_BLANK:
				u32 x, y;
				gfx_con_getpos_rot(&x, &y);
				current->action.y_pos = y;
				current->action.x_pos = x;
				break;
			default:
				break;
			}

			if(current == selected){
				if(current->disabled){
					gfx_con_setcol(TUI_COLOR_SCHEME_DEFAULT.fg_active_disabled, true, TUI_COLOR_SCHEME_DEFAULT.bg_active_disabled);
				}else{
					gfx_con_setcol(TUI_COLOR_SCHEME_DEFAULT.fg_active, true, TUI_COLOR_SCHEME_DEFAULT.bg_active);
				}
			}else{
				if(current->disabled){
					gfx_con_setcol(TUI_COLOR_SCHEME_DEFAULT.fg_disabled, true, TUI_COLOR_SCHEME_DEFAULT.bg_disabled);
				}else{
					gfx_con_setcol(TUI_COLOR_SCHEME_DEFAULT.fg_active_disabled, true, TUI_COLOR_SCHEME_DEFAULT.bg_active_disabled);
				}
			}

			gfx_printf("%s\n", title);
		}

		u8 btn = btn_wait_timeout_single1(1000);

		if(btn & BTN_VOL_UP){
			tui_entry_t *next_selected = selected;
			for(tui_entry_t *current = menu->entries; current != selected; current = current->next){
				if(tui_entry_is_selectable(current)){
					next_selected = current;
				}
			}
			selected = next_selected;
		}else if(btn & BTN_VOL_DOWN){
			for(tui_entry_t *next_selected = selected->next; next_selected != NULL; next_selected = next_selected->next){
				if(tui_entry_is_selectable(next_selected)){
					selected = next_selected;
					break;
				}
			}
		}else if(btn & BTN_POWER){
			switch(selected->type){
				case TUI_ENTRY_TYPE_MENU:
					tui_menu_start_rot(&selected->menu);
					break;
				case TUI_ENTRY_TYPE_ACTION_NO_BLANK:
					if(!selected->disabled){
						selected->action.cb(selected->action.data);
					}
					break;
				case TUI_ENTRY_TYPE_ACTION:
					if(!selected->disabled){
						selected->action.cb(selected->action.data);
						gfx_clear_color(TUI_COLOR_SCHEME_DEFAULT.bg);
					}
					break;
				case TUI_ENTRY_TYPE_ACTION_MODIFYING_NO_BLANK:
					if(!selected->disabled){
						selected->action_modifying.cb(selected->action.data, (tui_entry_t*)&selected, menu);
					}
					break;
				case TUI_ENTRY_TYPE_ACTION_MODIFYING:
					if(!selected->disabled){
						selected->action_modifying.cb(selected->action.data, (tui_entry_t*)&selected, menu);
						gfx_clear_color(TUI_COLOR_SCHEME_DEFAULT.bg);
					}
					break;
				case TUI_ENTRY_TYPE_BACK:
					gfx_con_set_origin(ox, oy);
					return(TUI_SUCCESS);
				case TUI_ENTRY_TYPE_TEXT:
				default:
					break;
			}
		}
	}
}

void tui_menu_clear_screen(tui_entry_menu_t *menu){
	gfx_clear_rect_rot(menu->colors->bg, menu->pos_x, menu->pos_y, menu->width * 8, menu->height * 8);
}

void tui_print_menu(tui_entry_menu_t *menu){
	const tui_color_scheme_t *colors = menu->colors;
	u32 ox, oy;
	u32 _x, y;
	gfx_con_get_origin_rot(&ox, &oy);
	gfx_con_set_origin_rot(menu->pos_x, menu->pos_y);

	gfx_con_setpos_rot(menu->pos_x, menu->pos_y);
	gfx_con_setcol(colors->fg_title, true, colors->bg);
	if(menu->title.text && menu->show_title){
		gfx_printf_rot("%s\n", menu->title.text);
	}
	gfx_con_setcol(colors->fg, true, colors->bg);

	for(tui_entry_t *current = menu->entries; current != NULL; current = current->next){
		const char *title = NULL;
		switch(current->type){
			case TUI_ENTRY_TYPE_MENU:
			case TUI_ENTRY_TYPE_ACTION:
			case TUI_ENTRY_TYPE_ACTION_MODIFYING:
			case TUI_ENTRY_TYPE_TEXT:
			case TUI_ENTRY_TYPE_ACTION_NO_BLANK:
			case TUI_ENTRY_TYPE_ACTION_MODIFYING_NO_BLANK:
				title = current->title.text;
				break;
			case TUI_ENTRY_TYPE_BACK:
				title = "Back";
				break;
		}

		switch(current->type){
		case TUI_ENTRY_TYPE_ACTION:
		case TUI_ENTRY_TYPE_ACTION_MODIFYING:
		case TUI_ENTRY_TYPE_ACTION_NO_BLANK:
		case TUI_ENTRY_TYPE_ACTION_MODIFYING_NO_BLANK:
			gfx_con_getpos_rot(&_x, &y);
			current->action.y_pos = y;
			current->action.x_pos = menu->pos_x;
			break;
		default:
			break;
		}

		if(current == menu->selected){
			if(current->disabled){
				gfx_con_setcol(colors->fg_active_disabled, true, colors->bg_active_disabled);
			}else{
				gfx_con_setcol(colors->fg_active, true, colors->bg_active);
			}
		}else{
			if(current->disabled){
				gfx_con_setcol(colors->fg_disabled, true, colors->bg_disabled);
			}else{
				gfx_con_setcol(colors->fg, true, colors->bg);
			}
		}

		if(title){
			gfx_printf_rot("%s", title);
			for(u8 i = strlen(title); i < menu->pad; i++){
				gfx_putc_rot(' ');
			}
			gfx_putc_rot('\n');
		}
	}

	gfx_con_set_origin_rot(ox, oy);
}

tui_status_t tui_menu_start_rot(tui_entry_menu_t *menu){
	tui_print_battery_icon(true);

	u32 time = get_tmr_ms();

	menu->selected = NULL;
	for(tui_entry_t *current = menu->entries; current != NULL; current = current->next){
		if(tui_entry_is_selectable(current)){
			menu->selected = current;
			break;
		}
	}

	if(!menu->selected){
		return(TUI_ERR_NO_SELECTABLE_ENTRY);
	}

	tui_menu_clear_screen(menu);

	while(true){
		tui_print_menu(menu);

		u8 btn = btn_wait_timeout_single1(1000);

		if(!btn){
			if(menu->timeout_ms){
				if(get_tmr_ms() - time > menu->timeout_ms){
					tui_menu_clear_screen(menu);
					return TUI_TIMEOUT;
				}
			}
		}

		tui_print_battery_icon(false);

		if(btn & BTN_VOL_UP){
			tui_entry_t *next_selected = menu->selected;
			for(tui_entry_t *current = menu->entries; current != menu->selected; current = current->next){
				if(tui_entry_is_selectable(current)){
					next_selected = current;
				}
			}
			menu->selected = next_selected;
		}else if(btn & BTN_VOL_DOWN){
			for(tui_entry_t *next_selected = menu->selected->next; next_selected != NULL; next_selected = next_selected->next){
				if(tui_entry_is_selectable(next_selected)){
					menu->selected = next_selected;
					break;
				}
			}
		}else if(btn & BTN_POWER){
			switch(menu->selected->type){
				case TUI_ENTRY_TYPE_MENU:
					tui_menu_start_rot(&menu->selected->menu);
					break;
				case TUI_ENTRY_TYPE_ACTION_NO_BLANK:
					if(!menu->selected->disabled){
						menu->selected->action.cb(menu->selected->action.data);
					}
					break;
				case TUI_ENTRY_TYPE_ACTION:
					if(!menu->selected->disabled){
						menu->selected->action.cb(menu->selected->action.data);
						gfx_clear_color(TUI_COLOR_SCHEME_DEFAULT.bg);
					}
					break;
				case TUI_ENTRY_TYPE_ACTION_MODIFYING_NO_BLANK:
					if(!menu->selected->disabled){
						menu->selected->action_modifying.cb(menu->selected->action.data, menu->selected, menu);
					}
					break;
				case TUI_ENTRY_TYPE_ACTION_MODIFYING:
					if(!menu->selected->disabled){
						menu->selected->action_modifying.cb(menu->selected->action.data, menu->selected, menu);
						gfx_clear_color(TUI_COLOR_SCHEME_DEFAULT.bg);
					}
					break;
				case TUI_ENTRY_TYPE_BACK:
					tui_menu_clear_screen(menu);
					return(TUI_SUCCESS);
				case TUI_ENTRY_TYPE_TEXT:
				default:
					break;
			}
		}

		if(btn){
			time = get_tmr_ms();
		}
	}
}

void tui_clear_status(){
	gfx_clear_rect_rot(TUI_COLOR_SCHEME_DEFAULT.bg, 0, TUI_STATUS_POS_Y, gfx_ctxt.height, 8);
}

void tui_print_status(u8 col_fg, const char *fmt){
	gfx_con_setpos_rot(0, TUI_STATUS_POS_Y);
	tui_clear_status();
	gfx_con_setcol(col_fg, true, TUI_COLOR_SCHEME_DEFAULT.bg);
	gfx_print_centered_rot(fmt);
}
