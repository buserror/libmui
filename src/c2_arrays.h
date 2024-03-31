/*
 * c2_arrays.h
 *
 * Copyright (C) 2023 Michel Pollet <buserror@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 */

#ifndef C2_ARRAYS_H_
#define C2_ARRAYS_H_

#include "c2_geometry.h"
#include "c_array.h"

DECLARE_C_ARRAY(c2_pt_t, c2_pt_array, 16);
DECLARE_C_ARRAY(c2_coord_t, c2_coord_array, 16);
DECLARE_C_ARRAY(c2_rect_t, c2_rect_array, 16);

IMPLEMENT_C_ARRAY(c2_pt_array);
IMPLEMENT_C_ARRAY(c2_coord_array);
IMPLEMENT_C_ARRAY(c2_rect_array);

/*! Simplify array 'a' into 'b', return 1 if it was.
 * This takes a list of rectangles 'a', duplicates are skipped. If two rectangles
 * overlap, see if the union is 'worth' it. Returns results in array 'b'
 * Returns 1 if the b array has been simplified somehow, zero if not
 */
int
c2_rect_array_simplify(
		c2_rect_array_p a,
		c2_rect_array_p b);

#endif /* C2_ARRAYS_H_ */
