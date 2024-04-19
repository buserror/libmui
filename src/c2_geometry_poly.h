/*
 * c2_geometry_poly.h
 *
 * Copyright (C) 2023 Michel Pollet <buserror@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef C2_GEOMETRY_POLY_H_
#define C2_GEOMETRY_POLY_H_

#include "c2_arrays.h"

typedef struct c2_polyline_t {
	c2_pt_array_t 	pt;
	c2_rect_t		bounds;
} c2_polyline_t, *c2_polyline_p;

DECLARE_C_ARRAY(c2_polyline_p, c2_polyline_array, 16,
		c2_polyline_p current; );
DECLARE_C_ARRAY(c2_coord_array_t, c2_scanline_array, 16,
		c2_coord_t height; );

IMPLEMENT_C_ARRAY(c2_scanline_array);
IMPLEMENT_C_ARRAY(c2_polyline_array);


void
c2_polyline_clear(
		c2_polyline_p pl);

int
c2_polyline_get_segment(
		c2_polyline_p pl,
		unsigned long ind,
		c2_segment_p o );

void
c2_polyline_offset(
		c2_polyline_p pl,
		c2_coord_t inX,
		c2_coord_t inY );

void
c2_polyline_scale(
		c2_polyline_p pl,
		double inFactor,
		c2_rect_p inSkip /* = NULL */ );

void
c2_polyline_add_pt(
		c2_polyline_p pl,
		c2_pt_p p );

int
c2_polyline_array_clip(
		c2_polyline_array_p pa,
		c2_rect_p clip,
		c2_polyline_array_p outPoly );
void
c2_polyline_array_break(
		c2_polyline_array_p pa);
void
c2_polyline_array_add_pt(
		c2_polyline_array_p pa,
		c2_pt_p p );

void
c2_polyline_array_scale(
		c2_polyline_array_p pa,
		float inFactor,
		c2_rect_p inSkip /* = NULL */ );
void
c2_polyline_array_offset(
		c2_polyline_array_p pa,
		c2_coord_t inX,
		c2_coord_t inY );


void
c2_scanline_array_proper_alloc(
		c2_scanline_array_p a,
		c2_coord_t height);
void
c2_scanline_array_proper_clear(
		c2_scanline_array_p a);
void
c2_scanline_array_add_coord(
		c2_scanline_array_p a,
		int inY,
		c2_coord_t inX );


typedef c2_polyline_p c2_polygon_p;

int
c2_polygon_isempty(
		c2_polygon_p pl);

c2_coord_t
c2_polygon_get_heigth(
		c2_polygon_p pl);


void
c2_polygon_clip(
		c2_polygon_p pl,
		c2_rect_p clip,
		c2_polygon_p outPoly );

void
c2_polygon_scanline(
		c2_polygon_p pl,
		c2_scanline_array_p ioList,
		c2_coord_t ymin );



#endif /* C2_GEOMETRY_POLY_H_ */
