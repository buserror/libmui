/*
 * c2_geometry.c
 *
 * C2DGeometry Implementation
 * Created:	Monday, March 30, 1998 11:51:47
 * Converted back to C99 May 2013
 *
 * Copyright (C) 1998-2023 Michel Pollet <buserror@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 */
#include <stdio.h>
#include "c2_geometry.h"
#include "c2_geometry_poly.h"

const char *
c2_rect_as_str(
		const c2_rect_p r )
{
	static char ret[8][32];
	static int  reti = 0;
	char *d = ret[reti];
	reti = (reti+1) & 7;
	if (r)
		sprintf(d, "[%d,%d,%d,%d]",r->v[0],r->v[1],r->v[2],r->v[3]);
	else strcpy(d, "[NULL]");
	return d;
}

uint8_t
c2_rect_get_next_edge(
		uint8_t	inEdge,
		int	inCW )
{
	uint8_t ret = inCW ? (inEdge << 1) & 0xf : (inEdge >> 1) & 0xf;
	return ret ? ret : inCW ? out_Left : out_Bottom;
}

uint8_t
c2_rect_is_on_edge(
		const c2_rect_p r,
		const c2_pt_p p )
{
	if (p->v[X] == r->tl.v[X])
		return out_Left;
	if (p->v[Y] == r->tl.v[Y])
		return out_Top;
	if (p->v[X] == r->br.v[X])
		return out_Right;
	if (p->v[Y] == r->br.v[Y])
		return out_Bottom;
	return 0;
}

int
c2_rect_get_edge(
		const c2_rect_p r,
		uint8_t inEdge,
		c2_segment_p outEdge )
{
	switch (inEdge) {
		case out_Left:
			c2_segment_set(outEdge, r->tl.v[X], r->tl.v[Y], r->tl.v[X], r->br.v[Y]);
			break;
		case out_Top:
			c2_segment_set(outEdge, r->tl.v[X], r->tl.v[Y], r->br.v[X], r->tl.v[Y]);
			break;
		case out_Right:
			c2_segment_set(outEdge, r->br.v[X], r->tl.v[Y], r->br.v[X], r->br.v[Y]);
			break;
		case out_Bottom:
			c2_segment_set(outEdge, r->tl.v[X], r->br.v[Y], r->br.v[X], r->br.v[Y]);
			break;
		default:
			return -1;
	}
	return 0;
}

int
c2_rect_get_corner(
		const c2_rect_p r,
		uint8_t inCorner,
		c2_pt_p outCorner,
		int inCW  )
{
	uint8_t corner = inCW ? inCorner : c2_rect_get_next_edge(inCorner, 0);
	switch (corner) {
		case corner_TopLeft:
			*outCorner = r->tl;
			break;
		case corner_TopRight:
			outCorner->v[X] = r->br.v[X];
			outCorner->v[Y] = r->tl.v[Y];
			break;
		case corner_BottomRight:
			*outCorner = r->br;
			break;
		case corner_BottomLeft:
			outCorner->v[X] = r->tl.v[X];
			outCorner->v[Y] = r->br.v[Y];
			break;
		default:
			return -1;
	}
	return 0;
}


