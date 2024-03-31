/*
 * stb_ttc.h
 *
 * Copyright (C) 2020 Michel Pollet <buserror@gmail.com>
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#ifndef STB_TTC_H_
#define STB_TTC_H_

#include "stb_truetype.h"

#define STB_TTC_PACKED __attribute__((packed))
/* Number of bins in the hash table(s) */
#define STB_TTC_BINCOUNT		16
/* Number of structures we prealocate when resizing arrays */
#define STB_TTC_PAGESIZE		16

/*
 * This is used in the hash table mapping the glyph/scale pair to the
 * primary array of cached glyphs.
 */
typedef struct stb_ttc_index {
	unsigned int 	intscale, glyph, index;
} STB_TTC_PACKED stb_ttc_index;

/*
 * This is used by MeasureText. Returns the number os glyphs in the string,
 * the ascent and descent /of that text/ and the bounding box of that text.
 * Note that x0 can be negative, if the first character is for example, a 'j'
 */
typedef struct stb_ttc_measure {
	short		glyph_count;
	short		ascent, descent;
	short 		x0,y0,x1,y1;
} STB_TTC_PACKED stb_ttc_measure;

/*
 * A glyph info for a certain scale. You can have the same glyph at multiple
 * scales in one cache.
 */
typedef struct stb_ttc_g {
	unsigned int 	index;		// index in global table
	unsigned int	intscale;	// for comparison purpose
	float			scale;
	unsigned int 	glyph;
	int 			advance, lsb;
	short 			x0,y0,x1,y1;
	// position in the pixel cache, or 0xffff if not yet cached.
	unsigned short	p_x, p_y;
} STB_TTC_PACKED stb_ttc_g;

/*
 * Used to cache the codepoints to glyphs map in a hash table
 */
typedef struct stb_ttc_cp_gl {
	unsigned int	cp;
	unsigned int	glyph;
} STB_TTC_PACKED stb_ttc_cp_gl;

/*
 * Used to cache the kerning between cp1 and cp2 in a hash
 */
typedef struct stb_ttc_cp_kern {
	unsigned int	hash;
	unsigned int	cp1, cp2;
	int				kern;
} STB_TTC_PACKED stb_ttc_cp_kern;

// to remap codepoint->glyph without calling back into stb
struct _stb_ttc_cp_bin {
	unsigned int	cp_count;
	stb_ttc_cp_gl *	cp_gl;
};
// to remap codepoint pairs to associated kerning
struct _stb_ttc_kn_bin {
	unsigned int	kn_count;
	stb_ttc_cp_kern *	cp_kn;
};
// for glyph -> position in the glyph array
struct _stb_ttc_g_bin {
	unsigned int    i_count;
	stb_ttc_index *	index;
};

/*
 * Main STB TrueType Cache structure.
 */
typedef struct stb_ttc_info {
	stbtt_fontinfo  font;
	unsigned int	font_size;
	unsigned 		font_mmap: 1; // is font from a file
	int				ascent, descent;
	// hash to remap codepoint->glyph without calling back into stb
	struct _stb_ttc_cp_bin	cp_bin[STB_TTC_BINCOUNT];
	// hash to remap codepoint pairs to associated kerning
	struct _stb_ttc_kn_bin	kn_bin[STB_TTC_BINCOUNT];
	// hash that for glyph -> position in the glyph array
	struct _stb_ttc_g_bin	g_bin[STB_TTC_BINCOUNT];
	// glyph array, contains glyph dimensions, and x,y of pixel in cache
	unsigned int    g_count;
	stb_ttc_g *      glyph;
	// pixels cache for glyphs
	unsigned int    p_stride;
	unsigned int    p_height;	// current total height
	unsigned int	p_line_height;	// current glyph line height
	unsigned int	p_line_x, p_line_y;	// current x,y position in pixels
	unsigned char *	pixels;
} stb_ttc_info;


#ifdef STBTTC_STATIC
#define STBTTC_DEF static
#else
#define STBTTC_DEF extern
#endif

