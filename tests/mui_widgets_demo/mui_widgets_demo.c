/*
 * ui_tests.c
 *
 * Copyright (C) 2023 Michel Pollet <buserror@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 */
#define _GNU_SOURCE // for asprintf
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>

#include "mui.h"
#include "mui_shell_plugin.h"
#include "c2_geometry.h"

typedef struct cg_ui_t {
	mui_t *ui;
} cg_ui_t;


static void
_test_show_about(
		cg_ui_t *g)
{
	mui_window_t *w = mui_window_get_by_id(g->ui, FCC('a','b','o','t'));
	if (w) {
		mui_window_select(w);
		return;
	}
	w = mui_alert(g->ui, C2_PT(0,0),
					"About MUI",
					"Version " MUI_VERSION "\n"
					"Build " __DATE__ " " __TIME__,
					MUI_ALERT_INFO);
	mui_window_set_id(w, FCC('a','b','o','t'));
}


static void
_test_textedit_demo(
		mui_t *mui)
{
	mui_window_t *w = mui_window_get_by_id(mui, FCC('t','e','s','t'));
	if (w) {
		mui_window_select(w);
		return;
	}
	c2_pt_t where = {};
	c2_rect_t wpos = C2_RECT_WH(where.x, where.y, 510, 270);
	if (where.x == 0 && where.y == 0)
		c2_rect_offset(&wpos,
			(mui->screen_size.x / 2) - (c2_rect_width(&wpos) / 2),
			(mui->screen_size.y * 0.45) - (c2_rect_height(&wpos) / 2));
	w = mui_window_create(mui, wpos, NULL, MUI_WINDOW_LAYER_NORMAL,
					"This is VERY Work in Progress", 0);
	mui_window_set_id(w, FCC('t','e','s','t'));

	mui_control_t * c = NULL;
	c2_rect_t cf;

	cf = C2_RECT_WH(10, 10, 480, 170);
	c = mui_textedit_control_new(w, cf,
			MUI_CONTROL_TEXTBOX_FRAME | MUI_CONTROL_TEXTEDIT_VERTICAL);
	mui_textedit_set_text(c,
		"The quick brown fox Jumps over the Lazy dog.\n"
		"Lorem Ipsum is simply dummy text of the printing "
		"and typesetting industry. Lorem Ipsum has been the "
		"industry's standard dummy text ever since the 1500s.\n"
		#if 1
		"Now let's step back and look at what's happening. "
		"Writing to the disk is a load and shift process, a "
		"little like HIRES pattern outputs but much slower.\n"
		"Also, the MPU takes a very active role in the loading "
		"and shifting of disk write data. There are two 8-state "
		"loops in the WRITE sequence. After initializing the "
		"WRITE sequence, data is stored in the data register "
		"at a critical point in the A7' loop. As (quickly "
		"thereafter as the 6502 can do it, the sequencer is "
		"configured to shift left at the critical point "
		"instead of loading."
		#endif
		);
	c2_rect_bottom_of(&cf, cf.b, 10);
	cf.b = cf.t + 35;
	c = mui_textedit_control_new(w, cf, MUI_CONTROL_TEXTBOX_FRAME);
	mui_textedit_set_text(c,
		"Fulling Mill Online Return Center.pdf");

}

static void
_test_static_text_and_boxes(
		struct mui_t *ui)
{
	mui_window_t *w = mui_window_get_by_id(ui, FCC('s','t','x','t'));
	if (w) {
		mui_window_select(w);
		return;
	}
	c2_pt_t where = {};
	c2_rect_t wpos = C2_RECT_WH(where.x, where.y, 566, 320);
	if (where.x == 0 && where.y == 0)
		c2_rect_offset(&wpos,
			(ui->screen_size.x / 2) - (c2_rect_width(&wpos) / 2),
			(ui->screen_size.y * 0.45) - (c2_rect_height(&wpos) / 2));
	w = mui_window_create(ui, wpos, NULL, MUI_WINDOW_LAYER_NORMAL,
					"Static Text and Boxes", 0);
	mui_window_set_id(w, FCC('s','t','x','t'));

