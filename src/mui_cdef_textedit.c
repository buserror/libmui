/*
 * mui_cdef_textedit.c
 *
 * Copyright (C) 2023 Michel Pollet <buserror@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 */
/*
 * This is a simple textedit control, it's not meant to be a full fledged
 * text editor, but more a simple text input field.
 *
 * One obvious low hanging fruit would be to split the drawing code
 * to be able to draw line-by-line, allowing skipping lines that are
 * not visible. Currently the whole text is drawn every time, and relies
 * on clipping to avoid drawing outside the control.
 *
 * System is based on mui_font_measure() returning a mui_glyph_line_array_t
 * that contains the position of each glyph in the text, and the width of
 * each line.
 * The text itself is a UTF8 array, so we need to be aware of multi-byte
 * glyphs. The 'selection' is kept as a start and end glyph index, and
 * the drawing code calculates the rectangles for the selection.
 *
 * There is a text_content rectangle that deals with the scrolling, and
 * the text (and selection) is drawn offset by the top left of this rectangle.
 *
 * There is a carret timer that makes the carret blink, and a carret is
 * drawn when the selection is empty.
 *
 * There can only one 'carret' blinking at a time, the one in the control
 * that has the focus, so the carret timer is a global timer that is reset
 * every time a control gets the focus.
 *
 * The control has a margin, and a frame, and the text is drawn inside the
 * frame, and the margin is used to inset the text_content rectangle.
 * Margin is optional, and frame is optional too.
 *
 * The control deals with switching focus as well, so clicking in a textedit
 * will deactivate the previously focused control, and activate the new one.
 * TAB will do the same, for the current window.
 */
#include <stdio.h>
#include <stdlib.h>

#include "mui.h"
#include "cg.h"

enum {
	MUI_CONTROL_TEXTEDIT 		= FCC('T','e','a','c'),
};

#define D(_w)  ; // _w

enum {
	MUI_TE_SELECTING_GLYPHS		= 0,
	MUI_TE_SELECTING_WORDS,
//	MUI_TE_SELECTING_LINES,		// TODO?
};

/*
 * This describes a text edit action, either we insert some text at some position,
 * or we delete some text at some position.
 * These actions are queued in a TAILQ, so we can undo/redo them.
 * The text is UTF8, and the position is a BYTE index in the text (not a glyph).
 *
 * We preallocate a fixed number of actions, and when we reach the limit, we
 * start reusing the oldest ones. This limits the number of undo/redo actions
 * to something sensible.
 */
typedef struct mui_te_action_t {
	TAILQ_ENTRY(mui_te_action_t) self;
	uint  		insert : 1;			// if not insert, its a delete
	uint32_t  	position, length;
	mui_utf8_t 	text;
} mui_te_action_t;

// action queue
typedef TAILQ_HEAD(mui_te_action_queue_t, mui_te_action_t) mui_te_action_queue_t;

/*
 * This describes the selection in the text-edit, it can either be a carret,
 * or a selection of text. The selection is kept as a start and end glyph index,
 * and the drawing code calculates the rectangles for the selection.
 */
typedef struct mui_sel_t {
	uint carret: 1;		// carret is visible (if sel.start == end)
	uint start, end;	// glyph index in text
	// rectangles for the first partial line, the body,
	// and the last partial line. All of them can be empty
	union {
		struct {
			c2_rect_t first, body, last;
		};
		c2_rect_t e[3];
	};
} mui_sel_t;

typedef struct mui_textedit_control_t {
	mui_control_t 		control;
	uint				trace : 1;	// debug trace
	uint32_t			flags;		// display flags
	mui_sel_t			sel;
	mui_font_t *		font;
	mui_utf8_t 			text;
	mui_glyph_line_array_t  measure;
	c2_pt_t				margin;
	c2_rect_t 			text_content;
	struct {
		uint 				start, end;
	}					click;
	uint 				selecting_mode;
} mui_textedit_control_t;

extern const mui_control_color_t mui_control_color[MUI_CONTROL_STATE_COUNT];


static void
_mui_textedit_select_signed(
		mui_textedit_control_t *	te,
		int 						glyph_start,
		int 						glyph_end);
static void
_mui_textedit_refresh_sel(
		mui_textedit_control_t *	te,
		mui_sel_t * 				sel);
static bool
mui_cdef_textedit(
		struct mui_control_t * 	c,
		uint8_t 				what,
		void * 					param);

/*
 * Rectangles passed here are in TEXT coordinates.
 * which means they are already offset by margin.x, margin.y
 * and the text_content.tl.x, text_content.tl.y
 */
static void
_mui_textedit_inval(
		mui_textedit_control_t *	te,
		c2_rect_t 					r)
{
	c2_rect_offset(&r, te->text_content.tl.x, te->text_content.tl.y);
	if (!c2_rect_isempty(&r))
		mui_window_inval(te->control.win, &r);
}

/* this is the timer used to make the carret blink *for all windows* */
static mui_time_t
_mui_textedit_carret_timer(
		struct mui_t *				mui,
		mui_time_t 					now,
		void * 						param)
{
	mui_window_t *win = mui_window_front(mui);

//	printf("carret timer win %p focus %p\n", win, win->control_focus);
	if (win && win->control_focus.control &&
			win->control_focus.control->type == MUI_CONTROL_TEXTEDIT) {
		mui_textedit_control_t *te =
				(mui_textedit_control_t *)win->control_focus.control;
		te->sel.carret = !te->sel.carret;
		if (te->sel.start == te->sel.end)
			_mui_textedit_refresh_sel(te, NULL);
	}
	return 500 * MUI_TIME_MS;
}