/*
 * Preload a range of codepoints into the glyph cache. This doesn't add the
 * pixels, it just load the glyph measurement, advance, lsb into the cache
 */
STBTTC_DEF int
stb_ttc_CacheCodepointRange(
		struct stb_ttc_info * ttc,
		unsigned int cp,
		unsigned int count,
		float scale );
/*
 * This walks the cache for glyphs that haven't been pre-rendered and
 * render them in the cache. Also, it attempts to sort the glyphs by height
 * into the cache to save a little bit of memory
 * Note: This call is optional, the glyph renderer will populate the pixel
 * cache lazily on the fly.
 */
STBTTC_DEF int
stb_ttc_RenderAllCachedGlyphs(
		struct stb_ttc_info * ttc );

STBTTC_DEF int
stb_ttc_MeasureText(
		struct stb_ttc_info * ttc,
		float scale,
		const char * text,
		stb_ttc_measure *out);

STBTTC_DEF int
stb_ttc_DrawText(
		struct stb_ttc_info * ttc,
		float 			scale,
		const char * 	text,
		unsigned int	dx,
		unsigned int	base_dy,
		unsigned char * pixels,
		unsigned int 	p_w,
		unsigned int 	p_h,
		unsigned int 	p_stride);
// load a font from a file
STBTTC_DEF int
stb_ttc_MapFont(
		struct stb_ttc_info * ttc,
		const char * font_file);
// load a font from memory
STBTTC_DEF int
stb_ttc_LoadFont(
		struct stb_ttc_info * ttc,
		const void * font_data,
		unsigned int font_size);
STBTTC_DEF void
stb_ttc_Free(
		struct stb_ttc_info * ttc);

#ifdef STB_TTC_IMPLEMENTATION
/*
 *	return font glyph index from codepoint, cache the result if it wasn't.
 */
static int
stb_ttc__CodepointGetGlyph(
		struct stb_ttc_info *fi,
		unsigned int cp )
{
	struct _stb_ttc_cp_bin *b = &fi->cp_bin[cp & (STB_TTC_BINCOUNT - 1)];
	int di = 0;
	for (unsigned int gi = 0; gi < b->cp_count; gi++, di++)
		if (b->cp_gl[gi].cp == cp)
			return b->cp_gl[gi].glyph;
		else if (b->cp_gl[gi].cp > cp)
			break;
	int gl = stbtt_FindGlyphIndex(&fi->font, cp);
	if (gl == 0)
		return -1;
	if (!(b->cp_count % STB_TTC_PAGESIZE))
		b->cp_gl = realloc(b->cp_gl,
				sizeof(b->cp_gl[0]) * (b->cp_count + STB_TTC_PAGESIZE));
	memmove(b->cp_gl + di + 1, b->cp_gl + di,
			sizeof(b->cp_gl[0]) * (b->cp_count - di));
	b->cp_count++;
	b->cp_gl[di].cp = cp;
	b->cp_gl[di].glyph = gl;
	return gl;
}

/*
 *  return kerning for 2 codepoints, cache the result.
 */
static int
stb_ttc__CodepointsGetKerning(
		struct stb_ttc_info *fi,
		unsigned int cp1,
		unsigned int cp2 )
{
	unsigned int hash = cp1 + ((cp1 * 100) * cp2);
	struct _stb_ttc_kn_bin *b = &fi->kn_bin[hash & (STB_TTC_BINCOUNT - 1)];
	int di = 0;
	for (unsigned int gi = 0; gi < b->kn_count; gi++, di++)
		if (b->cp_kn[gi].hash == hash &&
				b->cp_kn[gi].cp1 == cp1 &&
				b->cp_kn[gi].cp2 == cp2)
			return b->cp_kn[gi].kern;
		else if (b->cp_kn[gi].hash > hash)
			break;
	int kern = stbtt_GetCodepointKernAdvance(&fi->font, cp1, cp2);

	if (!(b->kn_count % STB_TTC_PAGESIZE))
		b->cp_kn = realloc(b->cp_kn,
				sizeof(b->cp_kn[0]) * (b->kn_count + STB_TTC_PAGESIZE));
	memmove(b->cp_kn + di + 1, b->cp_kn + di,
			sizeof(b->cp_kn[0]) * (b->kn_count - di));
	stb_ttc_cp_kern k = {
			.hash = hash,
			.cp1 = cp1,
			.cp2 = cp2,
			.kern = kern,
	};
	b->kn_count++;
	b->cp_kn[di] = k;
	return kern;
}

