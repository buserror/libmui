/*
 * mui_font.c
 *
 * Copyright (C) 2023 Michel Pollet <buserror@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#define STB_TRUETYPE_IMPLEMENTATION
#define STB_TTC_IMPLEMENTATION
#include "stb_ttc.h"


//#ifndef __wasm__
#define INCBIN_STYLE INCBIN_STYLE_SNAKE
#define INCBIN_PREFIX mui_
#include "incbin.h"

INCBIN(main_font, "fonts/Charcoal_mui.ttf");
INCBIN(icon_font, "fonts/typicon.ttf");
INCBIN(dingbat_font, "fonts/Dingbat.ttf");
INCBIN(geneva_font, "fonts/Geneva.ttf");
//#endif

#include "mui.h"

// "Narrow style" reduces the advance by this factor
// Not the 'space' characters are reduced even more (twice that)
#define MUI_NARROW_ADVANCE_FACTOR	0.92
// Interline factor for compact text
#define MUI_COMPACT_FACTOR 			0.85

mui_font_t *
mui_font_find(
	mui_t *ui,
	const char *name)
{
	mui_font_t *f;
	TAILQ_FOREACH(f, &ui->fonts, self) {
		if (!strcmp(f->name, name))
			return f;
	}
	return NULL;
}

static void
_mui_font_pixman_prep(
	mui_font_t *f)
{
	f->font.pix.bpp = 8;
	f->font.pix.size.x = f->ttc.p_stride;
	f->font.pix.size.y = f->ttc.p_height;
	f->font.pix.row_bytes = f->ttc.p_stride;
	f->font.pix.pixels = f->ttc.pixels;
}

mui_font_t *
mui_font_from_mem(
		mui_t *ui,
		const char *name,
		uint size,
		const void *font_data,
		uint font_size )
{
	mui_font_t *f = calloc(1, sizeof(*f));
	f->name = strdup(name);
	f->size = size;
	stb_ttc_LoadFont(&f->ttc, font_data, font_size);
	TAILQ_INSERT_TAIL(&ui->fonts, f, self);
//	printf("%s: Loaded font %s:%d\n", __func__, name, size);

	return f;
}

void
mui_font_init(
		mui_t *ui)
{
//	printf("%s: Loading fonts\n", __func__);
#ifndef __wasm__
	mui_font_from_mem(ui, "main", 28,
			mui_main_font_data, mui_main_font_size);
	mui_font_from_mem(ui, "icon_large", 96,
			mui_icon_font_data, mui_icon_font_size);
	mui_font_from_mem(ui, "icon_small", 30,
			mui_icon_font_data, mui_icon_font_size);
#endif
}

void
mui_font_dispose(
		mui_t *ui)
{
	mui_font_t *f;
	while ((f = TAILQ_FIRST(&ui->fonts))) {
		TAILQ_REMOVE(&ui->fonts, f, self);
		stb_ttc_Free(&f->ttc);
		mui_drawable_dispose(&f->font);
		free(f->name);
		free(f);
	}
}

int
mui_font_text_measure(
		mui_font_t *font,
		const char *text,
		stb_ttc_measure *m )
{
	struct stb_ttc_info * ttc = &font->ttc;
	float scale = stbtt_ScaleForPixelHeight(&ttc->font, font->size);
	int w = stb_ttc_MeasureText(ttc, scale, text, m);
	return w;
}


void
mui_font_text_draw(
		mui_font_t *font,
		mui_drawable_t *dr,
		c2_pt_t where,
		const char *text,
		uint text_len,
		mui_color_t color)
{
	struct stb_ttc_info * ttc = &font->ttc;
	uint state = 0;
	float scale = stbtt_ScaleForPixelHeight(&ttc->font, font->size);
	double xpos = 0;
	uint last = 0;
	uint cp = 0;

	if (!text_len)
		text_len = strlen(text);
	mui_drawable_t * src = &font->font;
	mui_drawable_t * dst = dr;

	pixman_color_t pc = PIXMAN_COLOR(color);
	pixman_image_t * fill = pixman_image_create_solid_fill(&pc);

	where.y += font->ttc.ascent * scale;
	for (uint ch = 0; text[ch] && ch < text_len; ch++) {
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
		if (gc->p_y == (unsigned short) -1)
			stb_ttc__ScaledGlyphRenderToCache(ttc, gc);
//		int pxpos = gc->x0 + ((xpos + gc->lsb) * scale);
		int pxpos = where.x + gc->x0 + ((xpos + 0) * scale);
	//	if (gc->lsb)
	//		printf("glyph %3d : %04x:%c lsb %d\n", ch, cp, cp < 32 ? '.' : cp, gc->lsb);

		int ph = gc->y1 - gc->y0;
		int pw = gc->x1 - gc->x0;
		_mui_font_pixman_prep(font);
		pixman_image_composite32(
				PIXMAN_OP_OVER,
				fill,
				mui_drawable_get_pixman(src),
				mui_drawable_get_pixman(dst),
				0, 0, gc->p_x, gc->p_y,
				pxpos, where.y + gc->y0, pw, ph);
		xpos += gc->advance;
	}
	pixman_image_unref(fill);
}

IMPLEMENT_C_ARRAY(mui_glyph_array);
IMPLEMENT_C_ARRAY(mui_glyph_line_array);


void
mui_font_measure(
		mui_font_t *font,
		c2_rect_t bbox,
		const char *text,
		uint text_len,
		mui_glyph_line_array_t *lines,
		mui_text_e flags)
{
	struct stb_ttc_info * ttc = &font->ttc;
	uint state = 0;
	float scale = stbtt_ScaleForPixelHeight(&ttc->font, font->size);
	uint last = 0;
	uint cp = 0;
	int debug = flags & MUI_TEXT_DEBUG;

	if (!text_len)
		text_len = strlen(text);
	//debug = !strncmp(text, "Titan", 5) || !strcmp(text, "Driver");
	if (debug)
		printf("Measure text %s\n", text);
	lines->margin_left = c2_rect_width(&bbox);
	lines->margin_right = 0;
	lines->height = 0;
	c2_pt_t where = {};
	uint ch = 0;
	int wrap_chi = 0;
	int wrap_w = 0;
	int wrap_count = 0;
	float compact = flags & MUI_TEXT_ALIGN_COMPACT ?
						MUI_COMPACT_FACTOR : 1.0;
	float narrow = flags & MUI_TEXT_STYLE_NARROW ?
						MUI_NARROW_ADVANCE_FACTOR : 1.0;
	float narrow_space = flags & MUI_TEXT_STYLE_NARROW ?
						narrow * 0.9 : 1.0;
	mui_glyph_array_t * line = NULL;
	do {
		const mui_glyph_array_t zero = {};
		mui_glyph_line_array_push(lines, zero);
		line = &lines->e[lines->count - 1];
		line->x = 0;
		line->t = where.y;
		where.y += (font->ttc.ascent * compact) * scale;
		line->b = where.y - (font->ttc.descent * scale);
		line->y = where.y;
		line->w = 0;
		wrap_chi = ch;
		wrap_w = 0;
		wrap_count = 0;
		if (debug)
			printf("line %d y:%3d ch:%3d\n", lines->count,
						line->y, ch);
		for (;text[ch]; ch++) {
			if (stb_ttc__UTF8_Decode(&state, &cp, text[ch]) != UTF8_ACCEPT)
				continue;
			if (last) {
				int kern = scale * stb_ttc__CodepointsGetKerning(ttc, last, cp);
				line->w += kern;
			}
			last = cp;
			if (debug) printf("  glyph ch:%3d : %04x:%c S:%d L:%2d:%2d\n",
						ch, cp, cp < 32 ? '.' : cp, state,
						lines->count-1, line->count);
			if (cp == '\n') {
				line->line_break = true;
				ch++;
				break;
			}
			if (isspace(cp) || ispunct(cp)) {
				wrap_chi 	= ch;
				wrap_w 		= line->w;
				wrap_count 	= line->count;
			}
			int gl = stb_ttc__CodepointGetGlyph(ttc, cp);
			if (gl == -1)
				continue;
			stb_ttc_g *gc = stb_ttc__ScaledGlyphGetCache(ttc, gl, scale);
			if (!gc)
				continue;
			if (gc->p_y == (unsigned short) -1)
				stb_ttc__ScaledGlyphRenderToCache(ttc, gc);
			float advance = gc->advance * narrow;
			// we make spaces even narrower (if narrow style is on)
			if (cp == ' ')
				advance *= narrow_space;
			if (((line->w + advance) * scale) > c2_rect_width(&bbox)) {
				if (wrap_count) {
					ch = wrap_chi + 1;
					line->count = wrap_count;
					line->w = wrap_w;
				}
				break;
			}
			mui_glyph_t g = {
				.glyph = cp,
				.pos = ch,
				.index = gc->index,
				.x = (line->w * scale) + gc->x0,
				.w = advance * scale,
			};
			mui_glyph_array_push(line, g);
	//		printf("	PUSH[%2d] glyph %3d : %04x:%c x:%3d w:%3d\n",
	//				line->count - 1, g.pos, text[g.pos], text[g.pos],
	//				g.x, g.w);
			line->w += advance;
		};
		if (line->line_break) {
			// stuff a newline here
			mui_glyph_t g = {
				.glyph = 0,
				.pos = ch,
				.x = (line->w) * scale,
			};
			mui_glyph_array_push(line, g);
		}
	// zero terminate the line, so there is a marker at the end
		mui_glyph_t g = {
			.glyph = 0,
			.pos = ch,
			.x = (line->w) * scale,
		};
		mui_glyph_array_push(line, g);
		line->count--;
		where.y += -font->ttc.descent * scale;
	} while (text[ch] && ch < text_len);
	/*
	 * Finalise the lines, calculate the total height, and the margins
	 * Margins are the minimal x and maximal x of the lines
	 */
	for (uint i = 0; i < lines->count; i++) {
		mui_glyph_array_t * line = &lines->e[i];
		lines->height = line->y - (font->ttc.descent * scale);
		line->w *= scale;
	}
	int ydiff = 0;
	if (flags & MUI_TEXT_ALIGN_MIDDLE) {
		ydiff = (c2_rect_height(&bbox) - (int)lines->height) / 2;
	} else if (flags & MUI_TEXT_ALIGN_BOTTOM) {
		ydiff = c2_rect_height(&bbox) - (int)lines->height;
	}
	if (debug)
		printf("box height is %d/%d ydiff:%d\n",
				lines->height, c2_rect_height(&bbox), ydiff);
	for (uint i = 0; i < lines->count; i++) {
		mui_glyph_array_t * line = &lines->e[i];
		line->y += ydiff;
		if (i == lines->count - 1)	// last line is always a break
			line->line_break = true;
		if (flags & MUI_TEXT_ALIGN_RIGHT) {
			line->x = c2_rect_width(&bbox) - line->w;
		} else if (flags & MUI_TEXT_ALIGN_CENTER) {
			line->x = (c2_rect_width(&bbox) - line->w) / 2;
		} else if (flags & MUI_TEXT_ALIGN_FULL) {
			line->x = 0;
			if (line->count > 1 && !line->line_break) {
				float space = (c2_rect_width(&bbox) - line->w) / (line->count - 1);
				for (uint ci = 1; ci < line->count; ci++)
					line->e[ci].x += ci * space;
			}
		}
		if (line->x < (int)lines->margin_left)
			lines->margin_left = line->x;
		if (line->x + line->w > lines->margin_right)	// last x
			lines->margin_right = line->x + line->w;
		if (debug)
			printf("  line %d y:%3d size %3d width %.2f\n", i,
					line->y, line->count, line->w);
	}
}