	float base_size = mui_font_find(ui, "main")->size;
	float margin = base_size * 0.7;
	mui_control_t * c = NULL;
	c2_rect_t cf, cb;
	cf = C2_RECT_WH(margin, margin/2, 250, 3 + base_size * 4);

	c = mui_groupbox_new(w, cf, "Justification: ", 0);
	cb = cf;
	cb.t += base_size;
	cb.b = cb.t + base_size;
	cb.l += margin / 3;
	cb.r = cf.r - (margin / 3);
	uint16_t debug_frames = 0;//MUI_CONTROL_TEXTBOX_FRAME;
	c = mui_textbox_new(w, cb, "Left Aligned", NULL,
			MUI_TEXT_ALIGN_LEFT | debug_frames);
	cb = c2_rect_bottom_of(&cb, cb.b, 0);
	c = mui_textbox_new(w, cb, "Centered & Grayed", NULL,
			MUI_TEXT_ALIGN_CENTER | debug_frames);
	mui_control_set_state(c, MUI_CONTROL_STATE_DISABLED);
	cb = c2_rect_bottom_of(&cb, cb.b, 0);
	c = mui_textbox_new(w, cb, "Right Aligned", NULL,
			MUI_TEXT_ALIGN_RIGHT | debug_frames);
	c2_rect_t bottom_box = cf;
	bottom_box = c2_rect_bottom_of(&bottom_box, bottom_box.b, margin*1.5);

	c2_rect_t sr = cf;

	cf = c2_rect_right_of(&cf, cf.r, margin);
	// plonk in a separator to demo them
	sr.r = cf.r;
	sr.t = sr.b - 2;
	c2_rect_inset(&sr, 80, 0);
	sr = c2_rect_bottom_of(&sr, sr.b, margin);
	mui_separator_new(w, sr);

	c = mui_groupbox_new(w, cf, "Style: ", MUI_TEXT_ALIGN_RIGHT);
	cb = cf;
	cb.t += base_size;
	cb.b = cb.t + base_size;
	cb.l += margin / 3;
	cb.r = cf.r - (margin / 3);
	c = mui_textbox_new(w, cb, "Synthetic Bold", NULL,
				MUI_TEXT_ALIGN_CENTER | MUI_TEXT_STYLE_BOLD);
	cb = c2_rect_bottom_of(&cb, cb.b, 0);
	c = mui_textbox_new(w, cb, "With Narrow Spacing", NULL,
				MUI_TEXT_ALIGN_CENTER | MUI_TEXT_STYLE_NARROW);
	cb = c2_rect_bottom_of(&cb, cb.b, 0);
	c = mui_textbox_new(w, cb, "El Cheapo Underline", NULL,
				MUI_TEXT_ALIGN_CENTER | MUI_TEXT_STYLE_ULINE);
	cb = c2_rect_bottom_of(&cb, cb.b, 0);

	cf = bottom_box;
	c = mui_groupbox_new(w, cf, "Spacing: ", 0);
	cb = cf;
	cb.t += base_size;
	cb.b = cb.t + base_size*3;
	cb.l += margin / 3;
	cb.r = cb.l + (c2_rect_width(&cf) / 2) - (margin / 3);
	c = mui_textbox_new(w, cb, "Normal\nLine\nSpacing", NULL,
				MUI_TEXT_ALIGN_CENTER | 0 | debug_frames);
	cb = c2_rect_right_of(&cb, cb.r, margin/3);
	c = mui_textbox_new(w, cb, "Compact\nLine\nSpacing", NULL,
				MUI_TEXT_ALIGN_CENTER | MUI_TEXT_ALIGN_COMPACT | debug_frames);

	cf = c2_rect_right_of(&cf, cf.r, margin);
	c = mui_groupbox_new(w, cf, "Justification: ", 0);
	cb = cf;
	cb.t += base_size;
	cb.b = cb.t + base_size * 3;
	cb.l += margin / 3;
	cb.r = cf.r - (margin / 3);
	c = mui_textbox_new(w, cb,
				"This quick brown fox is both Narrow, "
				"and Fully Justified these days.", NULL,
				MUI_TEXT_STYLE_NARROW | MUI_TEXT_ALIGN_FULL | debug_frames);
}

/*
 * This demos most of the controls, buttons, radios, checkboxes, listbox
 * and a few other things.
 */
