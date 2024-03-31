/*
 * mui_cdef_boxes.c
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
	MUI_CONTROL_GROUP				= FCC('g','r','o','p'),
	MUI_CONTROL_SEPARATOR			= FCC('s','e','p','r'),
	MUI_CONTROL_TEXTBOX				= FCC('t','b','o','x'),
	MUI_CONTROL_GROUPBOX			= FCC('g','b','o','x'),
};

typedef struct mui_textbox_control_t {
	mui_control_t 		control;
	mui_font_t *		font;
	uint32_t			flags;
} mui_textbox_control_t;

extern const mui_control_color_t mui_control_color[MUI_CONTROL_STATE_COUNT];

static void
mui_textbox_draw(
		mui_window_t * 	win,
		mui_control_t * c,
		mui_drawable_t *dr )
{
	c2_rect_t f = c->frame;
	c2_rect_offset(&f, win->content.l, win->content.t);

	mui_textbox_control_t *tb = (mui_textbox_control_t *)c;

	c2_rect_t text_frame = f;
	mui_drawable_clip_push(dr, &f);
	mui_font_textbox(tb->font, dr,
			text_frame, c->title, strlen(c->title),
			mui_control_color[c->state].text,
			tb->flags);
	if (tb->flags & MUI_CONTROL_TEXTBOX_FRAME) {
		struct cg_ctx_t * cg = mui_drawable_get_cg(dr);
		cg_set_line_width(cg, 1);
		cg_set_source_color(cg, &CG_COLOR(mui_control_color[c->state].frame));
		cg_rectangle(cg, f.l, f.t,
						c2_rect_width(&f), c2_rect_height(&f));
		cg_stroke(cg);
	}
	mui_drawable_clip_pop(dr);
}

static void
mui_groupbox_draw(
		mui_window_t * 	win,
		mui_control_t * c,
		mui_drawable_t *dr )
{
	c2_rect_t f = c->frame;
	c2_rect_offset(&f, win->content.l, win->content.t);

	mui_textbox_control_t *tb = (mui_textbox_control_t *)c;

	c2_rect_t text_frame = f;
	c2_rect_t box_frame = f;

	mui_font_t * main = mui_font_find(win->ui, "main");
	stb_ttc_measure m = {};
	mui_font_text_measure(main, c->title, &m);
	text_frame.l += main->size * 0.3;
	text_frame.b = text_frame.t + main->size;
	text_frame.r = text_frame.l + m.x1 + m.x0;
	box_frame.t += m.ascent * 0.85;

	mui_color_t contentFill = MUI_COLOR(0xf0f0f0ff);
	mui_color_t decoColor 	= MUI_COLOR(0x666666ff);

	struct cg_ctx_t * cg = mui_drawable_get_cg(dr);
	cg_set_line_width(cg, 1);
	cg_set_source_color(cg, &CG_COLOR(decoColor));
	cg_rectangle(cg, box_frame.l, box_frame.t,
					c2_rect_width(&box_frame), c2_rect_height(&box_frame));
	cg_stroke(cg);
	// now erase text background
	cg_set_source_color(cg, &CG_COLOR(contentFill));
	cg_rectangle(cg, text_frame.l, text_frame.t,
					c2_rect_width(&text_frame), c2_rect_height(&text_frame));
	cg_fill(cg);

//	mui_drawable_clip_push(dr, &f);
	mui_font_textbox(main, dr,
			text_frame, c->title, strlen(c->title),
			mui_control_color[c->state].text,
			tb->flags);
//	mui_drawable_clip_pop(dr);
}

static void
mui_separator_draw(
		mui_window_t * 	win,
		mui_control_t * c,
		mui_drawable_t *dr )
{
	c2_rect_t f = c->frame;
	c2_rect_offset(&f, win->content.l, win->content.t);

//	mui_drawable_clip_push(dr, &f);
	struct cg_ctx_t * cg = mui_drawable_get_cg(dr);
	cg_set_line_width(cg, 1);

	mui_color_t decoColor 	= MUI_COLOR(0x666666ff);
	cg_set_source_color(cg, &CG_COLOR(decoColor));
	cg_move_to(cg, f.l + 0, f.t);
	cg_line_to(cg, f.r - 0, f.t);
	cg_stroke(cg);
//	mui_drawable_clip_pop(dr);
}

static bool
mui_cdef_boxes(
		struct mui_control_t * 	c,
		uint8_t 				what,
		void * 					param)
{
//	mui_textbox_control_t *tb = (mui_textbox_control_t *)c;
	switch (what) {
		case MUI_CDEF_DRAW: {
			mui_drawable_t * dr = param;
			switch (c->type) {
				case MUI_CONTROL_SEPARATOR:
					mui_separator_draw(c->win, c, dr);
					break;
				case MUI_CONTROL_GROUPBOX:
					mui_groupbox_draw(c->win, c, dr);
					break;
				case MUI_CONTROL_TEXTBOX:
					mui_textbox_draw(c->win, c, dr);
					break;
				default:
					break;
			}
		}	break;
	}
	return false;
}

mui_control_t *
mui_textbox_new(
		mui_window_t * 	win,
		c2_rect_t 		frame,
		const char *	text,
		const char * 	font,
		uint32_t		flags )
{
	mui_control_t * c = mui_control_new(
				win, MUI_CONTROL_TEXTBOX, mui_cdef_boxes,
				frame,  text, 0, sizeof(mui_textbox_control_t));
	mui_textbox_control_t *tb = (mui_textbox_control_t *)c;
	tb->font = mui_font_find(win->ui, font ? font : "main");
	tb->flags = flags;
	return c;
}

mui_control_t *
mui_separator_new(
		mui_window_t * 	win,
		c2_rect_t 		frame)
{
	return mui_control_new(
				win, MUI_CONTROL_SEPARATOR, mui_cdef_boxes,
				frame,  NULL, 0, sizeof(mui_textbox_control_t));
}

mui_control_t *
mui_groupbox_new(
		mui_window_t * 	win,
		c2_rect_t 		frame,
		const char *	title,
		uint32_t		flags )
{
	mui_control_t * c = mui_control_new(
				win, MUI_CONTROL_GROUPBOX, mui_cdef_boxes,
				frame,  title, 0, sizeof(mui_textbox_control_t));
	mui_textbox_control_t *tb = (mui_textbox_control_t *)c;
	tb->flags = flags;
	return c;
}