/*
 *  returns the index of glyph in the glyph table, or -1
 */
static int
stb_ttc__ScaledGlyphGetOffset(
		struct stb_ttc_info *fi,
		unsigned int glyph,
		float scale )
{
	unsigned int intscale = 1.0f / scale * 1000;
	unsigned int hash = glyph + (glyph * intscale);
	struct _stb_ttc_g_bin *b = &fi->g_bin[hash & (STB_TTC_BINCOUNT - 1)];
	for (unsigned int gi = 0; gi < b->i_count; gi++)
		if (b->index[gi].intscale == intscale &&
				b->index[gi].glyph == glyph)
			return b->index[gi].index;
		else if (b->index[gi].glyph > glyph)
			break;
	return -1;
}

static struct stb_ttc_g *
stb_ttc__ScaledGlyphGetCache(
		struct stb_ttc_info *ttc,
		unsigned int glyph,
		float scale )
{
	if (glyph == (unsigned int)-1)
		return NULL;
	int cached_index = stb_ttc__ScaledGlyphGetOffset(ttc, glyph, scale);
	if (cached_index != -1)
		return &ttc->glyph[cached_index];

	stb_ttc_g gc = { };

	gc.glyph = glyph;
	gc.intscale = 1.0f / scale * 1000;
	gc.scale = scale;
	gc.p_x = gc.p_y = -1; // not initialised yet
	// we use locals, as we are storing values in shorter types
	{
		int advance, lsb, x0, y0, x1, y1;
		stbtt_GetGlyphHMetrics(&ttc->font, glyph, &advance, &lsb);
		stbtt_GetGlyphBitmapBox(&ttc->font, glyph, scale, scale, &x0, &y0, &x1,
				&y1);
		gc.advance = advance;
		gc.lsb = lsb;
		gc.x0 = x0;
		gc.y0 = y0;
		gc.x1 = x1;
		gc.y1 = y1;
	}
	if (!(ttc->g_count % STB_TTC_PAGESIZE))
		ttc->glyph = realloc(ttc->glyph,
				sizeof(ttc->glyph[0]) * (ttc->g_count + STB_TTC_PAGESIZE));
	stb_ttc_index gh = {
			.intscale = gc.intscale,
			.glyph = glyph,
			.index = ttc->g_count
	};
	gc.index = ttc->g_count;
	ttc->g_count++;

	unsigned int hash = glyph + (glyph * gc.intscale);
	struct _stb_ttc_g_bin *b = &ttc->g_bin[hash & (STB_TTC_BINCOUNT - 1)];

	if (!(b->i_count % STB_TTC_PAGESIZE))
		b->index = realloc(b->index,
				sizeof(b->index[0]) * (b->i_count + STB_TTC_PAGESIZE));
	unsigned int di = b->i_count;
	for (unsigned int i = 0; i < b->i_count; i++)
		if (b->index[i].glyph > glyph) {
			di = i;
			break;
		}
	if (b->i_count - di)
		memmove(&b->index[di + 1], &b->index[di],
				sizeof(b->index[0]) * (b->i_count - di));
	b->index[di] = gh;
	b->i_count++;
	ttc->glyph[gh.index] = gc;
	return &ttc->glyph[gh.index];
}