static void
_test_demo_all_controls(
	struct mui_t *ui)
{
	mui_window_t *w = mui_window_get_by_id(ui, FCC('d','e','m','o'));
	if (w) {
		mui_window_select(w);
		return;
	}
	c2_pt_t where = {};
	c2_rect_t wpos = C2_RECT_WH(where.x, where.y, 620, 380);
	if (where.x == 0 && where.y == 0)
		c2_rect_offset(&wpos,
			(ui->screen_size.x / 2) - (c2_rect_width(&wpos) / 2),
			(ui->screen_size.y * 0.45) - (c2_rect_height(&wpos) / 2));
	w = mui_window_create(ui, wpos, NULL, MUI_WINDOW_LAYER_NORMAL,
					"Control Demo", 0);
	mui_window_set_id(w, FCC('d','e','m','o'));

	float base_size = mui_font_find(ui, "main")->size;
	float margin = base_size * 0.7;
	mui_control_t * c = NULL;
	c2_rect_t cf;
	cf = C2_RECT_WH(0, 0, base_size * 5, base_size * 1.4);
	c2_rect_left_of(&cf, c2_rect_width(&w->content), margin);
	c2_rect_top_of(&cf, c2_rect_height(&w->content), margin);

	c = mui_button_new(w,
					cf, MUI_BUTTON_STYLE_DEFAULT,
					"Default", 0);
	c->key_equ = MUI_KEY_EQU(0, 13); // return
	cf = c2_rect_left_of(&cf, cf.l, margin);
	c = mui_button_new(w,
					cf, MUI_BUTTON_STYLE_NORMAL,
					"Normal", 0);
	c->key_equ = MUI_KEY_EQU(0, 32); // space
	cf = c2_rect_left_of(&cf, cf.l, margin);
	c = mui_button_new(w,
					cf, MUI_BUTTON_STYLE_NORMAL,
					"Disabled", 0);
	mui_control_set_state(c, MUI_CONTROL_STATE_DISABLED);

	/* Radio can have a mask, if so, they each UID that matches the
	 * mask will switch like radio buttons ought to, without having to
	 * keep track of which are on or off */
	uint32_t radio_uid = FCC('r','a','d','0');
	cf = C2_RECT_WH(margin, margin, base_size * 7, base_size * 1.4);
	c = mui_button_new(w, cf, MUI_BUTTON_STYLE_RADIO,
					"Radio Button", FCC_INDEXED(radio_uid, 0));
	c->uid_mask = FCC_MASK;
	mui_control_set_value(c, 1);
	cf = c2_rect_bottom_of(&cf, cf.b, 0);
	c = mui_button_new(w, cf, MUI_BUTTON_STYLE_RADIO,
					"Other Radio", FCC_INDEXED(radio_uid, 1));
	mui_control_set_state(c, MUI_CONTROL_STATE_DISABLED);
	c->uid_mask = FCC_MASK;
	cf = c2_rect_bottom_of(&cf, cf.b, 0);
	c = mui_button_new(w, cf, MUI_BUTTON_STYLE_RADIO,
					"Third Choice", FCC_INDEXED(radio_uid, 2));
	c->uid_mask = FCC_MASK;
	c2_rect_t first_column = cf;

	cf = c2_rect_right_of(&cf, cf.r, margin);
	cf.r = cf.l + base_size * 6;
	c2_rect_offset(&cf, 0, -cf.t + margin);
	c = mui_button_new(w,
					cf, MUI_BUTTON_STYLE_CHECKBOX,
					"Checkbox", 0);
	cf = c2_rect_bottom_of(&cf, cf.b, margin/2);
	c = mui_button_new(w,
					cf, MUI_BUTTON_STYLE_CHECKBOX,
					"Another Checkbox", 0);
	mui_control_set_value(c, 1);
	c2_rect_t second_column = cf;