/* this 'forces' the carret to be visible, used when typing */
static void
_mui_textedit_show_carret(
		mui_textedit_control_t *	te)
{
	mui_t * mui = te->control.win->ui;
	mui_window_t *win = mui_window_front(mui);
	if (win && win->control_focus.control == &te->control) {
		mui_timer_reset(mui,
					mui->carret_timer,
					_mui_textedit_carret_timer,
					500 * MUI_TIME_MS);
	}
	te->sel.carret = 1;
	_mui_textedit_refresh_sel(te, NULL);
}

/* Return the line number, and glyph position in line a glyph index */
static int
_mui_glyph_to_line_index(
		mui_glyph_line_array_t * 	measure,
		uint  						glyph_pos,
		uint * 						out_line,
		uint * 						out_line_index)
{
	*out_line = 0;
	*out_line_index = 0;
	if (!measure->count)
		return -1;
	for (uint i = 0; i < measure->count; i++) {
		mui_glyph_array_t * line = &measure->e[i];
		if (glyph_pos > line->count) {
			glyph_pos -= line->count;
			continue;
		}
		*out_line = i;
		*out_line_index = glyph_pos;
		return i;
	}
	// return last glyph last line
	*out_line = measure->count - 1;
	*out_line_index = measure->e[*out_line].count - 1;
	return measure->count - 1;
}

/* Return the line number and glyph index in that line for a point x,y */
static int
_mui_point_to_line_index(
		mui_textedit_control_t *	te,
		mui_font_t *				font,
		c2_rect_t 					frame,
		c2_pt_t 					where,
		uint * 						out_line,
		uint * 						out_line_index)
{
	mui_glyph_line_array_t * 	measure = &te->measure;
	if (!measure->count)
		return -1;
	*out_line = 0;
	*out_line_index = 0;
	for (uint i = 0; i < measure->count; i++) {
		mui_glyph_array_t * line = &measure->e[i];
		c2_rect_t line_r = {
			.l = frame.l + te->text_content.l,
			.t = frame.t + line->t + te->text_content.t,
			.r = frame.r + te->text_content.l,
			.b = frame.t + line->b + te->text_content.t,
		};
		if (!((where.y >= line_r.t) && (where.y < line_r.b)))
			continue;
		*out_line = i;
		*out_line_index = line->count;
	//	printf("  last x: %d where.x: %d\n",
	//			frame.l + (int)line->e[line->count-1].x, where.x);
		if (where.x > (line_r.l + (int)line->e[line->count].x)) {
			*out_line_index = line->count;
			return 0;
		} else if (where.x < (line_r.l + (int)line->e[0].x)) {
			*out_line_index = 0;
			return 0;
		}
		for (uint j = 0; j < line->count; j++) {
			if (where.x < (line_r.l + (int)line->e[j].x))
				return 0;
			*out_line_index = j;
		}
	//	printf("point_to_line_index: line %d:%d / %d\n",
	//		*out_line, *out_line_index, line->count);
		return 0;
	}
	return -1;
}

/* Return the glyph position in the text for line number and index in line */
static uint
_mui_line_index_to_glyph(
		mui_glyph_line_array_t * 	measure,
		uint 						line,
		uint 						index)
{
	uint pos = 0;
	for (uint i = 0; i < line; i++)
		pos += measure->e[i].count;
	pos += index;
	return pos;
}

/* Return the beginning and end glyphs for the line/index in line */
static void
_mui_line_index_to_glyph_word(
		mui_glyph_line_array_t * 	measure,
		uint 						line,
		uint 						index,
		uint 						*word_start,
		uint 						*word_end)
{
	*word_start = 0;
	*word_end = 0;
	uint start = index;
	uint end = index;
	mui_glyph_array_t * l = &measure->e[line];
	while (start > 0 && l->e[start-1].glyph > 32)
		start--;
	while (end < l->count && l->e[end].glyph > 32)
		end++;
	*word_start = _mui_line_index_to_glyph(measure, line, start);
	*word_end = _mui_line_index_to_glyph(measure, line, end);
}

/* Convert a glyph index to a byte index (used to manipulate text array) */
static uint
_mui_glyph_to_byte_offset(
		mui_glyph_line_array_t * 	measure,
		uint 						glyph_pos)
{
	uint pos = 0;
	for (uint i = 0; i < measure->count; i++) {
		mui_glyph_array_t * line = &measure->e[i];
		if (glyph_pos > pos + line->count) {
			pos += line->count;
			continue;
		}
		uint idx = glyph_pos - pos;
	//	printf("glyph_to_byte_offset: glyph_pos %d line %d:%2d\n",
	//			glyph_pos, i, idx);
		return line->e[idx].pos;
	}
//	printf("glyph_to_byte_offset: glyph_pos %d out of range\n", glyph_pos);
	return 0;
}

/*
 * Calculate the 3 rectangles that represent the graphical selection.
 * The 'start' is the first line of the selection, or the position of the
 * carret if the selection is empty.
 * The other two are 'optional' (they can be empty), and represent the last
 * line of the selection, and the body of the selection that is the rectangle
 * between the first and last line.
 */