int
c2_rect_clip_segment(
		const c2_rect_p r,
		c2_segment_p s,
		c2_segment_p o,
		char * outEdges )
{
	int	accept = 0,	done = 0;
	uint8_t	outcode0 = c2_rect_get_out_code(r, &s->a);
	uint8_t	outcode1 = c2_rect_get_out_code(r, &s->b);
	*o = *s;
	do {
		if (!(outcode0 | outcode1)) {
			accept = 1;	done = 1;
			continue;
		} else if ((outcode0 & outcode1) != 0) {
			if (outEdges) {
				outEdges[0] = outcode0;
				outEdges[1] = outcode1;	// return offending borders
			}
			done = 1;			// don't accept
			continue;
		}

		// at least one end is outside, pick it
		c2_pt_t *p;
		uint8_t 	*outcode;
		uint8_t 	dummy;
		uint8_t	*edge = &dummy;
		if (outcode0) {
			outcode = &outcode0;
			p = &o->a;
			if (outEdges) edge = &edge[0];
		} else {
			outcode = &outcode1;
			p = &o->b;
			if (outEdges) edge = &edge[1];
		}
		if (*outcode & out_Top) {
			p->v[X] = s->a.v[X] +
					(s->b.v[X] - s->a.v[X]) *
					(double)(r->tl.v[Y] - s->a.v[Y]) / (s->b.v[Y] - s->a.v[Y]);
			p->v[Y] = r->tl.v[Y];
			*edge = out_Top;
			*outcode &= ~out_Top;
		} else if (*outcode & out_Bottom) {
			p->v[X] = s->a.v[X] + (s->b.v[X] - s->a.v[X]) *
					(double)(r->br.v[Y] - s->a.v[Y]) / (s->b.v[Y] - s->a.v[Y]);
			p->v[Y] = r->br.v[Y];
			*edge = out_Bottom;
			*outcode &= ~out_Bottom;
		}
		if (*outcode & out_Left) {
			p->v[Y] = s->a.v[Y] + (s->b.v[Y] - s->a.v[Y]) *
					(double)(r->tl.v[X] - s->a.v[X]) / (s->b.v[X] - s->a.v[X]);
			p->v[X] = r->tl.v[X];
			*edge = out_Left;
			*outcode &= ~out_Left;
		} else if (*outcode & out_Right) {
			p->v[Y] = s->a.v[Y] + (s->b.v[Y] - s->a.v[Y]) *
					(double)(r->br.v[X] - s->a.v[X]) / (s->b.v[X] - s->a.v[X]);
			p->v[X] = r->br.v[X];
			*edge = out_Right;
			*outcode &= ~out_Right;
		}
		*outcode = c2_rect_get_out_code(r, p);
		*outcode = 0;
	} while (!done);

	return accept;
}

// C2DRectangle::Intersect(
int
c2_rect_clip_rect(
		const c2_rect_p r,
		const c2_rect_p s,
		c2_rect_p o )
{
	uint8_t	outcode0 = c2_rect_get_out_code(r, &s->tl);
	uint8_t	outcode1 = c2_rect_get_out_code(r, &s->br);

	// if both corners are on the same 'side' as the
	// other corner, there can't be any intersection anyway
	if (outcode0 & outcode1) {
		o->br = o->tl;// make sure result rect is empty
		return 0;
	}
	// ^^ this is equivalent to testing for all corners:
	// for (int c = out_Left; c <= out_Bottom; c <<= 1)
	// 	if ((outcode0 & c) && (outcode1 & c))
	//		return 0;

	/* here we /know/ the rectangle intersects, or contains the
	 * other, so just detect where the corners are in relation to
	 * the rectangle, and copy the 'clipped' coordinates over
	 */
	*o = *s;
	if (outcode0 & out_Left)
		o->tl.x = r->tl.x;
	if (outcode0 & out_Top)
		o->tl.y = r->tl.y;
	if (outcode1 & out_Right)
		o->br.x = r->br.x;
	if (outcode1 & out_Bottom)
		o->br.y = r->br.y;
	// only return 1 is the result is a true
	// rectangle. adjacent rectangles would
	// return a 'valid' intersection with with
	// width or height being 0
	return c2_rect_width(o) && c2_rect_height(o);
}

//! return true if all corners of 'r2' are inside 'r1'
int
c2_rect_contains_rect(
		const c2_rect_p r1,
		const c2_rect_p r2 )
{
	for (int c = corner_TopLeft; c <= corner_BottomLeft; c <<= 1) {
		c2_pt_t p = {};
		c2_rect_get_corner(r2, c, &p, 1);
		if (!c2_rect_contains_pt(r1, &p))
			return 0;
	}
	return 1;
}

