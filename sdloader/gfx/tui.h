#ifndef _TUI_H_
#define _TUI_H_

#include <utils/types.h>
#include <gfx.h>

#define TUI_STATUS_POS_Y 72

#define TUI_ENTRY_TEXT(_text, _next) \
	{                                \
		.type = TUI_ENTRY_TYPE_TEXT, \
		.disabled = false,           \
		.next = (_next),             \
		.text = {                    \
			.title = {               \
				.text = (_text)      \
			}                        \
		}                            \
	}

#define TUI_ENTRY_TEXT_DISABLED(_text, _next) \
	{                                         \
		.type = TUI_ENTRY_TYPE_TEXT,          \
		.disabled = true,                     \
		.next = (_next),                      \
		.text = {                             \
			.title = {                        \
				.text = (_text)               \
			}                                 \
		}                                     \
	}

#define TUI_ENTRY_MENU(_title, _menu_entries, _disabled, _next) \
	{                                   \
		.type = TUI_ENTRY_TYPE_MENU,    \
		.disabled = (_disabled),        \
		.next = (_next),                \
		.menu = {                       \
			.title = {                  \
				.text = (_title)        \
			},                          \
			.entries = (_menu_entries), \
			.pos_x   = 0,               \
			.pos_y   = 0,               \
			.pad     = 0,               \
			.colors  = &TUI_COLOR_SCHEME_DEFAULT \
			.timeout_ms = 0,            \
			.width = 0,                 \
			.height = 0,                \
			.timeout_ms = 0,            \
			.show_title = false         \
		}                               \
	}

#define TUI_ENTRY_MENU_EX(_title, _show_title, _posx, _posy, _pad, _colors, _width, _height, _timeout, _menu_entries, _disabled, _next) \
	{                                   \
		.type = TUI_ENTRY_TYPE_MENU,    \
		.disabled = (_disabled),        \
		.next = (_next),                \
		.menu = {                       \
			.title = {                  \
				.text = (_title)        \
			},                          \
			.entries = (_menu_entries), \
			.pos_x   = (_posx),         \
			.pos_y   = (_posy),         \
			.pad     = (_pad),          \
			.colors  = (_colors),       \
			.width   = (_width),        \
			.height  = (_height),       \
			.timeout_ms = (_timeout),   \
			.show_title = (_show_title) \
		}                               \
	}

#define TUI_ENTRY_ACTION(_title, _cb, _data, _disabled, _next) \
	{                                  \
		.type = TUI_ENTRY_TYPE_ACTION, \
		.disabled = (_disabled),       \
		.next = (_next),               \
		.action = {                    \
			.title = {                 \
				.text = (_title)       \
			},                         \
			.data = (_data),           \
			.cb = (_cb)                \
		}                              \
	}

#define TUI_ENTRY_ACTION_MODIFYING(_title, _cb, _data, _disabled, _next) \
	{                                            \
		.type = TUI_ENTRY_TYPE_ACTION_MODIFYING, \
		.disabled = (_disabled),                 \
		.next = (_next),                         \
		.action_modifying = {                    \
			.title = {                           \
				.text = (_title)                 \
			},                                   \
			.data = (_data),                     \
			.cb = (_cb)                          \
		}                                        \
	}

#define TUI_ENTRY_ACTION_NO_BLANK(_title, _cb, _data, _disabled, _next)  \
	{                                           \
		.type = TUI_ENTRY_TYPE_ACTION_NO_BLANK, \
		.disabled = (_disabled),                \
		.next = (_next),                        \
		.action = {                             \
			.title = {                          \
				.text = (_title)                \
			},                                  \
			.data = (_data),                    \
			.cb = (_cb)                         \
		}                                       \
	}

#define TUI_ENTRY_ACTION_MODIFYING_NO_BLANK(_title, _cb, _data, _disabled, _next)  \
	{                                                     \
		.type = TUI_ENTRY_TYPE_ACTION_MODIFYING_NO_BLANK, \
		.disabled = (_disabled),                          \
		.next = (_next),                                  \
		.action_modifying = {                             \
			.title = {                                    \
				.text = (_title)                          \
			},                                            \
			.data = (_data),                              \
			.cb = (_cb)                                   \
		}                                                 \
	}