static int
_mui_make_sel_rects(
		mui_glyph_line_array_t * 	measure,
		mui_font_t *				font,
		mui_sel_t *					sel,
		c2_rect_t 					frame)
{
	if (!measure->count)
		return -1;
	sel->last = sel->first = sel->body = (c2_rect_t) {};
	uint start_line, start_index;
	uint end_line, end_index;
	_mui_glyph_to_line_index(measure, sel->start, &start_line, &start_index);
	_mui_glyph_to_line_index(measure, sel->end, &end_line, &end_index);
	mui_glyph_array_t * line = &measure->e[start_line];

	if (start_line == end_line) {
		// single line selection
		sel->first = (c2_rect_t) {
			.l = frame.l + line->e[start_index].x,
			.t = frame.t + line->t,
			.r = frame.l + line->e[end_index].x,
			.b = frame.t + line->b,
		};
		return 0;
	}
	// first line
	sel->first = (c2_rect_t) {
		.l = frame.l + line->e[start_index].x, .t = frame.t + line->t,
		.r = frame.r, .b = frame.t + line->b,
	};
	// last line
	line = &measure->e[end_line];
	sel->last = (c2_rect_t) {
		.l = frame.l, .t = frame.t + line->t,
		.r = frame.l + line->e[end_index].x, .b = frame.t + line->b,
	};
	// body
	sel->body = (c2_rect_t) {
		.l = frame.l, .t = sel->first.b,
		.r = frame.r, .b = sel->last.t,
	};
	return 0;
}

/* Refresh the whole selection (or around the carret selection) */
static void
_mui_textedit_refresh_sel(
		mui_textedit_control_t *	te,
		mui_sel_t * 				sel)
{
	if (!sel)
		sel = &te->sel;
	for (int i = 0; i < 3; i++) {
		c2_rect_t r = te->sel.e[i];
		if (i == 0 && te->sel.start == te->sel.end) {
			c2_rect_inset(&r, -1, -1);
//			printf("refresh_sel: carret %s\n", c2_rect_as_str(&r));
		}
		if (!c2_rect_isempty(&r))
			_mui_textedit_inval(te, r);
	}
}

/* this makes sure the text is always visible in the frame */
static void
_mui_textedit_clamp_text_frame(
		mui_textedit_control_t *	te)
{
	c2_rect_t f = te->control.frame;
	c2_rect_offset(&f, -f.l, -f.t);
	if (te->flags & MUI_CONTROL_TEXTBOX_FRAME)
		c2_rect_inset(&f, te->margin.x, te->margin.y);
	c2_rect_t old = te->text_content;
	te->text_content.r = te->text_content.l + te->measure.margin_right;
	te->text_content.b = te->text_content.t + te->measure.height;
	D(printf("  %s %s / %3dx%3d\n", __func__,
			c2_rect_as_str(&te->text_content),
			c2_rect_width(&f), c2_rect_height(&f));)
	if (te->text_content.b < c2_rect_height(&f))
		c2_rect_offset(&te->text_content, 0,
				c2_rect_height(&f) - te->text_content.b);
	if (te->text_content.t > f.t)
		c2_rect_offset(&te->text_content, 0, f.t - te->text_content.t);
	if (te->text_content.r < c2_rect_width(&f))
		c2_rect_offset(&te->text_content,
				c2_rect_width(&f) - te->text_content.r, 0);
	if (te->text_content.l > f.l)
		c2_rect_offset(&te->text_content, f.l - te->text_content.l, 0);
	if (c2_rect_equal(&te->text_content, &old))
		return;
	D(printf("   clamped TE from %s to %s\n", c2_rect_as_str(&old),
			c2_rect_as_str(&te->text_content));)
	mui_control_inval(&te->control);
}

/* This scrolls the view following the carret, used when typing.
 * This doesn't check for out of bounds, but the clamping should
 * have made sure the text is always visible. */
static void
_mui_textedit_ensure_carret_visible(
		mui_textedit_control_t *	te)
{
	c2_rect_t f = te->control.frame;
//	c2_rect_offset(&f, -f.l, -f.t);
	if (te->flags & MUI_CONTROL_TEXTBOX_FRAME)
		c2_rect_inset(&f, te->margin.x, te->margin.y);
	if (te->sel.start != te->sel.end)
		return;
	c2_rect_t old = te->text_content;
	c2_rect_t r = te->sel.first;
	D(printf("%s carret %s frame %s\n", __func__,
			c2_rect_as_str(&r), c2_rect_as_str(&f));)
	c2_rect_offset(&r, -te->text_content.l, -te->text_content.t);
	if (r.r < f.l) {
		D(printf("   moved TE LEFT %d\n", -(f.l - r.r));)
		c2_rect_offset(&te->text_content, -(f.l - r.l), 0);
	}
	if (r.l > f.r) {
		D(printf("   moved TE RIGHT %d\n", -(r.l - f.r));)
		c2_rect_offset(&te->text_content, -(r.l - f.r), 0);
	}
	if (r.t < f.t)
		c2_rect_offset(&te->text_content, 0, r.t - f.t);
	if (r.b > f.b)
		c2_rect_offset(&te->text_content, 0, r.b - f.b);
	if (c2_rect_equal(&te->text_content, &old))
		return;
	D(printf("   moved TE from %s to %s\n", c2_rect_as_str(&old),
			c2_rect_as_str(&te->text_content));)
	_mui_textedit_clamp_text_frame(te);
}