	cf = first_column;
	cf = c2_rect_bottom_of(&cf, cf.b, margin);
	c2_rect_t lr = cf;
	lr.r = lr.l + base_size * 3;
	c = mui_textbox_new(w, lr, "Popup:", NULL, MUI_TEXT_ALIGN_LEFT);
	cf = c2_rect_right_of(&cf, lr.r, 0);
	//c2_rect_offset(&cf, 0, -cf.t + margin);
	cf.b = cf.t + 34;
	c = mui_popupmenu_new(w, cf, "Popup", 0, MUI_TEXT_ALIGN_LEFT);
	mui_menu_items_t *items = mui_popupmenu_get_items(c);
	mui_menu_items_add(items, (mui_menu_item_t){.title="1200", .uid=1200 });
	mui_menu_items_add(items, (mui_menu_item_t){.title="2400", .uid=2400 });
	mui_menu_items_add(items, (mui_menu_item_t){.title="4800", .uid=4800 });
	mui_menu_items_add(items, (mui_menu_item_t){.title="9600", .uid=9600 });
	mui_menu_items_add(items, (mui_menu_item_t){.title="19200", .uid=19200 });
	// popup needs to be NULL terminated, AND prepared()
	mui_menu_items_add(items, (mui_menu_item_t){.title=NULL });
	mui_popupmenu_prepare(c);

	lr = c2_rect_bottom_of(&lr, lr.b, margin/4);
	lr.r = lr.l + base_size * 3;
	c = mui_textbox_new(w, lr, "Icons:", NULL, MUI_TEXT_ALIGN_LEFT);

	cf = c2_rect_bottom_of(&cf, cf.b, margin/4);
	//c2_rect_offset(&cf, 0, -cf.t + margin);
	cf.b = cf.t + 34;
	c = mui_popupmenu_new(w, cf, "Popup", 0, MUI_TEXT_ALIGN_LEFT);
	items = mui_popupmenu_get_items(c);
	mui_menu_items_add(items,
			(mui_menu_item_t){.title="Icon", .uid=1200, .icon=MUI_ICON_HARDDISK });
	mui_menu_items_add(items,
			(mui_menu_item_t){.title="Others", .uid=2400, .icon=MUI_ICON_FOLDER });
	// popup needs to be NULL terminated, AND prepared()
	mui_menu_items_add(items, (mui_menu_item_t){.title=NULL });
	mui_popupmenu_prepare(c);

	lr = c2_rect_bottom_of(&lr, lr.b, margin/4);
	lr.r = lr.l + base_size * 4.2;
	c = mui_textbox_new(w, lr, "Scrollbar:", NULL, MUI_TEXT_ALIGN_LEFT);
	c2_rect_right_of(&cf, lr.r, margin/4);
	cf = c2_rect_bottom_of(&cf, cf.b, margin/2);
	cf.b = cf.t + base_size;
	cf.r = cf.l + 200;
	c = mui_scrollbar_new(w, cf, 0, 5, 20);
	mui_scrollbar_set_max(c, 255);
//	mui_scrollbar_set_page(c, 10);

	cf = second_column;
	cf.b = cf.t + base_size;
	cf = c2_rect_right_of(&cf, cf.r, margin);
	c2_rect_offset(&cf, 0, -cf.t + margin);
	c = mui_textbox_new(w, cf, "Listbox:", NULL, 0);
	c2_rect_bottom_of(&cf, cf.b, 0);
	cf.b = cf.t + 6 * base_size;
	c = mui_listbox_new(w, cf, 0);
	mui_listbox_elems_t * elems = mui_listbox_get_elems(c);
	for (int i = 0; i < 25; i++) {
		mui_listbox_elem_t e = {
			.icon = MUI_ICON_FILE,
		};
		asprintf((char**)&e.elem, "Item %d", i);
		mui_listbox_elems_add(elems, e);
	}
	mui_listbox_prepare(c);
}

#define MUI_COLOR_APPLE_DEFINE
#include "mui_color_apple.h"

mui_menu_item_t m_color_apple_menu[] = {
	{ .color_icon = mui_color_apple, .is_menutitle = 1, },
	{ .title = "About MUI…",
			.uid = FCC('a','b','o','t') },
//	{ .title = "-", },
	{ },
};

mui_menu_item_t m_file_menu[] = {
	{ .title = "Quit",
			.uid = FCC('q','u','i','t'),
			.key_equ = MUI_KEY_EQU(MUI_MODIFIER_SUPER, 'Q'),
			.kcombo = MUI_GLYPH_COMMAND "Q" },
	{ },
};