#define TUI_ENTRY_BACK(_next) \
	{                                \
		.type = TUI_ENTRY_TYPE_BACK, \
		.disabled = false,           \
		.next = (_next)              \
	}

typedef struct tui_color_scheme_t{
	u8 bg;
	u8 fg;
	u8 bg_active;
	u8 fg_active;
	u8 bg_active_disabled;
	u8 fg_active_disabled;
	u8 bg_disabled;
	u8 fg_disabled;
	u8 fg_title;
}tui_color_scheme_t;

static const tui_color_scheme_t TUI_COLOR_SCHEME_DEFAULT = {
	.bg                 = COL_BLACK,
	.fg                 = COL_WHITE,
	.bg_active          = COL_GREY,
	.fg_active          = COL_WHITE,
	.bg_disabled        = COL_BLACK,
	.fg_disabled        = COL_GREY,
	.bg_active_disabled = COL_DARK_GREY,
	.fg_active_disabled = COL_GREY,
	.fg_title           = COL_WHITE,
};

static const tui_color_scheme_t TUI_COLOR_SCHEME_SHADOW = {
	.bg                 = COL_BLACK,
	.fg                 = COL_GREY,
	.bg_active          = COL_DARK_GREY,
	.fg_active          = COL_GREY,
	.bg_disabled        = COL_BLACK,
	.fg_disabled        = COL_DARK_GREY,
	.bg_active_disabled = COL_DARK_DARK_GREY,
	.fg_active_disabled = COL_DARK_GREY,
	.fg_title           = COL_LIGHT_GREY,
};
	
typedef struct tui_entry_t tui_entry_t;
typedef struct tui_entry_menu_t tui_entry_menu_t;

typedef void(*tui_action_cb_t)(void*);
typedef void(*tui_action_modifying_cb_t)(void*, tui_entry_t*, tui_entry_menu_t*);

typedef enum tui_entry_type_t{
	TUI_ENTRY_TYPE_MENU,
	TUI_ENTRY_TYPE_TEXT,
	TUI_ENTRY_TYPE_ACTION,
	TUI_ENTRY_TYPE_ACTION_MODIFYING,
	TUI_ENTRY_TYPE_ACTION_NO_BLANK,
	TUI_ENTRY_TYPE_ACTION_MODIFYING_NO_BLANK,
	TUI_ENTRY_TYPE_BACK
}tui_entry_type_t;

typedef enum tui_status_t{
	TUI_SUCCESS,
	TUI_ERR_NO_SELECTABLE_ENTRY,
	TUI_TIMEOUT,
}tui_status_t;

typedef struct tui_text_t{
	const char *text;
}tui_text_t;

typedef struct tui_entry_menu_t{
	tui_text_t title;
	tui_entry_t *entries;
	tui_entry_t *selected;
	const tui_color_scheme_t *colors;
	u8 pos_x;
	u8 pos_y;
	u8 width;
	u8 height;
	u8 pad;
	u32 timeout_ms;
	bool show_title;
}tui_entry_menu_t;

typedef struct tui_entry_text_t{
	tui_text_t title;
}tui_entry_text_t;

typedef struct tui_entry_action_t{
	tui_text_t title;
	void *data;
	tui_action_cb_t cb;
	u8 y_pos;
	u8 x_pos;
}tui_entry_action_t;

typedef struct tui_entry_action_modifying_t{
	tui_text_t title;
	void *data;
	tui_action_modifying_cb_t cb;
	u8 y_pos;
	u8 x_pos;
}tui_entry_action_modifying_t;

struct tui_entry_t{
	tui_entry_type_t type;
	bool disabled;
	struct tui_entry_t *next;
	union{
		tui_text_t title;
		union{
			tui_entry_menu_t menu;
			tui_entry_text_t text;
			tui_entry_action_t action;
			tui_entry_action_modifying_t action_modifying;
		};
	};
};



tui_status_t tui_menu_start(tui_entry_menu_t *menu);
tui_status_t tui_menu_start_rot(tui_entry_menu_t *menu);

void tui_menu_clear_screen(tui_entry_menu_t *menu);
void tui_print_menu(tui_entry_menu_t *menu);
void tui_print_battery_icon(bool force);

void tui_print_status(u8 col_fg, const char *fmt);
#endif