/*
 * This is to be called when the text changes, or the frame (width) changes
 */
static void
_mui_textedit_refresh_measure(
		mui_textedit_control_t *	te)
{
	c2_rect_t f = te->control.frame;
	c2_rect_offset(&f, -f.l, -f.t);
	if (te->flags & MUI_CONTROL_TEXTBOX_FRAME)
		c2_rect_inset(&f, te->margin.x, te->margin.y);
	if (!(te->flags & MUI_CONTROL_TEXTEDIT_VERTICAL))
		f.r = 0x7fff; // make it very large, we don't want wrapping.

	mui_glyph_line_array_t new_measure = {};

	mui_font_measure(te->font, f,
					(const char*)te->text.e, te->text.count-1,
					&new_measure, te->flags);

	f = te->control.frame;
	if (te->flags & MUI_CONTROL_TEXTBOX_FRAME)
		c2_rect_inset(&f, te->margin.x, te->margin.y);
	// Refresh the lines that have changed. Perhaps all of them did,
	// doesn't matter, but it's nice to avoid redrawing the whole text
	// when someone is typing.
	for (uint i = 0; i < new_measure.count && i < te->measure.count; i++) {
		if (i >= te->measure.count) {
			c2_rect_t r = f;
			r.t += new_measure.e[i].t;
			r.b = r.t + new_measure.e[i].b;
			r.r = new_measure.e[i].x + new_measure.e[i].w;
			_mui_textedit_inval(te, r);
		} else if (i >= new_measure.count) {
			c2_rect_t r = f;
			r.t += te->measure.e[i].t;
			r.b = r.t + te->measure.e[i].b;
			r.r = te->measure.e[i].x + te->measure.e[i].w;
			_mui_textedit_inval(te, r);
		} else {
			int dirty = 0;
			// unsure if this could happen, but let's be safe --
			// technically we should refresh BOTH rectangles (old, new)
			if (new_measure.e[i].t != te->measure.e[i].t ||
					new_measure.e[i].b != te->measure.e[i].b) {
				dirty = 1;
			} else if (new_measure.e[i].x != te->measure.e[i].x ||
					new_measure.e[i].count != te->measure.e[i].count ||
					new_measure.e[i].w != te->measure.e[i].w)
				dirty = 1;
			else {
				for (uint x = 0; x < new_measure.e[i].count; x++) {
					if (new_measure.e[i].e[x].glyph != te->measure.e[i].e[x].glyph ||
							new_measure.e[i].e[x].x != te->measure.e[i].e[x].x ||
							new_measure.e[i].e[x].w != te->measure.e[i].e[x].w) {
						dirty = 1;
						break;
					}
				}
			}
			if (dirty) {
				c2_rect_t r = f;
				r.t += new_measure.e[i].t;
				r.b = r.t + new_measure.e[i].b;
				r.r = new_measure.e[i].x + new_measure.e[i].w;
				_mui_textedit_inval(te, r);
			}
		}
	}
	mui_font_measure_clear(&te->measure);
	te->measure = new_measure;
	_mui_textedit_clamp_text_frame(te);
}

static void
_mui_textedit_sel_delete(
		mui_textedit_control_t *	te,
		bool 						re_measure,
		bool 						reset_sel)
{
	if (te->sel.start == te->sel.end)
		return;
	mui_utf8_delete(&te->text,
			_mui_glyph_to_byte_offset(&te->measure, te->sel.start),
			_mui_glyph_to_byte_offset(&te->measure, te->sel.end) -
				_mui_glyph_to_byte_offset(&te->measure, te->sel.start));
	if (re_measure)
		_mui_textedit_refresh_measure(te);
	if (reset_sel)
		_mui_textedit_select_signed(te,
				te->sel.start, te->sel.start);
}

void
mui_textedit_set_text(
		mui_control_t * 			c,
		const char * 				text)
{
	mui_textedit_control_t *te = (mui_textedit_control_t *)c;
	mui_utf8_clear(&te->text);
	int tl = strlen(text);
	mui_utf8_realloc(&te->text, tl + 1);
	memcpy(te->text.e, text, tl + 1);
	/*
	 * Note, the text.count *counts the terminating zero*
	 */
	te->text.count = tl + 1;
	if (!te->font)
		te->font = mui_font_find(c->win->ui, "main");
	_mui_textedit_refresh_measure(te);
}

/* this one allows passing -1 etc, which is handy of cursor movement */
static void
_mui_textedit_select_signed(
		mui_textedit_control_t *	te,
		int 						glyph_start,
		int 						glyph_end)
{
	if (glyph_start < 0)
		glyph_start = 0;
	if (glyph_end < 0)
		glyph_end = 0;
	if (glyph_end > (int)te->text.count)
		glyph_end = te->text.count;
	if (glyph_start > (int)te->text.count)
		glyph_start = te->text.count;
	if (glyph_start > glyph_end) {
		uint t = glyph_start;
		glyph_start = glyph_end;
		glyph_end = t;
	}

//	printf("%s %d:%d\n", __func__, glyph_start, glyph_end);
	c2_rect_t f = te->control.frame;
	if (te->flags & MUI_CONTROL_TEXTBOX_FRAME)
		c2_rect_inset(&f, te->margin.x, te->margin.y);

	mui_glyph_line_array_t * 	measure = &te->measure;
	_mui_textedit_refresh_sel(te, NULL);
	mui_sel_t newone = { .start = glyph_start, .end = glyph_end };
	_mui_make_sel_rects(measure, te->font, &newone, f);
	te->sel = newone;
	_mui_textedit_ensure_carret_visible(te);
	_mui_textedit_refresh_sel(te, NULL);
}