mui_menu_item_t m_sub_menu1[] = {
	{ .title = "Hierarchical Menu",
			.uid = FCC('s','h','m','b'),
			.key_equ = MUI_KEY_EQU(MUI_MODIFIER_RCTRL, MUI_KEY_F1),
			.kcombo = MUI_GLYPH_CONTROL MUI_GLYPH_F1 },
	{ .title = "Disabled item",
			.disabled = 1,
			.key_equ = MUI_KEY_EQU(MUI_MODIFIER_RCTRL, MUI_KEY_F11),
			.kcombo = MUI_GLYPH_CONTROL MUI_GLYPH_F11 },
	{ .title = "-", },
	{ .title = "Tick One",
			.mark = MUI_GLYPH_TICK,
			.uid = FCC('v','d','c','0') },
	{ .title = "Tick Two",
			.uid = FCC('v','d','c','1') },
	{ },
};

mui_menu_item_t m_sub_menu2[] = {
	{ .title = "Other Sub Menu",
			.uid = FCC('s','h','m','b'),
			.key_equ = MUI_KEY_EQU(MUI_MODIFIER_RCTRL, MUI_KEY_F2),
			.kcombo = MUI_GLYPH_CONTROL MUI_GLYPH_F2 },
	{ .title = "Other Marked Item",
			.uid = FCC('s','h','m','b'),
			.mark = "!", },
	{ },
};

mui_menu_item_t m_menu_demo[] = {
	{ .title = MUI_GLYPH_OAPPLE "-Control-Reset",
			.uid = FCC('c','r','e','s'),
			.key_equ = MUI_KEY_EQU(MUI_MODIFIER_RCTRL|MUI_MODIFIER_RSHIFT, MUI_KEY_F12),
			.kcombo = MUI_GLYPH_CONTROL MUI_GLYPH_SHIFT MUI_GLYPH_F12 },
	{ .title = "Control-Reset",
			.uid = FCC('r','e','s','t'),
			.key_equ = MUI_KEY_EQU(MUI_MODIFIER_RCTRL, MUI_KEY_F12),
			.kcombo = MUI_GLYPH_CONTROL MUI_GLYPH_F12 },
	{ .title = "-", },
	{ .title = "Sub Menu",
			.kcombo = MUI_GLYPH_SUBMENU,
			.submenu = m_sub_menu1 },
	{ .title = "Other Sub Menu",
			.kcombo = MUI_GLYPH_SUBMENU,
			.submenu = m_sub_menu2 },
	{ },
};

mui_menu_item_t m_windows_menu[] = {
	{ .title = "Basic Alert…",
			.uid = FCC('a','l','e','r'), },
	{ .title = "Standard 'get' file…",
			.uid = FCC('s','t','d','g'), },
	{ .title = "Static Text…",
			.uid = FCC('s','t','x','t'), },
	{ .title = "Text Edit (WIP!)…",
			.uid = FCC('t','x','t','e'), },
	{ .title = "Demo All Controls…",
			.uid = FCC('d','e','m','o'), },
	{ },
};

/*
 * This is a 'window' action function that is called when the Alert window
 * is interacted with. It typically you will get an ACTION_SELECT with
 * ok or cancel.
 */
static int
_test_alert_action_cb(
		mui_window_t *win,
		void * cb_param,
		uint32_t what,
		void * param)
{
//	cg_ui_t *g = cb_param;
	printf("%s %4.4s\n", __func__, (char*)&what);
	switch (what) {
		case MUI_CONTROL_ACTION_SELECT: {
			mui_control_t * c = param;
			uint32_t uid = mui_control_get_uid(c);
			printf("%s Button %4.4s\n", __func__, (char*)&uid);
		}	break;
	}
	return 0;
}

static int
_test_stdfile_action_cb(
		mui_window_t *win,
		void * cb_param,
		uint32_t what,
		void * param)
{
//	cg_ui_t *g = cb_param;
	printf("%s %4.4s\n", __func__, (char*)&what);
	switch (what) {
		case MUI_CONTROL_ACTION_SELECT: {
			mui_control_t * c = param;
			uint32_t uid = mui_control_get_uid(c);
			printf("%s Button %4.4s\n", __func__, (char*)&uid);
		}	break;
		case MUI_STDF_ACTION_CANCEL: {
			printf("%s cancel was clicked\n", __func__);
			mui_window_dispose(win);
		}	break;
	}
	return 0;
}

