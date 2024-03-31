/*
 * c2_arrays.c
 *
 *  Created on: 4 Feb 2023
 *      Author: michel
 */

#include "c2_arrays.h"

/*! Simplify array 'a' into 'b', return 1 if it was.
 * This takes a list of rectangles 'a', duplicates are skipped. If two rectangles
 * overlap, see if the union is 'worth' it. Returns results in array 'b'
 * Returns 1 if the b array has been simplified somehow, zero if not
 */
int
c2_rect_array_simplify(
		c2_rect_array_p a,
		c2_rect_array_p b)
{
	c2_rect_array_clear(b);
	for (unsigned int i = 0; i < a->count; i++) {
		int addit = 1;
		c2_rect_p ra = &a->e[i];
		for (unsigned int j = 0; j < b->count && addit; j++) {
			c2_rect_p rb = &b->e[j];
			if (c2_rect_equal(rb, ra)) {	// already a copy here
				addit = 0;
			} else if (c2_rect_contains_rect(rb, ra)) {
				addit = 0;
			} else if (c2_rect_intersect_rect(rb, ra)) {
				c2_rect_t o = *ra;
				c2_rect_union(&o, rb);
				long sa = c2_rect_surface_squared(ra);
				long sb = c2_rect_surface_squared(rb);
				long so = c2_rect_surface_squared(&o);
				if (so <= (sa + sb)) {
					*rb = o;
					addit = 0;
				}
			}
		}
		if (addit)
			c2_rect_array_add(b, *ra);
	}
	return b->count < a->count;
}
