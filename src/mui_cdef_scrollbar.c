/*
 * mui_cdef_scrollbar.c
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
	MUI_CONTROL_SCROLLBAR			= FCC('s','b','a','r'),
};

extern const mui_control_color_t mui_control_color[MUI_CONTROL_STATE_COUNT];

enum mui_sb_part_e {
	MUI_SB_PART_FRAME = 0,
	MUI_SB_PART_UP,
	MUI_SB_PART_DOWN,
	MUI_SB_PART_PAGEUP,
	MUI_SB_PART_PAGEDOWN,
	MUI_SB_PART_THUMB,
	MUI_SB_PART_COUNT,
};

typedef struct mui_scrollbar_control_t {
	mui_control_t 		control;
	uint32_t			visible;
	uint32_t			max;
	c2_pt_t 			drag_offset;
	uint32_t			saved_value; // to handle 'snapback'
} mui_scrollbar_control_t;

static void
mui_scrollbar_make_rects(
		mui_control_t * c,
		c2_rect_t * parts)
{
	c2_rect_t f = c->frame;
	c2_rect_offset(&f, c->win->content.l, c->win->content.t);
	parts[MUI_SB_PART_FRAME] = f;

	c2_rect_t part = f;
	part.b = part.t + c2_rect_width(&part);
	parts[MUI_SB_PART_UP] = part;
	part = f;
	part.t = part.b - c2_rect_width(&part);
	parts[MUI_SB_PART_DOWN] = part;

	mui_scrollbar_control_t *sb = (mui_scrollbar_control_t *)c;
	if (sb->max <= sb->visible) {
		c2_rect_t z = {};
		parts[MUI_SB_PART_THUMB] = z;
		parts[MUI_SB_PART_PAGEUP] = z;
		parts[MUI_SB_PART_PAGEDOWN] = z;
		return;
	}
	part = f;
	part.t = parts[MUI_SB_PART_UP].b + 1;
	part.b = parts[MUI_SB_PART_DOWN].t - 1;

	float visible = sb->visible / (float)sb->max;
	float thumb_size = visible * sb->visible;
	if (thumb_size < 20)
		thumb_size = 20;
	float thumb_pos = c->value / ((float)sb->max- sb->visible);
	float thumb_offset = 0.5 + thumb_pos * (c2_rect_height(&part) - thumb_size);
//	printf("%s visible:%.2f ts: %.2f thumb_pos:%.2f thumb_offset:%.2f\n",
//		__func__, visible, thumb_size, thumb_pos, thumb_offset);

	part.b = part.t + thumb_size;
	c2_rect_offset(&part, 0, thumb_offset);
	if (part.b > parts[MUI_SB_PART_DOWN].t) {
		c2_rect_offset(&part, 0, parts[MUI_SB_PART_DOWN].t - part.b);
	}
	parts[MUI_SB_PART_THUMB] = part;
	part = f;
	part.t = parts[MUI_SB_PART_UP].b + 1;
	part.b = parts[MUI_SB_PART_THUMB].t - 1;
	parts[MUI_SB_PART_PAGEUP] = part;
	part = f;
	part.t = parts[MUI_SB_PART_THUMB].b + 1;
	part.b = parts[MUI_SB_PART_DOWN].t - 1;
	parts[MUI_SB_PART_PAGEDOWN] = part;
}

static void
mui_scrollbar_draw(
		mui_window_t * 	win,
		mui_control_t * c,
		mui_drawable_t *dr )
{
	c2_rect_t f = c->frame;
	c2_rect_offset(&f, win->content.l, win->content.t);

	struct cg_ctx_t * cg = mui_drawable_get_cg(dr);
	cg_set_line_width(cg, 1);

	cg_rectangle(cg, f.l, f.t,
					c2_rect_width(&f), c2_rect_height(&f));
	cg_set_source_color(cg, &CG_COLOR(mui_control_color[c->state].fill));
	cg_fill_preserve(cg);
	cg_set_source_color(cg, &CG_COLOR(mui_control_color[c->state].frame));
	cg_stroke(cg);

	mui_font_t * icons = mui_font_find(win->ui, "icon_small");

	c2_rect_t parts[MUI_SB_PART_COUNT];
	mui_scrollbar_make_rects(c, parts);

	mui_color_t contentFill = MUI_COLOR(0xa0a0a0ff);
	mui_color_t decoColor 	= MUI_COLOR(0x666666ff);

	c2_rect_t pf;
	pf = parts[MUI_SB_PART_UP];
	cg_rectangle(cg, pf.l, pf.t,
					c2_rect_width(&pf), c2_rect_height(&pf));
	cg_set_source_color(cg,
				c->flags.hit_part == MUI_SB_PART_UP ?
					&CG_COLOR(decoColor) :
					&CG_COLOR(mui_control_color[c->state].fill));
	cg_fill_preserve(cg);
	cg_set_source_color(cg, &CG_COLOR(mui_control_color[c->state].frame));
	cg_stroke(cg);

	stb_ttc_measure m = {};
	mui_font_text_measure(icons, "", &m);
	pf.l = pf.l + (c2_rect_width(&pf) - m.x1 - m.x0) / 2;
	mui_font_text_draw(icons, dr, pf.tl, "", 0,
				mui_control_color[c->state].text);

	pf = parts[MUI_SB_PART_DOWN];
	cg_rectangle(cg, pf.l, pf.t,
					c2_rect_width(&pf), c2_rect_height(&pf));
	cg_set_source_color(cg,
				c->flags.hit_part == MUI_SB_PART_DOWN ?
					&CG_COLOR(decoColor) :
					&CG_COLOR(mui_control_color[c->state].fill));
	cg_fill_preserve(cg);
	cg_set_source_color(cg, &CG_COLOR(mui_control_color[c->state].frame));
	cg_stroke(cg);

	mui_font_text_measure(icons, "", &m);
	pf.l = pf.l + (c2_rect_width(&pf) - m.x1 - m.x0) / 2;
	mui_font_text_draw(icons, dr, pf.tl, "", 0,
				mui_control_color[c->state].text);

	pf = parts[MUI_SB_PART_PAGEUP];
	if (c2_rect_height(&pf) > 0) {
		cg_rectangle(cg, pf.l, pf.t,
						c2_rect_width(&pf), c2_rect_height(&pf));
		cg_set_source_color(cg,
				c->flags.hit_part == MUI_SB_PART_PAGEUP ?
					&CG_COLOR(decoColor) :
					&CG_COLOR(contentFill));
		cg_fill(cg);
	}
	pf = parts[MUI_SB_PART_PAGEDOWN];
	if (c2_rect_height(&pf) > 0) {
		cg_rectangle(cg, pf.l, pf.t,
						c2_rect_width(&pf), c2_rect_height(&pf));
		cg_set_source_color(cg,
				c->flags.hit_part == MUI_SB_PART_PAGEDOWN ?
					&CG_COLOR(decoColor) :
					&CG_COLOR(contentFill));
		cg_fill(cg);
	}
	pf = parts[MUI_SB_PART_THUMB];
	if (c2_rect_height(&pf) > 0) {
		cg_rectangle(cg, pf.l, pf.t,
						c2_rect_width(&pf), c2_rect_height(&pf));
		cg_set_source_color(cg,
				c->flags.hit_part == MUI_SB_PART_THUMB ?
					&CG_COLOR(decoColor) :
					&CG_COLOR(mui_control_color[c->state].fill));
		cg_fill_preserve(cg);
		cg_set_source_color(cg,
				&CG_COLOR(mui_control_color[c->state].frame));
		cg_stroke(cg);
	}
}

static void
_mui_scrollbar_scroll(
		mui_control_t * c,
		int32_t 		delta )
{
	mui_scrollbar_control_t *sb = (mui_scrollbar_control_t *)c;
	int32_t v = (int32_t)c->value + delta;
	if (v < 0)
		v = 0;
	if (v > ((int32_t)sb->max - (int32_t)sb->visible))
		v = sb->max - sb->visible;
	c->value = v;
	mui_control_inval(c);
	mui_control_action(c, MUI_CONTROL_ACTION_VALUE_CHANGED, NULL);
}

static bool
mui_scrollbar_mouse(
		struct mui_control_t * 	c,
		mui_event_t * 			ev)
{
	mui_scrollbar_control_t *sb = (mui_scrollbar_control_t *)c;
	c2_rect_t parts[MUI_SB_PART_COUNT];
	mui_scrollbar_make_rects(c, parts);

	switch (ev->type) {
		case MUI_EVENT_BUTTONDOWN: {
			for (int i = 1; i < MUI_SB_PART_COUNT; ++i) {
				if (c2_rect_contains_pt(&parts[i], &ev->mouse.where)) {
					c->flags.hit_part = i;
					sb->drag_offset.x =
							ev->mouse.where.x - parts[i].l;
					sb->drag_offset.y =
							ev->mouse.where.y - parts[i].t;
					sb->saved_value = c->value;
					break;
				}
			}
			int part = c->flags.hit_part % MUI_SB_PART_COUNT;
			switch (part) {
				case MUI_SB_PART_DOWN:
				case MUI_SB_PART_UP: {
					_mui_scrollbar_scroll(c,
							part == MUI_SB_PART_UP ? -30 : 30);
				}	break;
				case MUI_SB_PART_PAGEUP:
				case MUI_SB_PART_PAGEDOWN: {
					int32_t offset = sb->visible;
					_mui_scrollbar_scroll(c,
							part == MUI_SB_PART_PAGEUP ?
								-offset : offset);
				}	break;
				case MUI_SB_PART_THUMB:
					mui_control_inval(c);
					break;
			}
		//	printf("%s hit part %d\n", __func__, c->flags.hit_part);
		}	break;
		case MUI_EVENT_DRAG: {
			if (!c->flags.hit_part)
				break;
			int part = c->flags.hit_part % MUI_SB_PART_COUNT;
			c2_rect_t test_rect = parts[part];
			if (part == MUI_SB_PART_THUMB)
				c2_rect_inset(&test_rect, -60, -60);
			if (c2_rect_contains_pt(&test_rect, &ev->mouse.where)) {
				c->flags.hit_part = part;
				switch (part) {
					case MUI_SB_PART_THUMB: {
						c2_rect_t nt = parts[part];
						c2_rect_offset(&nt, 0,
								-(nt.t + parts[MUI_SB_PART_UP].b) +
								ev->mouse.where.y - sb->drag_offset.y);
					//	printf("%s thumb %s\n", __func__, c2_rect_as_str(&nt));
						if (nt.t < 0)
							c2_rect_offset(&nt, 0, -nt.t);
						if (nt.b > parts[MUI_SB_PART_DOWN].t)
							c2_rect_offset(&nt, 0, parts[MUI_SB_PART_DOWN].t - nt.b);

						int max_pixels = parts[MUI_SB_PART_DOWN].t -
												parts[MUI_SB_PART_UP].b -
										c2_rect_height(&nt);
						uint32_t nv = nt.t * (sb->max - sb->visible) / max_pixels;
						if (nv > (sb->max - sb->visible))
							nv = sb->max - sb->visible;
						c->value = nv;
					//	printf("v is %d vs %d max %d = %d new val %d\n",
					//			nt.t, max_pixels, sb->max, nv,
					//			mui_control_get_value(c));
						mui_control_inval(c);
						mui_control_action(c, MUI_CONTROL_ACTION_VALUE_CHANGED, NULL);
					}	break;
				}
			} else {
				c->flags.hit_part = part + MUI_SB_PART_COUNT;
				if (part == MUI_SB_PART_THUMB) {
					c->value = sb->saved_value;
					mui_control_inval(c);
					mui_control_action(c, MUI_CONTROL_ACTION_VALUE_CHANGED, NULL);
				}
			}
		}	break;
		case MUI_EVENT_BUTTONUP: {
			if (!c->flags.hit_part)
				break;
			mui_control_inval(c);
			c->flags.hit_part = 0;
		}	break;
	}
	return true;
}

static bool
mui_cdef_scrollbar(
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
			mui_scrollbar_draw(c->win, c, dr);
		}	break;
		case MUI_CDEF_EVENT: {
		//	printf("%s event\n", __func__);
			mui_event_t *ev = param;
			switch (ev->type) {
				case MUI_EVENT_BUTTONUP:
				case MUI_EVENT_DRAG:
				case MUI_EVENT_BUTTONDOWN: {
					return mui_scrollbar_mouse(c, ev);
				}	break;
			}
		}	break;
	}
	return false;
}

uint32_t
mui_scrollbar_get_max(
		mui_control_t * c)
{
	mui_scrollbar_control_t *sb = (mui_scrollbar_control_t *)c;
	return sb->max;
}
void
mui_scrollbar_set_max(
		mui_control_t * c,
		uint32_t 		max)
{
	mui_scrollbar_control_t *sb = (mui_scrollbar_control_t *)c;
	sb->max = max;
	mui_control_inval(c);
}
void
mui_scrollbar_set_page(
		mui_control_t * c,
		uint32_t 		page)
{
	mui_scrollbar_control_t *sb = (mui_scrollbar_control_t *)c;
	sb->visible = page;
	mui_control_inval(c);
}

mui_control_t *
mui_scrollbar_new(
		mui_window_t * 	win,
		c2_rect_t 		frame,
		uint32_t 		uid )
{
	return mui_control_new(
				win, MUI_CONTROL_SCROLLBAR, mui_cdef_scrollbar,
				frame, NULL, uid,
				sizeof(mui_scrollbar_control_t));
}