static void
stb_ttc__ScaledGlyphRenderToCache(
		struct stb_ttc_info *fi,
		struct stb_ttc_g *g )
{
	int wt = g->x1 - g->x0;
	wt = (wt + 3) & ~3;
	unsigned int ht = g->y1 - g->y0;
	// find the new horizontal position for glyph, "wrap" next line if needed
	if (fi->p_line_x + wt > fi->p_stride) {
		fi->p_line_x = 0;
		fi->p_line_y += fi->p_line_height;
		fi->p_line_height = 0;
	}
	if (ht > fi->p_line_height)
		fi->p_line_height = ht;
	// reallocate the pixel cache to accommodate more lines, if needed
	if (fi->p_line_y + ht > fi->p_height) {
		int add = fi->p_line_y + ht - fi->p_height;
		fi->pixels = realloc(fi->pixels, (fi->p_height + add) * fi->p_stride);
		memset(fi->pixels + (fi->p_height * fi->p_stride), 0xff,
				add * fi->p_stride);
		fi->p_height += add;
	}
	g->p_x = fi->p_line_x;
	g->p_y = fi->p_line_y;
	stbtt_MakeGlyphBitmap(&fi->font,
			fi->pixels + (g->p_y * fi->p_stride) + g->p_x, g->x1 - g->x0,
			g->y1 - g->y0, fi->p_stride, g->scale, g->scale, g->glyph);
	fi->p_line_x += wt;
}

static void
stb_ttc__GlyphRenderFromCache(
		struct stb_ttc_info * fi,
		struct stb_ttc_g *	g,
		unsigned int	dx,
		unsigned int	base_dy,
		unsigned char * pixels,
		unsigned int 	p_w,
		unsigned int 	p_h,
		unsigned int 	p_stride)
{
	int _dy = base_dy + g->y0;
	int _sy = 0;

	if (_dy >= (int)p_h || dx >= p_w || (int)base_dy + (g->y1 - g->y0) < 0)
		return;
	unsigned char * src = fi->pixels + (g->p_y * fi->p_stride);

	// skip lines that would be over the top of dst pixels
	if (_dy < 0) {
		_sy -= _dy;
		src += (-_dy) * fi->p_stride;
		_dy = 0;
	}
	unsigned char * dst = pixels + (_dy * p_stride);	// beginning of line in pixels
	while (_dy < (int)p_h && _sy < (g->y1 - g->y0)) {
		int rw = g->x1 - g->x0;	// remaining width of glyph line
		int line_dx = (int)dx - g->x0;
		unsigned char * src_p = src + g->p_x;
		// skip what would be before the left border of pixels
		if (line_dx < 0) {
			src_p += -line_dx;
			rw += line_dx;
			line_dx = 0;
		}
		if (line_dx + rw >= (int)p_w)
			rw = p_w - line_dx;
		unsigned char * dst_p = dst + line_dx;
		for (; rw > 0; rw--) {
			unsigned short s = *dst_p + *src_p;
			unsigned char d = -(s >> 8) | (unsigned char)s;
			*dst_p++ = d;
			src_p++;
		}
		dst += p_stride;
		src += fi->p_stride;
		_dy++;
		_sy++;
	}
}

/*
 * Preload a range of codepoints into the glyph cache. This doesn't add the
 * pixels, it just load the glyph measurement, advance, lsb into the cache
 */
STBTTC_DEF int
stb_ttc_CacheCodepointRange(
		struct stb_ttc_info * ttc,
		unsigned int cp,
		unsigned int count,
		float scale )
{
	int res = 0;
	for (unsigned int c = cp; c < cp + count; c++) {
		int gl = stb_ttc__CodepointGetGlyph(ttc, c);
		if (gl == -1)
			continue;
		stb_ttc__ScaledGlyphGetCache(ttc, gl, scale);
		res++;
	}
	return res;
}

static  stb_ttc_info * _ttc;
static int
_compare(
		const void *a,
		const void *b)
{
	stb_ttc_info *ttc = _ttc;
	int g1 = *((int*)a), g2 = *((int*)b);
	return (((ttc->glyph[g1].y1 - ttc->glyph[g1].y0) * 500) + ttc->glyph[g1].glyph) -
			(((ttc->glyph[g2].y1 - ttc->glyph[g2].y0) * 500) + ttc->glyph[g2].glyph);
}

/*
 * This walks the cache for glyphs that haven't been pre-rendered and
 * render them in the cache. Also, it attempts to sort the glyphs by height
 * into the cache to save a little bit of memory
 * Note: This call is optional, the glyph renderer will populate the pixel
 * cache lazily on the fly.
 */
