/*
 * c2_geometry_inline.h
 *
 * Copyright (C) 2023 Michel Pollet <buserror@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef C2_GEOMETRY_INLINE_H_
#define C2_GEOMETRY_INLINE_H_

#include "c2_geometry.h"

#ifndef C2_DECL
#define C2_DECL static inline __attribute__((unused))
#endif

C2_DECL void
c2_pt_offset(
		c2_pt_p p,
		c2_coord_t inX,
		c2_coord_t inY)
{
	p->x += inX;
	p->y += inY;
}

C2_DECL int
c2_pt_equal(
		const c2_pt_p p1,
		const c2_pt_p p2)
{
	return p1->v[0] == p2->v[0] && p1->v[1] == p2->v[1];
}

C2_DECL void
c2_pt_scale(
		c2_pt_p p,
		float inFactor )
{// TODO fix rounding
	p->v[X] *= inFactor;
	p->v[Y] *= inFactor;
}

C2_DECL c2_segment_p
c2_segment_set(
		c2_segment_p s,
		c2_coord_t x1,
		c2_coord_t y1,
		c2_coord_t x2,
		c2_coord_t y2 )
{
	s->a.v[X] = PMIN(x1, x2);
	s->a.v[Y] = PMIN(y1, y2);
	s->b.v[X] = PMAX(x1, x2);
	s->b.v[Y] = PMAX(y1, y2);
	return s;
}


C2_DECL int
c2_segment_equal(
		const c2_segment_p s1,
		const c2_segment_p s2)
{
	return (c2_pt_equal(&s1->a, &s2->a) && c2_pt_equal(&s1->b, &s2->b)) ||
		(c2_pt_equal(&s1->a, &s2->b) && c2_pt_equal(&s1->b, &s2->a));
}

C2_DECL int
c2_segment_isempty(
		const c2_segment_p s)
{
	return c2_pt_equal(&s->a, &s->b);
}

C2_DECL void
c2_segment_offset(
		c2_segment_p s,
		c2_coord_t	inX,
		c2_coord_t	inY )
{
	c2_pt_offset(&s->a, inX, inY);
	c2_pt_offset(&s->b, inX, inY);
}

C2_DECL void
c2_segment_scale(
		c2_segment_p s,
		double inFactor )
{
	c2_pt_scale(&s->a, inFactor);
	c2_pt_scale(&s->b, inFactor);
}


C2_DECL c2_rect_p
c2_rect_set(
		c2_rect_p r,
		c2_coord_t x1,
		c2_coord_t y1,
		c2_coord_t x2,
		c2_coord_t y2 )
{
	r->tl.v[X] = PMIN(x1, x2);
	r->tl.v[Y] = PMIN(y1, y2);
	r->br.v[X] = PMAX(x1, x2);
	r->br.v[Y] = PMAX(y1, y2);
	return r;
}

C2_DECL int
c2_rect_isempty(
		const c2_rect_p r)
{
	return c2_pt_equal(&r->tl, &r->br) ||
			r->tl.x >= r->br.x || r->tl.y >= r->br.y;
}
C2_DECL int
c2_rect_equal(
		const c2_rect_p r,
		const c2_rect_p o)
{
	return r->v[0] == o->v[0] && r->v[1] == o->v[1] &&
			r->v[2] == o->v[2] && r->v[3] == o->v[3];
	/* This got miscompiled under gcc 4.8 and 4.9.  The above is more straightforward.
	 * return c2_pt_equal(&r->tl, &o->tl) && c2_pt_equal(&r->br, &o->br);
	 */
}
C2_DECL c2_coord_t
c2_rect_height(
		const c2_rect_p r)
{
	return r->br.v[Y] - r->tl.v[Y];
}

C2_DECL c2_coord_t
c2_rect_width(
		const c2_rect_p r)
{
	return r->br.v[X] - r->tl.v[X];
}

C2_DECL c2_pt_t
c2_rect_size(
		const c2_rect_p r)
{
	return (c2_pt_t){ .x = c2_rect_width(r), .y = c2_rect_height(r) };
}

C2_DECL void
c2_rect_offset(
		c2_rect_p r,
		c2_coord_t inX,
		c2_coord_t inY)
{
	c2_pt_offset(&r->tl, inX, inY);
	c2_pt_offset(&r->br, inX, inY);
}

C2_DECL void
c2_rect_inset(
		c2_rect_p r,
		c2_coord_t inX,
		c2_coord_t inY)
{
	c2_pt_offset(&r->tl, inX, inY);
	c2_pt_offset(&r->br, -inX, -inY);
}

C2_DECL void
c2_rect_scale(
		c2_rect_p r,
		double inFactor )
{
	c2_pt_scale(&r->tl, inFactor);
	c2_pt_scale(&r->br, inFactor);
}

C2_DECL int
c2_rect_contains_pt(
		const c2_rect_p r,
		c2_pt_p	p )
{
	return (p->v[X] >= r->tl.v[X] && p->v[X] <= r->br.v[X]) &&
			(p->v[Y] >= r->tl.v[Y] && p->v[Y] <= r->br.v[Y]);
}

/* Returns 'r' as the union of 'r' and 'u' (enclosing rectangle) */
C2_DECL c2_rect_p
c2_rect_union(
		c2_rect_p r,
		const c2_rect_p u )
{
	if (!r || !u) return r;
	if (c2_rect_isempty(r)) {
		*r = *u;
		return r;
	}
	r->l = PMIN(r->l, u->l);
	r->t = PMIN(r->t, u->t);
	r->r = PMAX(r->r, u->r);
	r->b = PMAX(r->b, u->b);
	return r;
}

static c2_rect_t
c2_rect_left_of(
		c2_rect_t * r,
		int mark,
		int margin)
{
	c2_rect_offset(r, -r->l + mark - c2_rect_width(r) - margin, 0);
	return *r;
}
static c2_rect_t
c2_rect_right_of(
		c2_rect_t * r,
		int mark,
		int margin)
{
	c2_rect_offset(r, -r->l + mark + margin, 0);
	return *r;
}
static c2_rect_t
c2_rect_top_of(
		c2_rect_t * r,
		int mark,
		int margin)
{
	c2_rect_offset(r, 0, -r->t + mark - c2_rect_height(r) - margin);
	return *r;
}
static c2_rect_t
c2_rect_bottom_of(
		c2_rect_t * r,
		int mark,
		int margin)
{
	c2_rect_offset(r, 0, -r->t + mark + margin);
	return *r;
}

#endif /* C2_GEOMETRY_INLINE_H_ */
