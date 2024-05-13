/*
 * mui_cdef_buttons.c
 *
 * Copyright (C) 2023 Michel Pollet <buserror@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 */

#include <stdio.h>
#include <stdlib.h>

#include "mui.h"
#include "cg.h"

enum {
	MUI_CONTROL_BUTTON				= FCC('b','u','t','n'),

};

extern const mui_control_color_t mui_control_color[MUI_CONTROL_STATE_COUNT];

#define BUTTON_INSET 4
void
mui_button_draw(
		mui_window_t * 	win,
		mui_control_t * c,
		mui_drawable_t *dr )
{
	c2_rect_t f = c->frame;
	c2_rect_offset(&f, win->content.l, win->content.t);

	struct cg_ctx_t * cg = mui_drawable_get_cg(dr);

	cg_set_source_color(cg, &CG_COLOR(mui_control_color[c->state].frame));
	if (c->style == MUI_BUTTON_STYLE_DEFAULT) {
		cg_set_line_width(cg, 3);
		cg_round_rectangle(cg, f.l, f.t,
						c2_rect_width(&f), c2_rect_height(&f),
						10, 10);
		cg_stroke(cg);
		c2_rect_inset(&f, BUTTON_INSET, BUTTON_INSET);
	}
	mui_font_t * main = mui_font_find(win->ui, "main");
	stb_ttc_measure m = {};
	mui_font_text_measure(main, c->title, &m);

	int title_width = m.x1;// - m.x0;
	c2_rect_t title = C2_RECT_WH(0, 0, title_width, m.ascent - m.descent);
	c2_rect_offset(&title,
			f.l + ((c2_rect_width(&f) / 2) - (c2_rect_width(&title)) / 2),
			f.t + (c2_rect_height(&f) / 2) - (c2_rect_height(&title) / 2));
	mui_drawable_clip_push(dr, &f);
	cg = mui_drawable_get_cg(dr);
	c2_rect_t inner = f;
	c2_rect_inset(&inner, 1, 1);
	cg_set_line_width(cg, 2);
	cg_round_rectangle(cg, inner.l, inner.t,
					c2_rect_width(&inner), c2_rect_height(&inner), 6, 6);
	cg_set_source_color(cg, &CG_COLOR(mui_control_color[c->state].fill));
	cg_fill_preserve(cg);
//	cg_rectangle(cg, title.l, title.t,
//					c2_rect_width(&title), c2_rect_height(&title));
	cg_set_source_color(cg, &CG_COLOR(mui_control_color[c->state].frame));
	cg_stroke(cg);
	// offset for leading space
	mui_font_text_draw(main, dr,
			C2_PT(title.l - m.x0, title.t), c->title, strlen(c->title),
			mui_control_color[c->state].text);
	mui_drawable_clip_pop(dr);
}

void
mui_check_rad_draw(
		mui_window_t * 	win,
		mui_control_t * c,
		mui_drawable_t *dr )
{
	mui_font_t * main = mui_font_find(win->ui, "main");
	c2_rect_t f = c->frame;
	c2_rect_offset(&f, win->content.l, win->content.t);

	c2_rect_t box = f;
	box.r = box.l + (main->size * 0.95);
	box.b = box.t + (main->size * 0.95);
	c2_rect_offset(&box, 1, (c2_rect_height(&f) / 2) - (c2_rect_height(&box) / 2));
	c2_rect_t title = f;
	title.l = box.r + 8;

//	mui_drawable_clip_push(dr, &f);
	// only do a a get_cg after the clip is set, as this is what converts
	// the drawable clip rectangle list into a cg path
	struct cg_ctx_t * cg = mui_drawable_get_cg(dr);
	if (0) {	// debug draw the text rectangle as a box
		cg_rectangle(cg, title.l, title.t,
						c2_rect_width(&title), c2_rect_height(&title));
		cg_stroke(cg);
	}
	// draw the box/circle
	if (c->style == MUI_BUTTON_STYLE_RADIO) {
		cg_circle(cg, box.l + (c2_rect_width(&box) / 2),
						box.t + (c2_rect_height(&box) / 2),
						c2_rect_width(&box) / 2);
		cg_set_source_color(cg, &CG_COLOR(mui_control_color[c->state].fill));
		cg_fill_preserve(cg);
		cg_set_line_width(cg, 2);
		cg_set_source_color(cg, &CG_COLOR(mui_control_color[c->state].frame));
		cg_stroke(cg);
		if (c->value) { // fill the inside circle
			c2_rect_inset(&box, 5, 5);
			cg_circle(cg, box.l + (c2_rect_width(&box) / 2),
							box.t + (c2_rect_height(&box) / 2),
							c2_rect_width(&box) / 2);
			cg_fill(cg);
		}
	} else {
		cg_rectangle(cg, box.l, box.t,
						c2_rect_width(&box), c2_rect_height(&box));
		cg_set_source_color(cg, &CG_COLOR(mui_control_color[c->state].fill));
		cg_fill_preserve(cg);
		cg_set_line_width(cg, 2);
		cg_set_source_color(cg, &CG_COLOR(mui_control_color[c->state].frame));
		cg_stroke(cg);
		// now the cross
		if (c->value) {
			cg_set_line_width(cg, 2);
			cg_move_to(cg, box.l, box.t);
			cg_line_to(cg, box.r, box.b);
			cg_move_to(cg, box.r, box.t);
			cg_line_to(cg, box.l, box.b);
			cg_stroke(cg);
		}
	}
	mui_font_textbox(main, dr,
			title, c->title, 0,
			c->state == MUI_CONTROL_STATE_DISABLED ?
					mui_control_color[c->state].text :
					mui_control_color[0].text,
			MUI_TEXT_ALIGN_MIDDLE|MUI_TEXT_ALIGN_COMPACT);
//	mui_drawable_clip_pop(dr);
}


