/*
 * c2_geometry.h
 *
 * C2DGeometry Implementation
 * Created:	Monday, March 30, 1998 11:51:47
 * Converted back to C99 May 2013
 *
 * Copyright (C) 1998-2023 Michel Pollet <buserror@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef __C2_GEOMETRY_H__
#define __C2_GEOMETRY_H__

#include <stdint.h>

#define		PMIN(x1,x2)	((x1 < x2) ? x1 : x2)
#define		PMAX(x1,x2)	((x1 > x2) ? x1 : x2)

typedef		int32_t				c2_coord_t;

enum {
	X			= 0, Y,
//	L			= 0, T, R, B,
};

//
// a basic x,y point
//
typedef union c2_pt_t {
	struct {
		c2_coord_t x, y;
	};
	c2_coord_t v[2];
} c2_pt_t;//, *c2_pt_p;
#define c2_pt_p union c2_pt_t*

#define C2_PT(_a, _b) (c2_pt_t){ .x = (_a), .y = (_b) }
#define C2_PT_ZERO C2_PT(0,0)

//
// a basic segment, a pair of points
//
typedef struct c2_segment_t {
	c2_pt_t 	a, b;
} c2_segment_t;//, *c2_segment_p;
#define c2_segment_p struct c2_segment_t*

#define C2_SEGMENT_ZERO { C2_PT_ZERO, C2_PT_ZERO }

//
// a rectangle defined by two points
//
typedef union c2_rect_t {
	struct {
		c2_pt_t	tl, br;
	};
	c2_coord_t v[4];
	struct {
		c2_coord_t l,t,r,b;
	};
} c2_rect_t;//, *c2_rect_p;
#define c2_rect_p union c2_rect_t*

#define C2_RECT(_l,_t,_r,_b) (c2_rect_t){  .l = _l, .t = _t, .r = _r, .b = _b  }
#define C2_RECT_WH(_l,_t,_w,_h) (c2_rect_t){  \
	.l = _l, .t = _t, .r = (_l)+(_w), .b = (_t)+(_h)  }
/* Return the squared surface of the rectangle, for area comparison sake */
#define c2_rect_surface_squared(_r) \
					((c2_rect_width(_r)*c2_rect_width(_r))+\
					(c2_rect_height(_r)*c2_rect_height(_r)))

#define C2_RECT_ZERO C2_RECT(0,0,0,0)

int
c2_rect_contains_rect(
		const c2_rect_p r1,
		const c2_rect_p r2 );

int
c2_rect_clip_segment(
		const c2_rect_p r,
		c2_segment_p s,
		c2_segment_p o,
		char	* outEdges );
int
c2_rect_clip_rect(
		const c2_rect_p r,
		const c2_rect_p s,
		c2_rect_p o );
int
c2_rect_intersect_rect(
		const c2_rect_p s,
		const c2_rect_p r );

enum {
	out_Left					= (1 << 0),
	out_Top						= (1 << 1),
	out_Right					= (1 << 2),
	out_Bottom					= (1 << 3),
};
enum {
	corner_TopLeft				= out_Left,
	corner_TopRight				= out_Top,
	corner_BottomRight			= out_Right,
	corner_BottomLeft			= out_Bottom,
};

uint8_t
c2_rect_get_out_code(
		const c2_rect_p r,
		const c2_pt_p p );
void
c2_rect_clip_pt(
		const c2_rect_p r,
		c2_pt_p	p );

uint8_t
c2_rect_get_next_edge(
		uint8_t inEdge,
		int inCW );
uint8_t
c2_rect_is_on_edge(
		const c2_rect_p r,
		const c2_pt_p p );
int
c2_rect_get_edge(
		const c2_rect_p r,
		uint8_t inEdge,
		c2_segment_p outEdge );
int
c2_rect_get_corner(
		const c2_rect_p r,
		uint8_t inCorner,
		c2_pt_p outCorner,
		int	inCW  );

const char *
c2_rect_as_str(
		const c2_rect_p r );

#ifndef C2_GEOMETRY_INLINE_H_
#ifndef C2_DECL
#include "c2_geometry_inline.h"
#endif
#endif
#endif