STBTTC_DEF int
stb_ttc_RenderAllCachedGlyphs(
		struct stb_ttc_info * ttc )
{
	// go on and sort the glyph indexes by height
	int * to_sort = malloc(ttc->g_count * sizeof(int));
	for (int i = 0; i < (int)ttc->g_count; i++)
		to_sort[i] = i;
	_ttc = ttc;
	qsort(to_sort, ttc->g_count, sizeof(int), _compare);
	int count = 0;
	// now load all the glyph pixels to the pixel cache
	for (int i = 0; i < (int)ttc->g_count; i++) {
		stb_ttc_g * g = &ttc->glyph[to_sort[i]];
		if (g->p_y == (unsigned short)-1) {
			stb_ttc__ScaledGlyphRenderToCache(ttc, &ttc->glyph[to_sort[i]]);
			count++;
		}
	}
	free(to_sort);
	return count;
}

// Copyright (c) 2008-2010 Bjoern Hoehrmann <bjoern@hoehrmann.de>
// See http://bjoern.hoehrmann.de/utf-8/decoder/dfa/ for details.
#define UTF8_ACCEPT 0
#define UTF8_REJECT 12

static inline unsigned int
stb_ttc__UTF8_Decode(
		unsigned int* state,
		unsigned int* codep,
		unsigned char byte)
{
	static const unsigned char utf8d[] = {
		// The first part of the table maps bytes to character classes that
		// to reduce the size of the transition table and create bitmasks.
		0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
		0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
		0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
		0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
		1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,  9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,
		7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,  7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,
		8,8,2,2,2,2,2,2,2,2,2,2,2,2,2,2,  2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,
		10,3,3,3,3,3,3,3,3,3,3,3,3,4,3,3, 11,6,6,6,5,8,8,8,8,8,8,8,8,8,8,8,

		// The second part is a transition table that maps a combination
		// of a state of the automaton and a character class to a state.
		0,12,24,36,60,96,84,12,12,12,48,72, 12,12,12,12,12,12,12,12,12,12,12,12,
		12, 0,12,12,12,12,12, 0,12, 0,12,12, 12,24,12,12,12,12,12,24,12,24,12,12,
		12,12,12,12,12,12,12,24,12,12,12,12, 12,24,12,12,12,12,12,12,12,24,12,12,
		12,12,12,12,12,12,12,36,12,36,12,12, 12,36,12,12,12,12,12,36,12,36,12,12,
		12,36,12,12,12,12,12,12,12,12,12,12,
	};
	unsigned int type = utf8d[byte];
	*codep = (*state != UTF8_ACCEPT) ?
				(byte & 0x3fu) | (*codep << 6) :
				(0xff >> type) & (byte);
	*state = utf8d[256 + *state + type];
	return *state;
}

STBTTC_DEF int
stb_ttc_MeasureText(
		struct stb_ttc_info * ttc,
		float scale,
		const char * text,
		stb_ttc_measure *out)
{
	unsigned int state = 0;
	int xpos = 0;
	unsigned int last = 0;
	stb_ttc_measure m = { .ascent = ttc->ascent * scale,
			.descent = ttc->descent * scale};
	unsigned int cp = 0;

	for (int ch = 0; text[ch]; ch++) {
		if (stb_ttc__UTF8_Decode(&state, &cp, text[ch]) != UTF8_ACCEPT)
			continue;
		if (last) {
			int kern = scale * stb_ttc__CodepointsGetKerning(ttc, last, cp);
			xpos += kern;
		}
		last = cp;
		int gl = stb_ttc__CodepointGetGlyph(ttc, cp);
		if (gl == -1)
			continue;
		stb_ttc_g *gc = stb_ttc__ScaledGlyphGetCache(ttc, gl, scale);
		if (!gc)
			continue;
		if (m.glyph_count == 0)
			m.x0 = gc->x0;
		if (gc->y0 < m.y0) m.y0 = gc->y0;
		if (gc->y1 > m.y1) m.y1 = gc->y1;
		m.glyph_count++;
		xpos += gc->advance;
	}
	m.x1 = xpos * scale;
	if (out) *out = m;
	return m.x1 - m.x0;
}