// Return true if rectangle 'r' intersects with rectangle 's'
int
c2_rect_intersect_rect(
		const c2_rect_p s,
		const c2_rect_p r )
{
	// if any of the corners are inside, we are intersecting anyway
	for (int c = corner_TopLeft; c <= corner_BottomLeft; c <<= 1) {
		c2_pt_t p = {};
		c2_rect_get_corner(r, c, &p, 1);
		if (c2_rect_contains_pt(s, &p))
			return 1;
	}
	// if any of the edges of 'r' intersects 's', we intersect
	for (int e = out_Left; e <= out_Bottom; e <<= 1) {
		c2_segment_t seg, clip;
		c2_rect_get_edge(r, e, &seg);
		if (c2_rect_clip_segment(s, &seg, &clip, NULL))
			return 1;
	}
	// if 'r' *contains* 's', we intersect
	return c2_rect_contains_rect(r, s);
}

uint8_t
c2_rect_get_out_code(
		const c2_rect_p r,
		const c2_pt_p p )
{
	return ((p->v[X] < r->tl.v[X] ?		out_Left	: (p->v[X] > r->br.v[X]) ?		out_Right	: 0) |
			(p->v[Y] < r->tl.v[Y] ?		out_Top		: (p->v[Y] > r->br.v[Y]) ?		out_Bottom	: 0));
}

void
c2_rect_clip_pt(
		const c2_rect_p r,
		c2_pt_p	p )
{
	uint8_t o = c2_rect_get_out_code(r, p);
	if (o & out_Left)
		p->v[X] = r->tl.v[X];
	else
		p->v[X] = r->br.v[X];
	if (o & out_Top)
		p->v[Y] = r->tl.v[Y];
	else
		p->v[Y] = r->br.v[Y];
}


void
c2_polyline_clear(
		c2_polyline_p pl)
{
	const c2_rect_t z = C2_RECT_ZERO;
	pl->bounds = z;
	c2_pt_array_clear(&pl->pt);
}

//c2_polyline_t::GetSegment(
int
c2_polyline_get_segment(
		c2_polyline_p pl,
		unsigned long ind,
		c2_segment_p o )
{
	if (ind > pl->pt.count)
		return 0;
	o->a = pl->pt.e[ind];
	o->b = pl->pt.e[ind == pl->pt.count ? 1 : ind];
	return 1;
}

void
c2_polyline_offset(
		c2_polyline_p pl,
		c2_coord_t inX,
		c2_coord_t inY )
{
	for (unsigned int i = 0; i < pl->pt.count; i++) {
		pl->pt.e[i].x += inX;
		pl->pt.e[i].y += inY;
	}
	c2_rect_offset(&pl->bounds, inX, inY);
}

void
c2_polyline_scale(
		c2_polyline_p pl,
		double inFactor,
		c2_rect_p inSkip /* = NULL */ )
{
	long		pMax 		= pl->pt.count;
	c2_pt_t	* 	c			= pl->pt.e;
	long		count		= 0;
	long		total		= pMax;
	int			testrect	= inSkip != NULL;

	c2_rect_scale(&pl->bounds, inFactor);

	if (c2_rect_height(&pl->bounds) > 3 && c2_rect_width(&pl->bounds) > 3 && total >= 8) {
		c2_pt_t	delta = C2_PT_ZERO;
		c2_pt_t	o = C2_PT_ZERO;
		short		skip = 11;
		c2_pt_t	*d			= c;
		c2_pt_t	orig;

		for (int i = 0; i < total; i++, c++) {
			orig = *c;
			c2_pt_scale(c, inFactor);
			delta.v[X] += c->v[X] - o.v[X];
			delta.v[Y] += c->v[Y] - o.v[Y];
			int 	add =  0;

			if (testrect)
				add = 	orig.v[X] == inSkip->tl.v[X] || orig.v[X] == inSkip->br.v[X] ||
						orig.v[Y] == inSkip->tl.v[Y] || orig.v[X] == inSkip->br.v[Y];

			if (!add)
				add = 	skip > 10 || delta.v[X] > 3 || delta.v[X] < -3 ||
						delta.v[Y] > 3 || delta.v[Y] < -3;
			if (add) {
				*d++ = *c;
				count++;
				delta.v[X] = delta.v[Y] = 0;
				skip = 0;
			} else
				skip++;
			o = *c;
		}
		if (count < 2) {
			c	= pl->pt.e;
			c[0] = pl->bounds.tl;
			c[1] = pl->bounds.br;
			count = 2;
		}
		if (count & 1) {
			*d++ = o;
			count++;
		}
	} else {
		for (int i = 1; i <= total; i++, c++)
			c2_pt_scale(c, inFactor);
	}

	pl->pt.count = count;
	// TODO: trim?
}