void
mui_font_measure_clear(
		mui_glyph_line_array_t *lines)
{
	if (!lines)
		return;
	for (uint i = 0; i < lines->count; i++) {
		mui_glyph_array_t * line = &lines->e[i];
		mui_glyph_array_free(line);
	}
	mui_glyph_line_array_free(lines);
}


void
mui_font_measure_draw(
		mui_font_t *font,
		mui_drawable_t *dr,
		c2_rect_t bbox,
		mui_glyph_line_array_t *lines,
		mui_color_t color,
		mui_text_e flags)
{
	pixman_color_t pc = PIXMAN_COLOR(color);
	pixman_image_t * fill = pixman_image_create_solid_fill(&pc);
	struct stb_ttc_info * ttc = &font->ttc;

	mui_drawable_t * src = &font->font;
	mui_drawable_t * dst = dr;
	_mui_font_pixman_prep(font);
	for (uint li = 0; li < lines->count; li++) {
		mui_glyph_array_t * line = &lines->e[li];
		int lastu = line->x;
		for (uint ci = 0; ci < line->count; ci++) {
			uint cache_index = line->e[ci].index;
			if (line->e[ci].glyph < ' ')
				continue;
			stb_ttc_g *gc = &ttc->glyph[cache_index];
			float pxpos = line->e[ci].x;

			int ph = gc->y1 - gc->y0;
			int pw = gc->x1 - gc->x0;
			pixman_image_composite32(PIXMAN_OP_OVER, fill,
					mui_drawable_get_pixman(src),
					mui_drawable_get_pixman(dst),
					0, 0, gc->p_x, gc->p_y,
					bbox.l + (line->x + pxpos),
					bbox.t + (line->y + gc->y0), pw, ph);
			/*
			 * For 'cheap' bold, we just draw the glyph again over the
			 * same position, but shifted by one pixel in x.
			 * Works surprisingly well!
			 */
			if (flags & MUI_TEXT_STYLE_BOLD) {
				pixman_image_composite32(PIXMAN_OP_OVER, fill,
						mui_drawable_get_pixman(src),
						mui_drawable_get_pixman(dst),
						0, 0, gc->p_x, gc->p_y,
						bbox.l + line->x + pxpos + 1,
						bbox.t + line->y + gc->y0, pw, ph);
			}
			/*
			 * Underline is very primitive, it just draws a line
			 * under the glyphs, but it's enough for now. Skips the
			 * ones with obvious descenders. This is far from perfect
			 * obviously but it's a start.
			 */
			if (flags & MUI_TEXT_STYLE_ULINE) {
				// don't draw under glyphs like qpygj etc
				bool draw_underline = gc->y1 <= 2;
				if (draw_underline) {
					c2_rect_t u = C2_RECT(
							bbox.l + lastu,
							bbox.t + line->y + 2,
							bbox.l + line->x + pxpos + pw,
							bbox.t + line->y + 3);
					pixman_image_fill_boxes(PIXMAN_OP_OVER,
							mui_drawable_get_pixman(dst),
							&pc, 1, (pixman_box32_t*)&u);
				}
				lastu = line->x + pxpos + pw;
			}
		}
	}
	pixman_image_unref(fill);
}

void
mui_font_textbox(
		mui_font_t *font,
		mui_drawable_t *dr,
		c2_rect_t bbox,
		const char *text,
		uint text_len,
		mui_color_t color,
		mui_text_e flags)
{
	mui_glyph_line_array_t lines = {};

	if (!text_len)
		text_len = strlen(text);

	mui_font_measure(font, bbox, text, text_len, &lines, flags);
	mui_font_measure_draw(font, dr, bbox, &lines, color, flags);
	mui_font_measure_clear(&lines);
}
