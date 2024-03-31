/*
 * mui_drawable.c
 *
 * Copyright (C) 2023 Michel Pollet <buserror@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 */

#include <stdio.h>
#include <stdlib.h>

#include <pixman.h>
#include "mui.h"
#include "cg.h"


IMPLEMENT_C_ARRAY(mui_clip_stack);

// create a new mui_draware of size w x h, bpp depth.
// optionally allocate the pixels if pixels is NULL
mui_drawable_t *
mui_drawable_init(
		mui_drawable_t * d,
		c2_pt_t size,
		uint8_t bpp,
		void * pixels, // if NULL, will allocate
		uint32_t row_bytes)
{
	static const mui_drawable_t zero = {};
	*d = zero;
	d->pix.bpp = bpp;
	d->pix.size = size;
	d->pix.row_bytes = row_bytes == 0 ?
				(uint32_t)((size.x * (bpp / 8)) + 3) & ~3 :
						row_bytes;
	d->pix.pixels = pixels ? pixels : malloc(d->pix.row_bytes * size.y);
	d->dispose_pixels = pixels == NULL;
	return d;
}

// create a new mui_draware of size w x h, bpp depth.
// optionally allocate the pixels if pixels is NULL
mui_drawable_t *
mui_drawable_new(
		c2_pt_t size,
		uint8_t bpp,
		void * pixels, // if NULL, will allocate
		uint32_t row_bytes)
{
	mui_drawable_t *d = malloc(sizeof(mui_drawable_t));
	mui_drawable_init(d, size, bpp, pixels, row_bytes);
	d->dispose_drawable = 1;
	return d;
}

void
mui_drawable_clear(
		mui_drawable_t * dr)
{
	if (!dr)
		return;
	if (dr->cg)
		cg_destroy(dr->cg);
	dr->cg = NULL;
	if (dr->cg_surface)
		cg_surface_destroy(dr->cg_surface);
	dr->cg_surface = NULL;
	if (dr->pixman)
		pixman_image_unref(dr->pixman);
	dr->pixman = NULL;
	for (uint i = 0; i < dr->clip.count; i++)
		pixman_region32_fini(&dr->clip.e[i]);
	mui_clip_stack_clear(&dr->clip);
	if (dr->pix.pixels && dr->dispose_pixels)
		free(dr->pix.pixels);
	static const mui_pixmap_t zero = {};
	dr->pix = zero;
	dr->dispose_pixels = 0;
	dr->_pix_hash = NULL;
}

void
mui_drawable_dispose(
		mui_drawable_t * dr)
{
	if (!dr)
		return;
	mui_drawable_clear(dr);
	mui_clip_stack_free(&dr->clip);
	if (dr->dispose_drawable)
		free(dr);
}

static struct cg_ctx_t *
_cg_updated_clip(
		mui_drawable_t * dr)
{
	if (!dr->cg_clip_dirty)
		return dr->cg;
	dr->cg_clip_dirty = 0;
	cg_reset_clip(dr->cg);
	if (!dr->clip.count)
		return dr->cg;
	int cnt = 0;
	pixman_box32_t *r = pixman_region32_rectangles(
			&dr->clip.e[dr->clip.count-1], &cnt);
	for (int i = 0; i < cnt; i++) {
		cg_rectangle(dr->cg, r[i].x1, r[i].y1,
				r[i].x2 - r[i].x1, r[i].y2 - r[i].y1);
	}
	cg_clip(dr->cg);
	return dr->cg;
}

struct cg_ctx_t *
mui_drawable_get_cg(
		mui_drawable_t * dr)
{
	if (!dr)
		return NULL;

	if (dr->cg) {
		// in case the pix structure had changed...
		dr->cg_surface->stride = dr->pix.row_bytes;
		dr->cg_surface->pixels = dr->pix.pixels;
		dr->cg_surface->width = dr->pix.size.x;
		dr->cg_surface->height = dr->pix.size.y;
		return _cg_updated_clip(dr);
	}
	dr->cg_surface = cg_surface_create_for_data(
			dr->pix.size.x, dr->pix.size.y,
			dr->pix.pixels);
	dr->cg_surface->stride = dr->pix.row_bytes;
	dr->cg = cg_create(dr->cg_surface);
	return _cg_updated_clip(dr);
}

/*
 * Update clip to the 'latest' one from the stack of clips.
 */
static union pixman_image *
_pixman_updated_clip(
		mui_drawable_t * dr)
{
	if (!dr->pixman_clip_dirty)
		return dr->pixman;
	dr->pixman_clip_dirty = 0;
	if (dr->clip.count)
		pixman_image_set_clip_region32(
				dr->pixman, &dr->clip.e[dr->clip.count-1]);
	else
		pixman_image_set_clip_region32(
				dr->pixman, NULL);
	return dr->pixman;
}