void
c2_polyline_add_pt(
		c2_polyline_p pl,
		c2_pt_p p )
{
	if (!pl->pt.count) {
		pl->bounds.tl = *p;
		pl->bounds.br = *p;
	} else {
		pl->bounds.tl.v[X] = PMIN(pl->bounds.tl.v[X], p->v[X]);
		pl->bounds.tl.v[Y] = PMIN(pl->bounds.tl.v[Y], p->v[Y]);
		pl->bounds.br.v[X] = PMAX(pl->bounds.br.v[X], p->v[X]);
		pl->bounds.br.v[Y] = PMAX(pl->bounds.br.v[Y], p->v[Y]);
	}
	c2_pt_array_add(&pl->pt, *p);
}

#if 0
void
c2_polyline_t::Frame()
{
	Lock();
	SInt32		pMax		= GetCount();
	c2_pt_t	*c			= (c2_pt_t*)GetItemPtr(1);
	MoveTo(Pixel_32k(c->v[X]), Pixel_32k(c->v[Y]));
	for (int i = 1; i <= GetCount(); i++, c++) {
		if (i == 1)
			i = 1;
		LineTo(Pixel_32k(c->v[X]), Pixel_32k(c->v[Y]));
	}
	Unlock();
}
#endif

void
c2_polyline_array_break(
		c2_polyline_array_p pa)
{
	pa->current = NULL;
}

void
c2_polyline_array_add_pt(
		c2_polyline_array_p pa,
		c2_pt_p p )
{
	if (!pa->current) {
		pa->current = malloc(sizeof(c2_polyline_t));
		memset(pa->current, 0, sizeof(*pa->current));
		c2_polyline_array_add(pa, pa->current);
	}
	c2_polyline_add_pt(pa->current, p);
}

int
c2_polyline_array_clip(
		c2_polyline_array_p pa,
		c2_rect_p clip,
		c2_polyline_array_p outPoly )
{
	for (unsigned long poly = 0; poly < pa->count; poly++) {
		c2_polyline_t *p = pa->e[poly];
		if (!p)
			break;

		long		pMax		= p->pt.count;
		c2_pt_t	*	last		= p->pt.e;
		int			lastIn 		= c2_rect_contains_pt(clip, last);
		c2_pt_t	*	current		= last+1;
		int			currentIn	= c2_rect_contains_pt(clip, current);

		for (long pIndex = 2; pIndex <= pMax; pIndex++) {
			if (lastIn && currentIn) {			// both points are IN
				//outPoly.AddPoint(*last);
				c2_polyline_array_add_pt(outPoly, current);
			} else if (lastIn && !currentIn) {	// line goes OUT
				c2_segment_t	src = { .a = *last, .b = *current };
				c2_segment_t	dst;
				c2_rect_clip_segment(clip, &src, &dst, NULL);
				c2_polyline_array_add_pt(outPoly, &dst.b);
				c2_polyline_array_break(outPoly);
			} else if (!lastIn && currentIn) {	// line goes IN
				c2_segment_t	src = { .a = *last, .b = *current };
				c2_segment_t	dst;
				c2_rect_clip_segment(clip, &src, &dst, NULL);
				c2_polyline_array_break(outPoly);
				c2_polyline_array_add_pt(outPoly, &dst.a);
				c2_polyline_array_add_pt(outPoly, &dst.b);
			} else {								// line outside
				c2_segment_t	src = { .a = *last, .b = *current };
				c2_segment_t	dst;
				if (c2_rect_clip_segment(clip, &src, &dst, NULL)) {		// both point are out, but crosses the rectangle
					c2_polyline_array_break(outPoly);
					c2_polyline_array_add_pt(outPoly, &dst.a);
					c2_polyline_array_add_pt(outPoly, &dst.b);
				}
			}
			last++;
			current++;
			lastIn 		= c2_rect_contains_pt(clip, last);
			currentIn	= c2_rect_contains_pt(clip, current);
		}

	}
	return 0;
}