/*
 * Mark old selection as invalid, and set the new one,
 * and make sure it's visible
 */
void
mui_textedit_set_selection(
		mui_control_t * 			c,
		uint 						glyph_start,
		uint 						glyph_end)
{
	mui_textedit_control_t *te = (mui_textedit_control_t *)c;
	_mui_textedit_select_signed(te, glyph_start, glyph_end);
}

static void
mui_textedit_draw(
		mui_window_t * 	win,
		mui_control_t * c,
		mui_drawable_t *dr )
{
	c2_rect_t f = c->frame;
	c2_rect_offset(&f, win->content.l, win->content.t);

	mui_textedit_control_t *te = (mui_textedit_control_t *)c;

	mui_drawable_clip_push(dr, &f);
	struct cg_ctx_t * cg = mui_drawable_get_cg(dr);
	if (te->flags & MUI_CONTROL_TEXTBOX_FRAME) {
		cg_set_line_width(cg, mui_control_has_focus(c) ? 2 : 1);
		cg_set_source_color(cg, &CG_COLOR(mui_control_color[c->state].frame));
		cg_rectangle(cg, f.l + 0.5, f.t + 0.5,
						c2_rect_width(&f)-1, c2_rect_height(&f)-1);
		cg_stroke(cg);
	}
//	cg = mui_drawable_get_cg(dr);	// this updates the cg clip too
	if (te->text.count <= 1)
		goto done;
	if (te->flags & MUI_CONTROL_TEXTBOX_FRAME)
		c2_rect_inset(&f, te->margin.x, te->margin.y);
	mui_drawable_clip_push(dr, &f);
	cg = mui_drawable_get_cg(dr);	// this updates the cg clip too
	bool is_active = c == c->win->control_focus.control;
	if (te->sel.start == te->sel.end) {
		if (te->sel.carret && is_active) {
			c2_rect_t carret = te->sel.first;
			c2_rect_offset(&carret,
					c->win->content.l + te->text_content.tl.x,
					c->win->content.t + te->text_content.tl.y);
			// rect is empty, but it's a carret!
			// draw a line at the current position
			cg_set_line_width(cg, 1);
			cg_set_source_color(cg, &CG_COLOR(mui_control_color[c->state].text));
			cg_move_to(cg, carret.l, carret.t);
			cg_line_to(cg, carret.l, carret.b);
			cg_stroke(cg);
		}
	} else {
		if (is_active) {
			for (int i = 0; i < 3; i++) {
				if (!c2_rect_isempty(&te->sel.e[i])) {
					c2_rect_t sr = te->sel.e[i];
				//	c2_rect_clip_rect(&sr, &f, &sr);
					cg_set_source_color(cg, &CG_COLOR(c->win->ui->color.highlight));
					c2_rect_offset(&sr,
							c->win->content.l + te->text_content.tl.x,
							c->win->content.t + te->text_content.tl.y);
					cg_rectangle(cg,
							sr.l, sr.t, c2_rect_width(&sr), c2_rect_height(&sr));
					cg_fill(cg);
				}
			}
		} else {	// draw a path around the selection
			cg_set_line_width(cg, 2);
			cg_set_source_color(cg, &CG_COLOR(c->win->ui->color.highlight));
			mui_sel_t  o = te->sel;
			for (int i = 0; i < 3; i++)
				c2_rect_offset(&o.e[i],
						c->win->content.l + te->text_content.tl.x,
						c->win->content.t + te->text_content.tl.y);
			cg_move_to(cg, o.first.l, o.first.t);
			cg_line_to(cg, o.first.r, o.first.t);
			cg_line_to(cg, o.first.r, o.first.b);
			if (c2_rect_isempty(&o.last))
				cg_line_to(cg, o.first.l, o.first.b);
			else {
				cg_line_to(cg, o.first.r, o.first.b);
				cg_line_to(cg, o.first.r, o.last.t);
				cg_line_to(cg, o.last.r, o.last.t);
				cg_line_to(cg, o.last.r, o.last.b);
				cg_line_to(cg, o.last.l, o.last.b);
				cg_line_to(cg, o.last.l, o.first.b);
			}
			cg_line_to(cg, o.first.l, o.first.b);
			cg_line_to(cg, o.first.l, o.first.t);
			cg_stroke(cg);
		}
	}
	c2_rect_t tf = f;
	c2_rect_offset(&tf, te->text_content.tl.x, te->text_content.tl.y);
	mui_font_measure_draw(te->font, dr, tf,
			&te->measure, mui_control_color[c->state].text, te->flags);
	mui_drawable_clip_pop(dr);
	cg = mui_drawable_get_cg(dr);	// this updates the cg clip too
	if (te->flags & MUI_CONTROL_TEXTBOX_FRAME) {
		if (c2_rect_width(&f) < c2_rect_width(&te->text_content)) {
			// draw a line-like mini scroll bar to show scroll position
			int fsize = c2_rect_width(&f);
			int tsize = c2_rect_width(&te->text_content);
			float ratio = fsize / (float)tsize;
			float dsize = fsize * ratio;
			c2_rect_t r = C2_RECT_WH(f.l, f.b + 1, dsize, 1);
			float pos = -te->text_content.tl.x / (float)(tsize - fsize);
			c2_rect_offset(&r, (fsize - dsize) * pos, 0);
			cg_set_source_color(cg,
					&CG_COLOR(mui_control_color[c->state].frame));
			cg_move_to(cg, r.l, r.t);
			cg_line_to(cg, r.r, r.t);
			cg_stroke(cg);
		}
		// same for vertical
		if (c2_rect_height(&f) < c2_rect_height(&te->text_content)) {
			int fsize = c2_rect_height(&f);
			int tsize = c2_rect_height(&te->text_content);
			float ratio = fsize / (float)tsize;
			float dsize = fsize * ratio;
			c2_rect_t r = C2_RECT_WH(f.r +1, f.t, 1, dsize);
			float pos = -te->text_content.tl.y / (float)(tsize - fsize);
			c2_rect_offset(&r, 0, (fsize - dsize) * pos);
			cg_set_source_color(cg,
					&CG_COLOR(mui_control_color[c->state].frame));
			cg_move_to(cg, r.l, r.t);
			cg_line_to(cg, r.l, r.b);
			cg_stroke(cg);
		}
	}
done:
	mui_drawable_clip_pop(dr);
}