union pixman_image *
mui_pixmap_make_pixman(
		mui_pixmap_t * pix)
{
	pixman_format_code_t k = PIXMAN_a8r8g8b8;
	switch (pix->bpp) {
		case 8: k = PIXMAN_a8; break;
		case 16: k = PIXMAN_r5g6b5; break;
		case 24: k = PIXMAN_r8g8b8; break;
		case 32: k = PIXMAN_a8r8g8b8; break;
	}
	union pixman_image * res = pixman_image_create_bits_no_clear(
			k, pix->size.x, pix->size.y,
			(uint32_t*)pix->pixels, pix->row_bytes);
	return res;
}

union pixman_image *
mui_drawable_get_pixman(
		mui_drawable_t * dr)
{
	if (!dr)
		return NULL;
	void * _hash = dr->pix.pixels + dr->pix.size.y * dr->pix.row_bytes;
	if (dr->_pix_hash != _hash) {
		dr->_pix_hash = _hash;
		dr->_pix_hash = dr->pix.pixels + dr->pix.size.y * dr->pix.row_bytes;
		if (dr->pixman)
			pixman_image_unref(dr->pixman);
		dr->pixman = NULL;
	}
//		return _pixman_updated_clip(dr);
	if (dr->pixman)
		return _pixman_updated_clip(dr);
	dr->pixman = mui_pixmap_make_pixman(&dr->pix);
	dr->pixman_clip_dirty = 1;
	return _pixman_updated_clip(dr);
}

pixman_region32_t *
mui_drawable_clip_get(
		mui_drawable_t * dr)
{
	if (!dr || !dr->clip.count)
		return NULL;
	return &dr->clip.e[dr->clip.count-1];
}

void
mui_drawable_set_clip(
		mui_drawable_t * dr,
		c2_rect_array_p clip )
{
	if (!dr)
		return;
	for (uint i = 0; i < dr->clip.count; i++)
		pixman_region32_fini(&dr->clip.e[i]);
	mui_clip_stack_clear(&dr->clip);
	if (clip && clip->count) {
		pixman_region32_t r = {};

		pixman_region32_init_rects(&r,
				(pixman_box32_t*)clip->e, clip->count);
		// intersect the whole lot with the whole pixmap boundary
		pixman_region32_t rg = {};
		pixman_region32_intersect_rect(&rg, &r,
				0, 0, dr->pix.size.x, dr->pix.size.y);
		pixman_region32_fini(&r);

		mui_clip_stack_add(&dr->clip, rg);
	}
	dr->pixman_clip_dirty = 1;
	dr->cg_clip_dirty = 1;
}

int
mui_drawable_clip_intersects(
		mui_drawable_t * dr,
		c2_rect_p r )
{
	if (!dr || !r)
		return 0;
	if (!dr->clip.count)
		return 1;
	// conveniently, c2_rect_t is the same as pixman_box_32_t
	pixman_region_overlap_t o = pixman_region32_contains_rectangle(
			&dr->clip.e[dr->clip.count-1], (pixman_box32_t*)r);

	return o == PIXMAN_REGION_OUT ? 0 :
			o == PIXMAN_REGION_IN ? 1 : -1;
}

int
mui_drawable_clip_push(
		mui_drawable_t * dr,
		c2_rect_p r )
{
	if (!dr || !r)
		return 0;
	pixman_region32_t rg = {};
	if (dr->clip.count == 0) {
		pixman_region32_init_rect(&rg,
				r->l, r->t, c2_rect_width(r), c2_rect_height(r));
	} else {
		pixman_region32_intersect_rect(&rg, &dr->clip.e[dr->clip.count-1],
				r->l, r->t, c2_rect_width(r), c2_rect_height(r));
	}
	mui_clip_stack_add(&dr->clip, rg);
	dr->pixman_clip_dirty = 1;
	dr->cg_clip_dirty = 1;

	return dr->clip.count;
}

int
mui_drawable_clip_push_region(
		mui_drawable_t * dr,
		pixman_region32_t * rgn )
{
	if (!dr || !rgn)
		return 0;

	pixman_region32_t rg = {};
	if (dr->clip.count == 0) {
		pixman_region32_copy(&rg,  rgn);
	} else {
		pixman_region32_intersect(&rg,  &dr->clip.e[dr->clip.count-1], rgn);
	}
	mui_clip_stack_add(&dr->clip, rg);
	dr->pixman_clip_dirty = 1;
	dr->cg_clip_dirty = 1;

	return dr->clip.count;
}

int
mui_drawable_clip_substract_region(
		mui_drawable_t * dr,
		pixman_region32_t * rgn )
{
	if (!dr || !rgn)
		return 0;

	pixman_region32_t rg = {};
	if (dr->clip.count != 0) {
		pixman_region32_subtract(&rg,  &dr->clip.e[dr->clip.count-1], rgn);
	}
	mui_clip_stack_add(&dr->clip, rg);
	dr->pixman_clip_dirty = 1;
	dr->cg_clip_dirty = 1;

	return dr->clip.count;
}

void
mui_drawable_clip_pop(
		mui_drawable_t * dr )
{
	if (!dr || !dr->clip.count)
		return;
	pixman_region32_fini(&dr->clip.e[dr->clip.count-1]);
	dr->clip.count--;
	dr->pixman_clip_dirty = 1;
	dr->cg_clip_dirty = 1;
}
