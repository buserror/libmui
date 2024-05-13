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

#define MII_MUI_MENUS_C
#include "mii_mui_menus.h"
#include "mii_mui_settings.h"

#ifndef MUI_VERSION
#define MUI_VERSION "0.0.0"
#endif

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

static mii_machine_config_t g_machine_conf = {};
static mii_loadbin_conf_t g_loadbin_conf = {};


int
_test_menubar_action(
		mui_window_t *win,
		void * cb_param,
		uint32_t what,
		void * param)
{
	cg_ui_t *g = cb_param;

	printf("%s %4.4s\n", __func__, (char*)&what);

	static int video_mode = 0;
	static int audio_mute = 0;
	switch (what) {
		case MUI_MENUBAR_ACTION_PREPARE: {
			mui_menu_item_t * items = param;
			if (items == m_video_menu) {
				printf("%s prepare video %d\n", __func__, video_mode);
				for (int i = 0; m_video_menu[i].title; i++) {
					switch (m_video_menu[i].uid) {
						case FCC('v','d','c','0'):
						case FCC('v','d','c','1'):
						case FCC('v','d','c','2'): {
							int idx = FCC_INDEX(m_video_menu[i].uid);
							if (video_mode == idx)
								strcpy(m_video_menu[i].mark, MUI_GLYPH_TICK);
							else
								m_video_menu[i].mark[0] = 0;
						}	break;
					}
				}
			} else if (items == m_audio_menu) {
				printf("%s prepare audio %d\n", __func__, audio_mute);
				for (int i = 0; m_audio_menu[i].title; i++) {
					switch (m_audio_menu[i].uid) {
						case FCC('a','u','d','0'):
							if (audio_mute)
								strcpy(m_audio_menu[i].mark, MUI_GLYPH_TICK);
							else
								m_audio_menu[i].mark[0] = 0;
							break;
					}
				}
			} else {
				printf("%s prepare (%s)\n", __func__, items[0].title);
			}
		}	break;
		case MUI_MENUBAR_ACTION_SELECT: {
			mui_menu_item_t * item = param;
			printf("%s Selected %4.4s '%s'\n", __func__,
					(char*)&item->uid, item->title);
			switch (item->uid) {
				case FCC('a','b','o','t'): {
//					_test_show_about(g);
					mii_mui_about(g->ui);
				}	break;
				case FCC('q','u','i','t'): {
					printf("%s Quit\n", __func__);
					g->ui->quit_request = 1;
				}	break;
				case FCC('s','l','o','t'): {
					mii_mui_configure_slots(g->ui, &g_machine_conf);
				}	break;
				case FCC('l', 'r', 'u', 'n'): {
					mii_mui_load_binary(g->ui, &g_loadbin_conf);
				}	break;
				case FCC('a','u','d','0'):
					audio_mute = !audio_mute;
					break;
				case FCC('v','d','C','l'): {
//					printf("%s Cycle video\n", __func__);
					video_mode = (video_mode + 1) % 3;
				}	break;
				case FCC('v','d','c','0'):
				case FCC('v','d','c','1'):
				case FCC('v','d','c','2'):
					video_mode = FCC_INDEX(item->uid);
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
	asprintf(&ui->pref_directory, "%s/.local/share/mii", getenv("HOME"));

	cg_ui_t *g = calloc(1, sizeof(*g));
	g->ui = ui;
	printf("%s\n", __func__);
	mui_window_t * mbar = mui_menubar_new(ui);
	mui_window_set_action(mbar, _test_menubar_action, g);

	mui_menubar_add_menu(mbar, FCC('a','p','p','l'), m_color_apple_menu, 2);
	mui_menubar_add_simple(mbar, "File",
								FCC('f','i','l','e'),
								m_file_menu);
	mui_menubar_add_simple(mbar, "Machine",
								FCC('m','a','c','h'),
								m_machine_menu);
	mui_menubar_add_simple(mbar, "CPU",
								FCC('c','p','u','m'),
								m_cpu_menu);
//	mii_mui_configure_slots(g->ui, &g_machine_conf);
	mii_mui_load_binary(g->ui, &g_loadbin_conf);
//	mii_mui_load_1mbrom(g->ui, &g_machine_conf.slot[0].conf.rom1mb);
//	mii_mui_load_2dsk(g->ui, &g_machine_conf.slot[0].conf.disk2, MII_2DSK_DISKII);
//	mii_mui_about(g->ui);
//	mii_mui_configure_ssc(g->ui, &g_machine_conf.slot[0].conf.ssc);

#if 0
	mui_alert(ui, C2_PT(0,0),
					"Testing one Two",
					"Do you really want the printer to catch fire?\n"
					"This operation cannot be cancelled.",
					MUI_ALERT_WARN);
#endif
#if 0
	mui_stdfile_get(ui,
				C2_PT(0, 0),
				"Select image for SmartPort card",
				"hdv,po,2mg",
				getenv("HOME"), 0);
#endif

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
	.name 		= "MII UI Test",
	.init 		= _init,
	.dispose 	= _dispose,
	.draw 		= _draw,
	.event 		= _event,
};