static bool
mui_textedit_mouse(
		struct mui_control_t * 	c,
		mui_event_t * 			ev)
{
	mui_textedit_control_t *te = (mui_textedit_control_t *)c;

	c2_rect_t f = c->frame;
	c2_rect_offset(&f, c->win->content.l, c->win->content.t);
	uint line = 0, index = 0;
	bool res = false;
	switch (ev->type) {
		case MUI_EVENT_BUTTONDOWN: {
			if (!c2_rect_contains_pt(&f, &ev->mouse.where))
				break;
			if (!mui_control_has_focus(c))
				mui_control_set_focus(c);

			if (_mui_point_to_line_index(te, te->font, f,
					ev->mouse.where, &line, &index) == 0) {
				uint pos = _mui_line_index_to_glyph(
											&te->measure, line, index);
				te->selecting_mode = MUI_TE_SELECTING_GLYPHS;
				if (ev->mouse.count == 2) {
					// double click, select word
					uint32_t start,end;
					_mui_line_index_to_glyph_word(&te->measure, line, index,
							&start, &end);
					_mui_textedit_select_signed(te, start, end);
					te->selecting_mode = MUI_TE_SELECTING_WORDS;
				} else if (ev->modifiers & MUI_MODIFIER_SHIFT) {
					// shift click, extend selection
					if (pos < te->sel.start) {
						_mui_textedit_select_signed(te, pos, te->sel.end);
					} else {
						_mui_textedit_select_signed(te, te->sel.start, pos);
					}
				} else {
					// single click, set carret (and start selection
					_mui_textedit_select_signed(te, pos, pos);
				}
				te->click.start = te->sel.start;
				te->click.end = te->sel.end;
				D(printf("DOWN line %2d index %3d pos:%3d\n",
						line, index, pos);)
				res = true;
			};
			te->sel.carret = 0;
		}	break;
		case MUI_EVENT_BUTTONUP: {
			res = true;
			if (_mui_point_to_line_index(te, te->font, f,
					ev->mouse.where, &line, &index) == 0) {
				D(printf("UP line %d index %d\n", line, index);)
			}
			te->sel.carret = 1;
			_mui_textedit_refresh_sel(te, NULL);
		}	break;
		case MUI_EVENT_DRAG: {
			res = true;
			if (!c2_rect_contains_pt(&f, &ev->mouse.where)) {
				if (te->flags & MUI_CONTROL_TEXTEDIT_VERTICAL) {
					if (ev->mouse.where.y > f.b) {
						te->text_content.tl.y -= ev->mouse.where.y - f.b;
						D(printf("scroll down %3d\n", te->text_content.tl.y);)
						_mui_textedit_clamp_text_frame(te);
						mui_control_inval(c);
					} else if (ev->mouse.where.y < f.t) {
						te->text_content.tl.y += f.t - ev->mouse.where.y;
						D(printf("scroll up   %3d\n", te->text_content.tl.y);)
						_mui_textedit_clamp_text_frame(te);
						mui_control_inval(c);
					}
				} else {
					if (ev->mouse.where.x > f.r) {
						te->text_content.tl.x -= ev->mouse.where.x - f.r;
						D(printf("scroll right %3d\n", te->text_content.tl.x);)
						_mui_textedit_clamp_text_frame(te);
						mui_control_inval(c);
					} else if (ev->mouse.where.x < f.l) {
						te->text_content.tl.x += f.l - ev->mouse.where.x;
						D(printf("scroll left  %3d\n", te->text_content.tl.x);)
						_mui_textedit_clamp_text_frame(te);
						mui_control_inval(c);
					}
				}
			}
			if (_mui_point_to_line_index(te, te->font, f,
					ev->mouse.where, &line, &index) == 0) {
			//	printf("    line %d index %d\n", line, index);
				uint pos = _mui_line_index_to_glyph(
											&te->measure, line, index);
				if (te->selecting_mode == MUI_TE_SELECTING_WORDS) {
					uint32_t start,end;
					_mui_line_index_to_glyph_word(&te->measure, line, index,
							&start, &end);
					_mui_line_index_to_glyph_word(&te->measure,
							line, index, &start, &end);
					if (pos < te->click.start)
						_mui_textedit_select_signed(te, start, te->click.end);
					else
						_mui_textedit_select_signed(te, te->click.start, end);
				} else {
					if (pos < te->click.start)
						_mui_textedit_select_signed(te, pos, te->click.start);
					else
						_mui_textedit_select_signed(te, te->click.start, pos);
				}
			}
		}	break;
		case MUI_EVENT_WHEEL: {
			if (te->flags & MUI_CONTROL_TEXTEDIT_VERTICAL) {
				te->text_content.tl.y -= ev->wheel.delta * 10;
				_mui_textedit_clamp_text_frame(te);
				mui_control_inval(c);
			} else {
				te->text_content.tl.x -= ev->wheel.delta * 10;
				_mui_textedit_clamp_text_frame(te);
				mui_control_inval(c);
			}
			res = true;
		}	break;
		default:
			break;
	}
	return res;
}