static bool
mui_button_mouse(
		struct mui_control_t * 	c,
		mui_event_t * 			ev)
{
	if (c->state == MUI_CONTROL_STATE_DISABLED)
		return false;

	c2_rect_t f = c->frame;
	c2_rect_offset(&f, c->win->content.l, c->win->content.t);

	switch (ev->type) {
		case MUI_EVENT_BUTTONDOWN: {
			if (c2_rect_contains_pt(&f, &ev->mouse.where))
				mui_control_set_state(c, MUI_CONTROL_STATE_CLICKED);
		}	break;
		case MUI_EVENT_BUTTONUP: {
			if (c->state != MUI_CONTROL_STATE_CLICKED)
				break;
			mui_control_set_state(c, MUI_CONTROL_STATE_NORMAL);
			switch (c->style) {
				case MUI_BUTTON_STYLE_NORMAL:
				case MUI_BUTTON_STYLE_DEFAULT:
					break;
				/* Look for all other matching radio buttons, turn
				 * their values off, before setting this one to on */
				case MUI_BUTTON_STYLE_RADIO: {
					if (c->uid_mask) {
						mui_control_t * c2 = NULL;
						TAILQ_FOREACH(c2, &c->win->controls, self) {
							if (c2->type == MUI_CONTROL_BUTTON &&
									c2->style == MUI_BUTTON_STYLE_RADIO &&
									(c2->uid & c->uid_mask) == (c->uid & c->uid_mask) &&
									c2 != c) {
							//	printf("OFF %4.4s\n", (char*)&c2->uid);
								mui_control_set_value(c2, false);
							}
						}
					}
				//	printf("ON %4.4s\n", (char*)&c->uid);
					mui_control_set_value(c, true);
				}	break;
				case MUI_BUTTON_STYLE_CHECKBOX:
					mui_control_set_value(c,
							!mui_control_get_value(c));
					break;
			}
			mui_control_action(c, MUI_CONTROL_ACTION_SELECT, NULL);
		}	break;
		case MUI_EVENT_DRAG: {
			if (c2_rect_contains_pt(&f, &ev->mouse.where))
				mui_control_set_state(c, MUI_CONTROL_STATE_CLICKED);
			else
				mui_control_set_state(c, MUI_CONTROL_STATE_NORMAL);
		}	break;
		default:
			break;
	}
	return true;
}

static bool
mui_cdef_button(
		struct mui_control_t * 	c,
		uint8_t 				what,
		void * 					param)
{
	switch (what) {
		case MUI_CDEF_INIT:
			break;
		case MUI_CDEF_DISPOSE:
			break;
		case MUI_CDEF_DRAW: {
			mui_drawable_t * dr = param;
			switch (c->style) {
				case MUI_BUTTON_STYLE_NORMAL:
				case MUI_BUTTON_STYLE_DEFAULT:
					mui_button_draw(c->win, c, dr);
					break;
				case MUI_BUTTON_STYLE_CHECKBOX:
				case MUI_BUTTON_STYLE_RADIO:
					mui_check_rad_draw(c->win, c, dr);
					break;
				default:
					return false;
			}
		}	break;
		case MUI_CDEF_EVENT: {
			mui_event_t *ev = param;
		//	printf("%s event %d where %dx%d\n", __func__, ev->type,
		//			ev->mouse.where.x,
		//			ev->mouse.where.y);
			switch (ev->type) {
				case MUI_EVENT_BUTTONUP:
				case MUI_EVENT_DRAG:
				case MUI_EVENT_BUTTONDOWN: {
					return mui_button_mouse(c, ev);
				}	break;
				default:
					break;
			}
		}	break;
		case MUI_CDEF_SELECT: {
			if (c->style == MUI_BUTTON_STYLE_CHECKBOX) {
				mui_control_set_value(c, !c->value);
			}
		}	break;
	}
	return false;
}

mui_control_t *
mui_button_new(
		mui_window_t * 	win,
		c2_rect_t 		frame,
		uint8_t			style,
		const char *	title,
		uint32_t 		uid )
{
	switch (style) {
		case MUI_BUTTON_STYLE_NORMAL:
			break;
		case MUI_BUTTON_STYLE_DEFAULT:
			c2_rect_inset(&frame, -BUTTON_INSET, -BUTTON_INSET);
			break;
		case MUI_BUTTON_STYLE_CHECKBOX:
			break;
		case MUI_BUTTON_STYLE_RADIO:
			break;
	}
	mui_control_t * c = mui_control_new(
				win,  MUI_CONTROL_BUTTON, mui_cdef_button,
				frame, title, uid, 0);
	c->style = style;
	return c;
}