void
c2_polyline_array_scale(
		c2_polyline_array_p pa,
		float inFactor,
		c2_rect_p inSkip /* = NULL */ )
{
	for (unsigned int ind = 0; ind < pa->count; ind++) {
		c2_polyline_scale(pa->e[ind], inFactor, inSkip);
	}
}

void
c2_polyline_array_offset(
		c2_polyline_array_p pa,
		c2_coord_t inX,
		c2_coord_t inY )
{
	for (unsigned int ind = 0; ind < pa->count; ind++) {
		c2_polyline_offset(pa->e[ind], inX, inY);
	}
}

void
c2_scanline_array_proper_alloc(
		c2_scanline_array_p a,
		c2_coord_t height)
{
	c2_scanline_array_realloc(a, height);
	memset(a->e, 0, height * sizeof(a->e[0]));
}

void
c2_scanline_array_proper_clear(
		c2_scanline_array_p a)
{
	for (unsigned int i = 0; i < a->count; i++)
		c2_coord_array_free(&a->e[i]);
}

void
c2_scanline_array_add_coord(
		c2_scanline_array_p a,
		int inY,
		c2_coord_t inX )
{
	if (inY < 0 || inY >= (int)a->count)	return;

	c2_coord_array_p l = &a->e[inY];

	if (l->count == 0 || inX >= l->e[l->count-1]) {
		c2_coord_array_add(l, inX);
		return;
	}

	int num	= l->count;
	int 		ind	= 0;
	c2_coord_t *	cur = l->e;

	while (*cur < inX && num--) {
		cur++;
		ind++;
	}
	c2_coord_array_insert(l, ind, &inX, 1);
}

int
c2_polygon_isempty(
		c2_polygon_p pl)
{
	return !pl->pt.count || c2_rect_isempty(&pl->bounds);
}

c2_coord_t
c2_polygon_get_heigth(
		c2_polygon_p pl)
{
	return c2_rect_height(&pl->bounds);
}