int
_test_menubar_action(
		mui_window_t *win,
		void * cb_param,
		uint32_t what,
		void * param)
{
	cg_ui_t *g = cb_param;

//	printf("%s %4.4s\n", __func__, (char*)&what);

	switch (what) {
		case MUI_MENUBAR_ACTION_PREPARE: {
//			mui_menu_item_t * items = param;
		}	break;
		case MUI_MENUBAR_ACTION_SELECT: {
			mui_menu_item_t * item = param;
			printf("%s Selected %4.4s '%s'\n", __func__,
					(char*)&item->uid, item->title);
			switch (item->uid) {
				case FCC('a','b','o','t'): {
					_test_show_about(g);
				}	break;
				case FCC('q','u','i','t'): {
					printf("%s Quit\n", __func__);
					g->ui->quit_request = 1;
				}	break;
				case FCC('a','l','e','r'): {
					mui_window_t * w = mui_alert(g->ui, C2_PT(0,0),
							"Testing one Two",
							"Do you really want the printer to catch fire?\n"
							"This operation cannot be cancelled.",
							MUI_ALERT_WARN);
					mui_window_set_action(w, _test_alert_action_cb, g);
				}	break;
				case FCC('s','t','d','g'): {
					mui_window_t *w = mui_stdfile_get(g->ui,
								C2_PT(0, 0),
								"Select image for SmartPort card",
								"hdv,po,2mg",
								getenv("HOME"), 0);
					mui_window_set_action(w, _test_stdfile_action_cb, g);
				}	break;
				case FCC('t','x','t','e'):
					_test_textedit_demo(g->ui);
					break;
				case FCC('s','t','x','t'):
					_test_static_text_and_boxes(g->ui);
					break;
				case FCC('d','e','m','o'):
					_test_demo_all_controls(g->ui);
					break;
				default:
					printf("%s menu item %4.4s %s IGNORED\n",
							__func__, (char*)&item->uid, item->title);
					break;
			}
		}	break;
		default:
			printf("%s %4.4s IGNORED?\n", __func__, (char*)&what);
			break;
	}

	return 0;
}



static void *
_init(
		struct mui_t * ui,
		struct mui_plug_t * plug,
		mui_drawable_t * pix)
{
	mui_init(ui);
	ui->screen_size = pix->pix.size;
	asprintf(&ui->pref_directory, "%s/.local/share/mui", getenv("HOME"));

	cg_ui_t *g = calloc(1, sizeof(*g));
	g->ui = ui;
	printf("%s\n", __func__);
	mui_window_t * mbar = mui_menubar_new(ui);
	mui_window_set_action(mbar, _test_menubar_action, g);

	mui_menubar_add_menu(mbar, FCC('a','p','p','l'), m_color_apple_menu, 2);
	mui_menubar_add_simple(mbar, "File",
								FCC('f','i','l','e'),
								m_file_menu);
	mui_menubar_add_simple(mbar, "Menus",
								FCC('m','e','n','u'),
								m_menu_demo);
	mui_menubar_add_simple(mbar, "Windows",
								FCC('w','i','n','d'),
								m_windows_menu);
//	_test_textedit_demo(ui);
//	_test_static_text_and_boxes(ui);
	_test_demo_all_controls(ui);
	return g;
}

static void
_dispose(
		void *_ui)
{
	cg_ui_t *ui = _ui;
	printf("%s\n", __func__);
	mui_dispose(ui->ui);
	free(ui);
}

static int
_draw(
		struct mui_t *ui,
		void *param,
		mui_drawable_t *dr,
		uint16_t all)
{
	mui_draw(ui, dr, all);
	return 1;
}

static int
_event(
		struct mui_t *ui,
		void *param,
		mui_event_t *event)
{
	mui_handle_event(ui, event);
	return 0;
}

mui_plug_t mui_plug = {
	.name 		= "MUI Widgets Demo",
	.init 		= _init,
	.dispose 	= _dispose,
	.draw 		= _draw,
	.event 		= _event,
};