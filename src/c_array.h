/*
 * c_array.h
 *
 * Copyright 2012 Michel Pollet <buserror@gmail.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 * vi: ts=4
 */


#ifndef __C_ARRAY_H___
#define __C_ARRAY_H___

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifndef C_ARRAY_INLINE
#define C_ARRAY_INLINE inline
#endif
#ifndef C_ARRAY_SIZE_TYPE
#define C_ARRAY_SIZE_TYPE uint32_t
#endif
/* New compilers don't like unused static functions. However,
 * we do like 'static inlines' for these small accessors,
 * so we mark them as 'unused'. It stops it complaining */
#ifdef __GNUC__
#define C_ARRAY_DECL static  __attribute__ ((unused))
#else
#define C_ARRAY_DECL static
#endif


#define DECLARE_C_ARRAY(__type, __name, __page, __args...) \
enum { __name##_page_size = __page }; \
typedef __type __name##_element_t; \
typedef C_ARRAY_SIZE_TYPE __name##_count_t; \
typedef struct __name##_t {\
	volatile __name##_count_t count;\
	volatile __name##_count_t size;\
	__name##_element_t * e;\
	__args \
} __name##_t, *__name##_p;

#define C_ARRAY_NULL { 0, 0, NULL }

#ifndef NO_DECL
#define IMPLEMENT_C_ARRAY(__name) \
C_ARRAY_DECL C_ARRAY_INLINE \
	void __name##_init(\
			__name##_p a) \
{\
	static const __name##_t zero = {}; \
	if (!a) return;\
	*a = zero;\
}\
C_ARRAY_DECL C_ARRAY_INLINE \
	void __name##_free(\
			__name##_p a) \
{\
	if (!a) return;\
	if (a->e) free(a->e);\
	a->count = a->size = 0;\
	a->e = NULL;\
}\
C_ARRAY_DECL C_ARRAY_INLINE \
	void __name##_clear(\
			__name##_p a) \
{\
	if (!a) return;\
	a->count = 0;\
}\
C_ARRAY_DECL C_ARRAY_INLINE \
	void __name##_realloc(\
			__name##_p a, __name##_count_t size) \
{\
	if (!a || a->size == size) return; \
	if (size == 0) { if (a->e) free(a->e); a->e = NULL; } \
	else a->e = (__name##_element_t*)realloc(a->e, \
						size * sizeof(__name##_element_t));\
	a->size = size; \
}\
C_ARRAY_DECL C_ARRAY_INLINE \
	void __name##_trim(\
			__name##_p a) \
{\
	if (!a) return;\
	__name##_count_t n = a->count + __name##_page_size;\
	n -= (n % __name##_page_size);\
	if (n != a->size)\
		__name##_realloc(a, n);\
}\
C_ARRAY_DECL C_ARRAY_INLINE \
	__name##_element_t * __name##_get_ptr(\
			__name##_p a, __name##_count_t index) \
{\
	if (!a) return NULL;\
	if (index > a->count) index = a->count;\
	return index < a->count ? a->e + index : NULL;\
}\
C_ARRAY_DECL C_ARRAY_INLINE \
	__name##_count_t __name##_add(\
			__name##_p a, __name##_element_t e) \
{\
	if (!a) return 0;\
	if (a->count + 1 >= a->size)\
		__name##_realloc(a, a->size + __name##_page_size);\
	a->e[a->count++] = e;\
	return a->count;\
}\
C_ARRAY_DECL C_ARRAY_INLINE \
	__name##_count_t __name##_push(\
			__name##_p a, __name##_element_t e) \
{\
	return __name##_add(a, e);\
}\
C_ARRAY_DECL C_ARRAY_INLINE \
	int __name##_pop(\
			__name##_p a, __name##_element_t *e) \
{\
	if (a->count) { *e = a->e[--a->count]; return 1; } \
	return 0;\
}\
C_ARRAY_DECL C_ARRAY_INLINE \
	__name##_count_t __name##_insert(\
			__name##_p a, __name##_count_t index, \
			const __name##_element_t * e, __name##_count_t count) \
{\
	if (!a || !e || !count) return 0;\
	if (index > a->count) index = a->count;\
	if (a->count + count >= a->size) \
		__name##_realloc(a, (((a->count + count) / __name##_page_size)+1) * __name##_page_size);\
	if (index < a->count)\
		memmove(&a->e[index + count], &a->e[index], \
				(a->count - index) * sizeof(__name##_element_t));\
	memmove(&a->e[index], e, count * sizeof(__name##_element_t));\
	a->count += count;\
	return a->count;\
}\
C_ARRAY_DECL C_ARRAY_INLINE \
	__name##_count_t __name##_append(\
			__name##_p a, \
			const __name##_element_t * e, __name##_count_t count) \
{\
	if (!a) return 0;\
	return __name##_insert(a, a->count, e, count);\
}\
C_ARRAY_DECL C_ARRAY_INLINE \
	__name##_count_t __name##_delete(\
			__name##_p a, __name##_count_t index, __name##_count_t count) \
{\
	if (!a) return 0;\
	if (index > a->count) index = a->count;\
	if (index + count > a->count) \
		count = a->count - index;\
	if (count && a->count - index) { \
		memmove(&a->e[index], &a->e[index + count], \
				(a->count - index - count) * sizeof(__name##_element_t));\
	}\
	a->count -= count;\
	return a->count;\
}
#else /* NO_DECL */

#define IMPLEMENT_C_ARRAY(__name)

#endif

#endif /* __C_ARRAY_H___ */