static bool
mui_textedit_key(
		struct mui_control_t * 	c,
		mui_event_t * 			ev)
{
	mui_textedit_control_t *te = (mui_textedit_control_t *)c;

	_mui_textedit_show_carret(te);
	mui_glyph_line_array_t * me = &te->measure;
	if (ev->modifiers & MUI_MODIFIER_CTRL) {
		switch (ev->key.key) {
			case 'T': {
				te->trace = !te->trace;
				printf("TRACE %s\n", te->trace ? "ON" : "OFF");
			}	break;
			case 'D': {// dump text status and measures lines
				printf("Text:\n'%s'\n", te->text.e);
				printf("Text count: %d\n", te->text.count);
				printf("Text measure: %d\n", me->count);
				for (uint i = 0; i < me->count; i++) {
					mui_glyph_array_t * line = &me->e[i];
					printf("  line %d: %d\n", i, line->count);
					for (uint j = 0; j < line->count; j++) {
						mui_glyph_t * g = &line->e[j];
						printf("    %3d: %04x:%c x:%3f w:%3d\n",
								j, te->text.e[g->pos],
								te->text.e[g->pos] < ' ' ?
										'.' : te->text.e[g->pos],
								g->x, g->w);
					}
				}
				te->flags |= MUI_TEXT_DEBUG;
			}	break;
			case 'a': {
				_mui_textedit_select_signed(te, 0, te->text.count-1);
			}	break;
			case 'c': {
				if (te->sel.start != te->sel.end) {
					uint32_t start = _mui_glyph_to_byte_offset(me, te->sel.start);
					uint32_t end = _mui_glyph_to_byte_offset(me, te->sel.end);
					mui_clipboard_set(c->win->ui,
								te->text.e + start, end - start);
				}
			}	break;
			case 'x': {
				if (te->sel.start != te->sel.end) {
					uint32_t start = _mui_glyph_to_byte_offset(me, te->sel.start);
					uint32_t end = _mui_glyph_to_byte_offset(me, te->sel.end);
					mui_clipboard_set(c->win->ui,
								te->text.e + start, end - start);
					_mui_textedit_sel_delete(te, true, true);
				}
			}	break;
			case 'v': {
				uint32_t len;
				const uint8_t * clip = mui_clipboard_get(c->win->ui, &len);
				if (clip) {
					if (te->sel.start != te->sel.end)
						_mui_textedit_sel_delete(te, true, true);
					mui_utf8_insert(&te->text,
							_mui_glyph_to_byte_offset(me, te->sel.start),
							clip, len);
					_mui_textedit_refresh_measure(te);
					_mui_textedit_select_signed(te,
							te->sel.start + len, te->sel.start + len);
				}
			}	break;
		}
		return true;
	}
	switch (ev->key.key) {
		case MUI_KEY_UP: {
			uint line, index;
			_mui_glyph_to_line_index(me, te->sel.start, &line, &index);
			if (line > 0) {
				uint pos = _mui_line_index_to_glyph(me, line-1, index);
				if (ev->modifiers & MUI_MODIFIER_SHIFT) {
					_mui_textedit_select_signed(te, te->sel.start, pos);
				} else {
					_mui_textedit_select_signed(te, pos, pos);
				}
			}
		}	break;
		case MUI_KEY_DOWN: {
			uint line, index;
			_mui_glyph_to_line_index(me, te->sel.start, &line, &index);
			if (line < me->count-1) {
				uint pos = _mui_line_index_to_glyph(me, line+1, index);
				if (ev->modifiers & MUI_MODIFIER_SHIFT) {
					_mui_textedit_select_signed(te, te->sel.start, pos);
				} else {
					_mui_textedit_select_signed(te, pos, pos);
				}
			}
		}	break;
		case MUI_KEY_LEFT: {
			if (ev->modifiers & MUI_MODIFIER_SHIFT) {
				_mui_textedit_select_signed(te, te->sel.start - 1, te->sel.end);
			} else {
				if (te->sel.start == te->sel.end)
					_mui_textedit_select_signed(te, te->sel.start - 1, te->sel.start - 1);
				else
					_mui_textedit_select_signed(te, te->sel.start, te->sel.start);
			}
		}	break;
		case MUI_KEY_RIGHT: {
			if (ev->modifiers & MUI_MODIFIER_SHIFT) {
				_mui_textedit_select_signed(te, te->sel.start, te->sel.end + 1);
			} else {
				if (te->sel.start == te->sel.end)
					_mui_textedit_select_signed(te, te->sel.start + 1, te->sel.start + 1);
				else
					_mui_textedit_select_signed(te, te->sel.end, te->sel.end);
			}
		}	break;
		case MUI_KEY_BACKSPACE: {
			if (te->sel.start == te->sel.end) {
				if (te->sel.start > 0) {
					mui_utf8_delete(&te->text,
								_mui_glyph_to_byte_offset(me, te->sel.start - 1),
								1);
					_mui_textedit_refresh_measure(te);
					_mui_textedit_select_signed(te, te->sel.start - 1, te->sel.start - 1);
				}
			} else {
				_mui_textedit_sel_delete(te, true, true);
			}
		}	break;
		case MUI_KEY_DELETE: {
			if (te->sel.start == te->sel.end) {
				if (te->sel.start < te->text.count-1) {
					mui_utf8_delete(&te->text,
							_mui_glyph_to_byte_offset(me, te->sel.start), 1);
					_mui_textedit_refresh_measure(te);
					_mui_textedit_select_signed(te, te->sel.start, te->sel.start);
				}
			} else {
				_mui_textedit_sel_delete(te, true, true);
			}
		}	break;
		case '\t': {
			mui_control_switch_focus(c->win,
					ev->modifiers & MUI_MODIFIER_SHIFT ? -1 : 0);
		}	break;
		default:
			printf("%s key 0x%x\n", __func__, ev->key.key);
			if (ev->key.key == 13 && !(te->flags & MUI_CONTROL_TEXTEDIT_VERTICAL))
				return false;
			if (ev->key.key == 13 ||
						(ev->key.key >= 32 && ev->key.key < 127)) {
				if (te->sel.start != te->sel.end) {
					_mui_textedit_sel_delete(te, false, false);
					_mui_textedit_select_signed(te, te->sel.start, te->sel.start);
				}
				uint8_t k = ev->key.key;
				mui_utf8_insert(&te->text,
							_mui_glyph_to_byte_offset(me, te->sel.start), &k, 1);
				_mui_textedit_refresh_measure(te);
				_mui_textedit_select_signed(te,
							te->sel.start + 1, te->sel.start + 1);
			}
			break;
	}
	return true;
}

