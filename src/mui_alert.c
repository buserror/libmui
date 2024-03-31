/*
 * mui_alert.c
 *
 * Copyright (C) 2023 Michel Pollet <buserror@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "mui.h"


typedef struct mui_alert_t {
	mui_window_t 		win;
	mui_control_t *		ok, *cancel;
} mui_alert_t;

static int
_mui_alert_button_cb(
		mui_control_t * c,
		void * 			cb_param,
		uint32_t 		what,
		void * 			param)
{
//	mui_alert_t * alert = (mui_alert_t *)c->win;

	// notify the window handler of the control action
	mui_window_action(c->win, what, c);
	mui_window_dispose(c->win);
	return 0;
}

mui_window_t *
mui_alert(
		struct mui_t * ui,
		c2_pt_t where,
		const char * title,
		const char * message,
		uint16_t flags )
{
	c2_rect_t cf = C2_RECT_WH(0, 0, 540, 200);

	if (where.x && where.y)
		c2_rect_offset(&cf, where.x, where.y);
	else
		c2_rect_offset(&cf,
				(ui->screen_size.x / 2) - (c2_rect_width(&cf) / 2),
				(ui->screen_size.y * 0.3) - (c2_rect_height(&cf) / 2));
	mui_window_t *w = mui_window_create(ui, cf,
					NULL, MUI_WINDOW_LAYER_ALERT,
					title, sizeof(mui_alert_t));
	mui_alert_t * alert = (mui_alert_t *)w;
	mui_control_t * c = NULL;

	cf = C2_RECT_WH(0, 0, 120, 40);
	c2_rect_left_of(&cf, c2_rect_width(&w->content), 20);
	c2_rect_top_of(&cf, c2_rect_height(&w->content), 20);
	if (flags & MUI_ALERT_FLAG_OK) {
		alert->ok = c = mui_button_new(w, cf, MUI_BUTTON_STYLE_DEFAULT,
						"OK", MUI_ALERT_BUTTON_OK);
		alert->ok->key_equ = MUI_KEY_EQU(0, 13); // return
		c2_rect_left_of(&cf, cf.l, 30);
	}
	if (flags & MUI_ALERT_FLAG_CANCEL) {
		alert->cancel = c = mui_button_new(w, cf, MUI_BUTTON_STYLE_NORMAL,
							"Cancel", MUI_ALERT_BUTTON_CANCEL);
		alert->cancel->key_equ = MUI_KEY_EQU(0, 27); // ESC
	}
	cf = C2_RECT_WH(0, 10, 540-140, 70);
	c2_rect_left_of(&cf, c2_rect_width(&w->content), 20);
	c = mui_textbox_new(w, cf, message, NULL, MUI_TEXT_ALIGN_COMPACT);
	cf = C2_RECT_WH(10, 10, 80, 85);
	c = mui_textbox_new(w, cf,
				"", "icon_large",
				MUI_TEXT_ALIGN_CENTER | MUI_TEXT_ALIGN_MIDDLE |
				MUI_TEXT_ALIGN_COMPACT);

	c = NULL;
	TAILQ_FOREACH(c, &w->controls, self) {
		if (mui_control_get_uid(c) == 0)
			continue;
		mui_control_set_action(c, _mui_alert_button_cb, alert);
	}

	return w;
}