void
c2_polygon_clip(
		c2_polygon_p pl,
		c2_rect_p clip,
		c2_polygon_p outPoly )
{
	int	sect 		= c2_rect_intersect_rect(&pl->bounds, clip);
	int	contains 	= c2_rect_contains_rect(&pl->bounds, clip);
	if (!sect && !contains)
		return;		// don't even bother

	long		pMax		= pl->pt.count;
	c2_pt_t	*	last		= pl->pt.e;
	int			lastIn 		= c2_rect_contains_pt(clip, last);
	c2_pt_t	*	current		= last+1;
	int			currentIn	= c2_rect_contains_pt(clip, current);
	//char		outPart		= clip.GetOutCode(*last);
	uint8_t		edgesStack[40];
	short		edgesClock[40];
	long		edgesCount	= 0;

	char		edgesIndexes[13] =		// Bit valued to quadrant index
		{ 0, 1, 3, 2, 5, 0, 4, 0, 7, 8, 0, 0, 6 };

	// if we start outside, remember our quadrant
	if (!lastIn)
		edgesStack[edgesCount++] = edgesIndexes[c2_rect_get_out_code(clip, last)];
	if (lastIn)
		c2_polyline_add_pt(outPoly, last);
	for (long pIndex = 2; pIndex <= pMax; pIndex++) {
		if (lastIn && currentIn) {			// both points are IN
			c2_polyline_add_pt(outPoly, current);
		} else if (lastIn && !currentIn) {	// line goes OUT
			c2_segment_t	src = { .a = *last, .b = *current };
			c2_segment_t	dst;
			c2_rect_clip_segment(clip, &src, &dst, NULL);
			c2_polyline_add_pt(outPoly, &dst.b);
			edgesStack[edgesCount++] = edgesIndexes[c2_rect_is_on_edge(clip, &dst.b)];
		} else if (!lastIn && currentIn) {	// line goes IN
			// flush corner stack
			if (edgesCount > 1) {
				for (int c = 0; c < edgesCount; c++) {
					c2_pt_t	p;
					switch (edgesStack[c]) {
						case 2:	// topleft
							p = clip->tl;
							c2_polyline_add_pt(outPoly, &p);
							break;
						case 4:	// topright
							p.v[X] = clip->br.v[X];	p.v[Y] = clip->tl.v[Y];
							c2_polyline_add_pt(outPoly, &p);
							break;
						case 6:	// botright
							p = clip->br;
							c2_polyline_add_pt(outPoly, &p);
							break;
						case 8:	// botleft
							p.v[X] = clip->tl.v[X];	p.v[Y] = clip->br.v[Y];
							c2_polyline_add_pt(outPoly, &p);
							break;
					}
				}
			}
			edgesCount = 0;
			c2_segment_t	src = { .a = *last, .b = *current };
			c2_segment_t	dst;
			c2_rect_clip_segment(clip, &src, &dst, NULL);
			c2_polyline_add_pt(outPoly, &dst.a);
			c2_polyline_add_pt(outPoly, &dst.b);
		} else {								// line outside
			c2_segment_t	src = { .a = *last, .b = *current };
			c2_segment_t	dst;
			if (c2_rect_clip_segment(clip, &src, &dst, NULL)) {		// both point are out, but crosses the rectangle
				// flush corner stack
				if (edgesCount > 1) {
					for (int c = 0; c < edgesCount; c++) {
						c2_pt_t	p;
						switch (edgesStack[c]) {
							case 2:	// topleft
								p = clip->tl;
								c2_polyline_add_pt(outPoly, &p);
								break;
							case 4:	// topright
								p.v[X] = clip->br.v[X];	p.v[Y] = clip->tl.v[Y];
								c2_polyline_add_pt(outPoly, &p);
								break;
							case 6:	// botright
								p = clip->br;
								c2_polyline_add_pt(outPoly, &p);
								break;
							case 8:	// botleft
								p.v[X] = clip->tl.v[X];	p.v[Y] = clip->br.v[Y];
								c2_polyline_add_pt(outPoly, &p);
								break;
						}
					}
				}
				edgesCount = 0;
				c2_polyline_add_pt(outPoly, &dst.a);
				c2_polyline_add_pt(outPoly, &dst.b);
				edgesStack[edgesCount++] = edgesIndexes[c2_rect_is_on_edge(clip, &dst.b)];
			} else {							// setup edge stack properly
				unsigned char	clockTable[9] =
					{ 0, 0x0e, 0x1c, 0x38, 0x70, 0xe0, 0xc1, 0x83, 0x07 };
				uint8_t	o = edgesIndexes[c2_rect_get_out_code(clip, current)];
				if (o != edgesStack[edgesCount - 1]) {
					short	clock = clockTable[edgesStack[edgesCount - 1]] & (1 << (o-1)) ? 1 : -1;

					do {
						if (edgesCount == 1)
							edgesClock[edgesCount -1] = clock;

						if (clock == edgesClock[edgesCount -1]) {
							int c = edgesStack[edgesCount - 1];
							do {
								c += clock;
								if (c == 0) c = 8;
								else if (c == 9) c = 1;
								edgesClock[edgesCount] = clock;
								edgesStack[edgesCount++] = c;
#ifdef TRACELIB
								{
									static long ecount = 0;
									if (edgesCount > ecount) {
										ecount = edgesCount;
										printf("Max edgecount = %d\n", ecount);
									}
								}
								if (edgesCount == 40) {
									DebugStr("\p HELP!");
								}
#endif
							} while (c != o);
						} else	{	// change direction, remove points
							while (edgesCount > 1 && edgesStack[edgesCount - 1] != o)
								edgesCount--;
						}
					} while (o != edgesStack[edgesCount -1]);
				}
			}
		}
		last++;
		current++;
		lastIn 		= c2_rect_contains_pt(clip, last);
		currentIn	= c2_rect_contains_pt(clip, current);
	}
	// flush corner stack
	if (edgesCount > 1) {
		for (int c = 0; c < edgesCount; c++) {
			c2_pt_t	p;
			switch (edgesStack[c]) {
				case 2:	// topleft
					p = clip->tl;
					c2_polyline_add_pt(outPoly, &p);
					break;
				case 4:	// topright
					p.v[X] = clip->br.v[X];	p.v[Y] = clip->tl.v[Y];
					c2_polyline_add_pt(outPoly, &p);
					break;
				case 6:	// botright
					p = clip->br;
					c2_polyline_add_pt(outPoly, &p);
					break;
				case 8:	// botleft
					p.v[X] = clip->tl.v[X];	p.v[Y] = clip->br.v[Y];
					c2_polyline_add_pt(outPoly, &p);
					break;
			}
		}
	}
	edgesCount = 0;
}


