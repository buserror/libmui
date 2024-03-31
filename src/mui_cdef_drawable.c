/*
 * mui_cdef_drawable.c
 *
 * Copyright (C) 2023 Michel Pollet <buserror@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 */

#include <stdio.h>
#include <stdlib.h>

#include "mui.h"

enum {
	MUI_CONTROL_DRAWABLE 		= FCC('D','R','A','W'),
};

typedef struct mui_drawable_control_t {
	mui_control_t 		control;
	uint16_t			flags;
	mui_drawable_t *	mask;
	mui_drawable_array_t drawables;
} mui_drawable_control_t;

IMPLEMENT_C_ARRAY(mui_drawable_array);

static void
mui_drawable_draw(
		mui_window_t * 	win,
		mui_control_t * c,
		mui_drawable_t *dr )
{
	c2_rect_t f = c->frame;
	c2_rect_offset(&f, win->content.l, win->content.t);

	mui_drawable_clip_push(dr, &f);
	mui_drawable_control_t *dc = (mui_drawable_control_t *)c;

	for (uint i = 0; i < dc->drawables.count; i++) {
		mui_drawable_t *d = dc->drawables.e[i];
		if (!d->pix.pixels)
			continue;
		c2_rect_t src = C2_RECT_WH(0, 0,
							d->pix.size.x,
							d->pix.size.y);
		c2_rect_offset(&src, d->origin.x, d->origin.y);

		pixman_image_composite32(PIXMAN_OP_OVER,
				mui_drawable_get_pixman(d),
				mui_drawable_get_pixman(dc->mask),
				mui_drawable_get_pixman(dr),
				src.l, src.t, 0, 0,
				f.l, f.t,
				c2_rect_width(&src), c2_rect_height(&src));
	}
	mui_drawable_clip_pop(dr);
}

static bool
mui_cdef_drawable(
		struct mui_control_t * 	c,
		uint8_t 				what,
		void * 					param)
{
	switch (what) {
		case MUI_CDEF_DRAW: {
			mui_drawable_t * dr = param;
			switch (c->type) {
				case MUI_CONTROL_DRAWABLE:
					mui_drawable_draw(c->win, c, dr);
					break;
			}
		}	break;
		case MUI_CDEF_DISPOSE: {
			switch (c->type) {
				case MUI_CONTROL_DRAWABLE: {
					mui_drawable_control_t *dc = (mui_drawable_control_t *)c;
					for (uint i = 0; i < dc->drawables.count; i++) {
						mui_drawable_t *d = dc->drawables.e[i];
						mui_drawable_dispose(d);
					}
					mui_drawable_dispose(dc->mask);
					mui_drawable_array_free(&dc->drawables);
				}	break;
			}
		}	break;
	}
	return false;
}

mui_control_t *
mui_drawable_control_new(
		mui_window_t * 	win,
		c2_rect_t 		frame,
		mui_drawable_t * dr,
		mui_drawable_t * mask,
		uint16_t 		flags)
{
	mui_drawable_control_t *dc = (mui_drawable_control_t *)mui_control_new(
				win, MUI_CONTROL_DRAWABLE, mui_cdef_drawable,
				frame,  NULL, 0, sizeof(mui_drawable_control_t));
	dc->mask = mask;
	dc->flags = flags;
	if (dr)
		mui_drawable_array_add(&dc->drawables, dr);
	return &dc->control;
}

mui_drawable_t *
mui_drawable_control_get_drawable(
		mui_control_t * c)
{
	mui_drawable_control_t *dc = (mui_drawable_control_t *)c;
	return dc->drawables.count ? dc->drawables.e[0] : NULL;
}