static bool
mui_cdef_textedit(
		struct mui_control_t * 	c,
		uint8_t 				what,
		void * 					param)
{
	if (!c)
		return false;
	mui_textedit_control_t *te = (mui_textedit_control_t *)c;
	switch (what) {
		case MUI_CDEF_INIT: {
			/* If we are the first text-edit created, register the timer */
			if (c->win->ui->carret_timer == MUI_TIMER_NONE)
				c->win->ui->carret_timer = mui_timer_register(c->win->ui,
							_mui_textedit_carret_timer, NULL,
							500 * MUI_TIME_MS);
			if (mui_window_isfront(c->win) &&
						c->win->control_focus.control == NULL)
				mui_control_set_focus(c);
		}	break;
		case MUI_CDEF_DRAW: {
			mui_drawable_t * dr = param;
			mui_textedit_draw(c->win, c, dr);
		}	break;
		case MUI_CDEF_DISPOSE: {
			mui_font_measure_clear(&te->measure);
			mui_utf8_clear(&te->text);
			/*
			 * If we are the focus, and we are being disposed, we need to
			 * find another control to focus on, if there is one.
			 * This is a bit tricky, as the control isn't attached to the
			 * window anymore, so we might have to devise another plan.
			 */
			if (c->win->control_focus.control == c) {
				mui_control_deref(&c->win->control_focus);
			}
		}	break;
		case MUI_CDEF_EVENT: {
		//	printf("%s event\n", __func__);
			mui_event_t *ev = param;
			switch (ev->type) {
				case MUI_EVENT_WHEEL:
				case MUI_EVENT_BUTTONUP:
				case MUI_EVENT_DRAG:
				case MUI_EVENT_BUTTONDOWN: {
					return mui_textedit_mouse(c, ev);
				}	break;
				case MUI_EVENT_KEYDOWN: {
					return mui_textedit_key(c, ev);
				}	break;
				default:
					break;
			}
		}	break;
		case MUI_CDEF_CAN_FOCUS: {
			return true;
		}	break;
		case MUI_CDEF_FOCUS: {
		//	int activate = *(int*)param;
		//	printf("%s activate %d\n", __func__, activate);
		//	mui_textedit_control_t *te = (mui_textedit_control_t *)c;
		}	break;
	}
	return false;
}

mui_control_t *
mui_textedit_control_new(
		mui_window_t * 	win,
		c2_rect_t 		frame,
		uint32_t 		flags)
{
	mui_textedit_control_t *te = (mui_textedit_control_t *)mui_control_new(
				win, MUI_CONTROL_TEXTEDIT, mui_cdef_textedit,
				frame,  NULL, 0, sizeof(mui_textedit_control_t));
	te->flags = flags;
	te->margin = (c2_pt_t){ .x = 4, .y = 2 };
	return &te->control;
}