#define exchange(t, p1, p2)	{ t e=p1;p1=p2;p2=e; }

void
c2_polygon_scanline(
		c2_polygon_p pl,
		c2_scanline_array_p ioList,
		c2_coord_t ymin )
{
	c2_coord_t			ysize = ioList->height;//TODO check if we meant the polygon height?
	if (ysize <= 0 || c2_rect_width(&pl->bounds) <= 0)
		return;

	c2_pt_t		*first = pl->pt.e;
	c2_pt_t		*sp1 = first;
	c2_pt_t		*sp2 = sp1+1;
	long		numPoints = pl->pt.count;
	//
	// Compute list of intersections between the vertex and the horizontal
	// lines. The lists are automaticaly sorted
	//
	do {
		if (numPoints == 1) sp2 = first;
		c2_pt_t	*p1 = sp1;
		c2_pt_t	*p2	= sp2;
		if (p2->v[Y] < p1->v[Y])
			exchange(c2_pt_t *, p1, p2);
		c2_coord_t	y1 = p1->v[Y];
		c2_coord_t	y2 = p2->v[Y];
		c2_coord_t	idy = y2 - y1;
		if ((y1 < ymin && y2 < ymin) ||
			((y1-ymin > ioList->height) && (y2-ymin > ioList->height)))
				continue;
		if (idy) {
			double x, dx;
			x = p1->v[X];
			dx = (p2->v[X] - p1->v[X]) / (double)idy;
			do {
				c2_coord_t ix = x;
				c2_scanline_array_add_coord(ioList, y1-ymin, x - ix > 0.5 ? ix+1 : ix);
				x += dx;
				y1++;
			} while (y1 < y2);
		} else {
			c2_scanline_array_add_coord(ioList, y1-ymin, p1->v[X]);
			c2_scanline_array_add_coord(ioList, y1-ymin, p2->v[X]);
		}
		sp1++;
		sp2++;
	} while(numPoints-- > 1);
}

#if 0
void
c2_polygon_t::Paint()
{
	c2_scanline_list_t	scan(Height()+1);
	AddIntersections(scan, bounds.tl.v[Y]);

	for (int i = 0; i < Height(); i++) {
		SInt32	num		= scan.Get(i).GetCount();
		if (num > 1) {
			c2_coord_t	*	cur = scan.Get(i);
			for (SInt32 ind = 1; ind <= num - 1; ind += 2, cur += 2) {
				if (cur[0] < cur[1]) {
					MoveTo(cur[0], bounds.tl.v[Y]+i);
					LineTo(cur[1], bounds.tl.v[Y]+i);
				}
			}
		}
	}
}
#endif