STBTTC_DEF int
stb_ttc_DrawText(
		struct stb_ttc_info * ttc,
		float 			scale,
		const char * 	text,
		unsigned int	dx,
		unsigned int	base_dy,
		unsigned char * pixels,
		unsigned int 	p_w,
		unsigned int 	p_h,
		unsigned int 	p_stride)
{
	unsigned int state = 0;
	int xpos = dx / scale;
	unsigned int last = 0;
	int glyph_count = 0;
	unsigned int cp = 0;

	for (int ch = 0; text[ch]; ch++) {
		if (stb_ttc__UTF8_Decode(&state, &cp, text[ch]) != UTF8_ACCEPT)
			continue;
		if (last) {
			int kern = scale * stb_ttc__CodepointsGetKerning(ttc, last, cp);
			xpos += kern;
		}
		glyph_count++;
		last = cp;
		int gl = stb_ttc__CodepointGetGlyph(ttc, cp);
		if (gl == -1)
			continue;
		stb_ttc_g *gc = stb_ttc__ScaledGlyphGetCache(ttc, gl, scale);
		if (!gc)
			continue;
	//	if (glyph_count == 1)
	//		xpos += -gc->x0 / scale;
		if (gc->p_y == (unsigned short) -1)
			stb_ttc__ScaledGlyphRenderToCache(ttc, gc);

		int pxpos = gc->x0 + ((xpos + gc->lsb) * scale);
		stb_ttc__GlyphRenderFromCache(ttc, gc, pxpos, base_dy,
				pixels, p_w, p_h, p_stride);
		xpos += gc->advance;
	}
	return glyph_count;
}

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>

STBTTC_DEF int
stb_ttc_MapFont(
		struct stb_ttc_info * ttc,
		const char * font_file)
{
	int fd = open(font_file, O_RDONLY);
	if (fd == -1)
		return -1;
	struct stat st;
	if (fstat(fd, &st) == -1) {
		close(fd);
		return -1;
	}
	ttc->font_size = st.st_size;
	unsigned char *map = mmap(NULL, ttc->font_size, PROT_READ, MAP_PRIVATE, fd, 0);
	close(fd);
	if (map == MAP_FAILED)
		return -1;
	ttc->font_mmap = 1;
	ttc->p_stride = 100;	// default value;
	// this make the code work for both ttf and ttc files
	int offset = stbtt_GetFontOffsetForIndex(map, 0);
	stbtt_InitFont(&ttc->font, map, offset);
	stbtt_GetFontVMetrics(&ttc->font, &ttc->ascent, &ttc->descent, 0);
	return 0;
}

STBTTC_DEF int
stb_ttc_LoadFont(
		struct stb_ttc_info * ttc,
		const void * font_data,
		unsigned int font_size)
{
	ttc->font_size = font_size;
	unsigned char *map = (unsigned char *)font_data;
	ttc->font_mmap = 0;
	ttc->p_stride = 100;	// default value;
	// this make the code work for both ttf and ttc files
	int offset = stbtt_GetFontOffsetForIndex(map, 0);
	stbtt_InitFont(&ttc->font, map, offset);
	stbtt_GetFontVMetrics(&ttc->font, &ttc->ascent, &ttc->descent, 0);
	return 0;
}

STBTTC_DEF void
stb_ttc_Free(
		struct stb_ttc_info * ttc)
{
	for (int i = 0; i < STB_TTC_BINCOUNT; i++) {
		free(ttc->cp_bin[i].cp_gl);
		free(ttc->kn_bin[i].cp_kn);
		free(ttc->g_bin[i].index);
	}
	free(ttc->pixels);
	free(ttc->glyph);
	if (ttc->font_mmap)
		munmap(ttc->font.data, ttc->font_size);
}
#endif /* STB_TTC_IMPLEMENTATION */

#endif /* STB_TTC_H_ */
