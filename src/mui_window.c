/*
 * mui_window.c
 *
 * Copyright (C) 2023 Michel Pollet <buserror@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 */

#include <stdio.h>
#include <stdlib.h>

#include "mui_priv.h"
#include "cg.h"

enum mui_window_part_e {
	MUI_WINDOW_PART_NONE = 0,
	MUI_WINDOW_PART_CONTENT,
	MUI_WINDOW_PART_TITLE,
	MUI_WINDOW_PART_FRAME,
	MUI_WINDOW_PART_COUNT,
};

static void
mui_window_update_rects(
		mui_window_t *win,
		mui_font_t * main )
{
	int title_height = main->size;
	c2_rect_t content = win->frame;
	c2_rect_inset(&content, 4, 4);
	content.t += title_height - 1;
	win->content = content;
}

void
mui_titled_window_draw(
		struct mui_t *ui,
		mui_window_t *win,
		mui_drawable_t *dr)
{
	mui_font_t * main = mui_font_find(ui, "main");
	if (!main)
		return;
	mui_window_update_rects(win, main);
	int title_height = main->size;

	struct cg_ctx_t * cg 	= mui_drawable_get_cg(dr);
	mui_color_t frameFill 	= MUI_COLOR(0xbbbbbbff);
	mui_color_t contentFill = MUI_COLOR(0xf0f0f0ff);
	mui_color_t frameColor 	= MUI_COLOR(0x000000ff);
	mui_color_t decoColor 	= MUI_COLOR(0x999999ff);
	mui_color_t titleColor 	= MUI_COLOR(0x000000aa);
	mui_color_t dimTitleColor 	= MUI_COLOR(0x00000055);

	cg_set_line_width(cg, 1);
	cg_rectangle(cg, win->frame.l + 0.5f, win->frame.t + 0.5f,
					c2_rect_width(&win->frame) - 1, c2_rect_height(&win->frame) - 1);
	cg_rectangle(cg, win->content.l + 0.5f, win->content.t + 0.5f,
					c2_rect_width(&win->content) - 1, c2_rect_height(&win->content) - 1);
	cg_set_source_color(cg, &CG_COLOR(frameFill));
	cg_fill_preserve(cg);
	cg_set_source_color(cg, &CG_COLOR(frameColor));
	cg_stroke(cg);

	bool isFront = mui_window_front(ui) == win;
	if (isFront) {
		const int lrMargin = 6;
		const int steps = 6;
		cg_set_line_width(cg, 2);
		for (int i = 1; i < (title_height + 4) / steps; i++) {
			cg_move_to(cg, win->frame.l + lrMargin, win->frame.t + i * steps);
			cg_line_to(cg, win->frame.r - lrMargin, win->frame.t + i * steps);
		}
		cg_set_source_color(cg, &CG_COLOR(decoColor));
		cg_stroke(cg);
	}
	if (win->title) {
		stb_ttc_measure m = {};
		mui_font_text_measure(main, win->title, &m);

		int title_width = m.x1 - m.x0;
		c2_rect_t title = win->frame;
		c2_rect_offset(&title, 0, 1);
		title.b = title.t + title_height;
		title.l += (c2_rect_width(&win->frame) / 2) - (title_width / 2);
		title.r = title.l + title_width;
		if (isFront) {
			c2_rect_t titleBack = title;
			c2_rect_inset(&titleBack, -6, 0);
			cg_round_rectangle(cg, titleBack.l, titleBack.t,
					c2_rect_width(&titleBack), c2_rect_height(&titleBack), 12, 12);
			cg_set_source_color(cg, &CG_COLOR(frameFill));
			cg_fill(cg);
		}
		mui_font_text_draw(main, dr,
				C2_PT(-m.x0 + 1 + title.l, title.t + 0),
				win->title, strlen(win->title),
				isFront ? titleColor : dimTitleColor);
	}
	cg_set_source_color(cg, &CG_COLOR(contentFill));
	cg_rectangle(cg, win->content.l + 0.5f, win->content.t + 0.5f,
					c2_rect_width(&win->content) - 1, c2_rect_height(&win->content) - 1);
	cg_fill(cg);
}

static bool
mui_wdef_titlewindow(
		struct mui_window_t * win,
		uint8_t 		what,
		void * 			param)
{
	switch (what) {
		case MUI_WDEF_DRAW:
			mui_titled_window_draw(win->ui, win, param);
			break;
		case MUI_WDEF_SELECT:
//			mui_window_inval(win, NULL);
			if (win->control_focus.control) {
				int activate = 1;
				if (win->control_focus.control->cdef)
					win->control_focus.control->cdef(
								win->control_focus.control,
								MUI_CDEF_FOCUS, &activate);
			}
			break;
		case MUI_WDEF_DESELECT:
			if (win->control_focus.control) {
				int activate = 0;
				if (win->control_focus.control->cdef)
					win->control_focus.control->cdef(
								win->control_focus.control,
								MUI_CDEF_FOCUS, &activate);
			}
			break;
		case MUI_WDEF_DISPOSE:
			break;
	}
	return false;
}

mui_window_t *
mui_window_create(
		struct mui_t *ui,
		c2_rect_t frame,
		mui_wdef_p wdef,
		uint8_t layer,
		const char *title,
		uint32_t instance_size)
{
	mui_window_t * w = calloc(1,
						instance_size >= sizeof(*w) ?
								instance_size : sizeof(*w));
	w->ui = ui;
	w->frame = frame;
	w->title = title ? strdup(title) : NULL;
	w->wdef = wdef ? wdef : mui_wdef_titlewindow;
	w->flags.layer = layer;
	mui_refqueue_init(&w->refs);
	TAILQ_INIT(&w->controls);
	STAILQ_INIT(&w->actions);
	pixman_region32_init(&w->inval);
	TAILQ_INSERT_HEAD(&ui->windows, w, self);
	mui_window_select(w); // place it in it's own layer
	mui_font_t * main = mui_font_find(ui, "main");
	mui_window_update_rects(w, main);
	mui_window_inval(w, NULL); // just to mark the UI dirty

	return w;
}

void
_mui_window_free(
		mui_window_t *win)
{
	if (!win)
		return;
	pixman_region32_fini(&win->inval);
	mui_control_t * c;
	while ((c = TAILQ_FIRST(&win->controls))) {
		mui_control_dispose(c);
	}
	if (win->title)
		free(win->title);
	free(win);
}

void
mui_window_dispose_actions(
		mui_window_t * 	win)
{
	if (!win)
		return;
	mui_action_t *a;
	while ((a = STAILQ_FIRST(&win->actions)) != NULL) {
		STAILQ_REMOVE_HEAD(&win->actions, self);
		free(a);
	}
}

void
mui_window_dispose(
		mui_window_t *win)
{
	if (!win)
		return;
	if (!win->flags.disposed) {
		win->flags.disposed = true;
		bool was_front = mui_window_isfront(win);
		mui_window_action(win, MUI_WINDOW_ACTION_CLOSE, NULL);
		mui_window_inval(win, NULL); // just to mark the UI dirty
		if (win->wdef)
			win->wdef(win, MUI_WDEF_DISPOSE, NULL);
		win->flags.hidden = true;
		struct mui_t *ui = win->ui;
		TAILQ_REMOVE(&ui->windows, win, self);
		mui_window_dispose_actions(win);
		if (was_front) {
			mui_window_t * front = mui_window_front(ui);
			if (front) {
				mui_window_inval(front, NULL);
				if (front->wdef)
					front->wdef(front, MUI_WDEF_SELECT, NULL);
			}
		}
	}
	if (mui_refqueue_dispose(&win->refs) != 0) {
		// we have some references, we'll have to wait
	//	printf("%s Warning: window %s still has references\n",
	//			__func__, win->title);
		return;
	}
	_mui_window_free(win);
}

void
mui_window_draw(
		mui_window_t *win,
		mui_drawable_t *dr)
{
	if (!win)
		return;
	if (win->flags.hidden)
		return;
	mui_drawable_clip_push(dr, &win->frame);
	if (win->wdef)
		win->wdef(win, MUI_WDEF_DRAW, dr);
	struct cg_ctx_t * cg 	= mui_drawable_get_cg(dr);
	cg_save(cg);
//	cg_translate(cg, content.l, content.t);
	mui_control_t * c, *safe;
	TAILQ_FOREACH_SAFE(c, &win->controls, self, safe) {
		mui_control_draw(win, c, dr);
	}
	cg_restore(cg);

	mui_drawable_clip_pop(dr);
}

/*
 * Keys are passed first to the control that is in focus (if any), then
 * to all the others in sequence until someone handles it (or not).
 */
bool
mui_window_handle_keyboard(
		mui_window_t *win,
		mui_event_t *event)
{
	if (win->flags.hidden)
		return false;
	if (!mui_window_isfront(win))
		return false;
	if (win->wdef && win->wdef(win, MUI_WDEF_EVENT, event)) {
//		printf("%s  %s handled it\n", __func__, win->title);
		return true;
	}
//	printf("%s %s checkint controls\n", __func__, win->title);
	/*
	 * Start with the control in focus, if there's any
	 */
	mui_control_t * first = win->control_focus.control ?
								win->control_focus.control :
								TAILQ_FIRST(&win->controls);
	mui_control_t * c = first;
	while (c) {
		if (mui_control_event(c, event)) {
		//	printf("%s control %s handled it\n", __func__, c->title);
			return true;
		}
		c = TAILQ_NEXT(c, self);
		if (!c)
			c = TAILQ_FIRST(&win->controls);
		if (c == first)
			break;
	}
	return false;
}

bool
mui_window_handle_mouse(
		mui_window_t *win,
		mui_event_t *event)
{
	if (win->flags.hidden)
		return false;
	if (win->wdef && win->wdef(win, MUI_WDEF_EVENT, event))
		return true;
	switch (event->type) {
		case MUI_EVENT_WHEEL: {
			if (!c2_rect_contains_pt(&win->frame, &event->wheel.where))
				return false;
			mui_control_t * c = mui_control_locate(win, event->wheel.where);
			if (!c)
				return false;
			if (c->cdef && c->cdef(c, MUI_CDEF_EVENT, event)) {
				return true;
			}
		}	break;
		case MUI_EVENT_BUTTONDOWN: {
			if (!c2_rect_contains_pt(&win->frame, &event->mouse.where))
				return false;
			mui_control_t * c = mui_control_locate(win, event->mouse.where);
			/* if modifiers like control is down, don't select */
			if (!(event->modifiers & MUI_MODIFIER_CTRL))
				mui_window_select(win);
			if (mui_window_front(win->ui) != win)
				c = NULL;
			if (!c) {
				/* find where we clicked in the window */
				mui_window_ref(&win->ui->event_capture, win,
							FCC('E', 'V', 'C', 'P'));
				win->click_loc = event->mouse.where;
				c2_pt_offset(&win->click_loc, -win->frame.l, -win->frame.t);
				win->flags.hit_part = MUI_WINDOW_PART_FRAME;
				if (event->mouse.where.y < win->content.t)
					win->flags.hit_part = MUI_WINDOW_PART_TITLE;
				else if (c2_rect_contains_pt(&win->content, &event->mouse.where))
					win->flags.hit_part = MUI_WINDOW_PART_CONTENT;
			} else
				win->flags.hit_part = MUI_WINDOW_PART_CONTENT;
			if (c) {
				if (c->cdef && c->cdef(c, MUI_CDEF_EVENT, event)) {
	//			c->state = MUI_CONTROL_STATE_CLICKED;
					mui_control_ref(&win->control_clicked, c,
							FCC('E', 'V', 'C', 'C'));
				}
			}
			return true;
		}	break;
		case MUI_EVENT_DRAG:
			if (win->flags.hit_part == MUI_WINDOW_PART_TITLE) {
				c2_rect_t frame = win->frame;
				c2_rect_offset(&frame,
						-win->frame.l + event->mouse.where.x - win->click_loc.x,
						-win->frame.t + event->mouse.where.y - win->click_loc.y);
				// todo, get that visibel rectangle from somewhere else
				c2_rect_t screen = { .br = win->ui->screen_size };
				screen.t += 35;
				c2_rect_t title_bar = frame;
				title_bar.b = title_bar.t + 35; // TODO fix that
				if (c2_rect_intersect_rect(&title_bar, &screen)) {
					c2_rect_t o;
					c2_rect_clip_rect(&title_bar, &screen, &o);
					if (c2_rect_width(&o) > 10 && c2_rect_height(&o) > 10) {
						mui_window_inval(win, NULL);	// old frame
						win->frame = frame;
						mui_window_inval(win, NULL);	// new frame
					}
				}
			//	mui_window_inval(win, NULL);
				return true;
			}
			if (win->control_clicked.control) {
				mui_control_t * c = win->control_clicked.control;
				if (c->cdef && c->cdef(c, MUI_CDEF_EVENT, event)) {
					return true;
				} else
					mui_control_deref(&win->control_clicked);
			}
			return win->flags.hit_part != MUI_WINDOW_PART_NONE;
			break;
		case MUI_EVENT_BUTTONUP: {
			int part = win->flags.hit_part;
			win->flags.hit_part = MUI_WINDOW_PART_NONE;
			mui_window_deref(&win->ui->event_capture);
			if (win->control_clicked.control) {
				mui_control_t * c = win->control_clicked.control;
				mui_control_deref(&win->control_clicked);
				if (c->cdef && c->cdef(c, MUI_CDEF_EVENT, event))
					return true;
			}
			return part != MUI_WINDOW_PART_NONE;
		}	break;
		case MUI_EVENT_MOUSEENTER:
		case MUI_EVENT_MOUSELEAVE:
			break;
		default:
			break;
	}
//	printf("MOUSE %s button %d\n", __func__, event->mouse.button);
//	printf("MOUSE %s %s\n", __func__, c->title);
	return false;
}

void
mui_window_inval(
		mui_window_t *win,
		c2_rect_t * r)
{
	if (!win || win->flags.hidden)
		return;
	c2_rect_t frame = win->frame;
	c2_rect_t forward = {};

	if (!r) {
	//	printf("%s %s inval %s (whole)\n", __func__, win->title, c2_rect_as_str(&frame));
		pixman_region32_reset(&win->inval, (pixman_box32_t*)&frame);
		forward = frame;

		mui_window_t * w, *save;
		TAILQ_FOREACH_SAFE(w, &win->ui->windows, self, save) {
			if (w == win || !c2_rect_intersect_rect(&w->frame, &forward))
				continue;
			pixman_region32_union_rect(&w->inval, &w->inval,
				forward.l, forward.t,
				c2_rect_width(&forward), c2_rect_height(&forward));
		}
	} else {
		c2_rect_t local = *r;
		c2_rect_offset(&local, win->content.l, win->content.t);
		forward = local;

		pixman_region32_union_rect(&win->inval, &win->inval,
			forward.l, forward.t,
			c2_rect_width(&forward), c2_rect_height(&forward));
	}
	if (c2_rect_isempty(&forward))
		return;
	pixman_region32_union_rect(&win->ui->inval, &win->ui->inval,
			forward.l, forward.t,
			c2_rect_width(&forward), c2_rect_height(&forward));
}

mui_window_t *
mui_window_front(
		struct mui_t *ui)
{
	if (!ui)
		return NULL;
	mui_window_t * w, *save;
	TAILQ_FOREACH_REVERSE_SAFE(w, &ui->windows, windows, self, save) {
		if (w->flags.hidden)
			continue;
		if (w->flags.layer < MUI_WINDOW_MENUBAR_LAYER)
			return w;
	}
	return NULL;
}

bool
mui_window_isfront(
		mui_window_t *win)
{
	if (!win)
		return NULL;
	mui_window_t * next = TAILQ_NEXT(win, self);
	while (next && next->flags.hidden)
		next = TAILQ_NEXT(next, self);
	if (!next)
		return true;
	if (next->flags.layer > win->flags.layer)
		return true;
	return false;
}

bool
mui_window_select(
		mui_window_t *win)
{
	bool res = false;
	if (!win)
		return false;
	mui_window_t *last = NULL;
	if (mui_window_isfront(win))
		goto done;
	res = true;
	mui_window_inval(win, NULL);
	TAILQ_REMOVE(&win->ui->windows, win, self);
	mui_window_t *w, *save;
	TAILQ_FOREACH_SAFE(w, &win->ui->windows, self, save) {
		if (w->flags.layer > win->flags.layer) {
			TAILQ_INSERT_BEFORE(w, win, self);
			goto done;
		}
		last = w;
	}
	TAILQ_INSERT_TAIL(&win->ui->windows, win, self);
	if (win->wdef)
		win->wdef(win, MUI_WDEF_SELECT, NULL);
done:
	if (last) {// we are deselecting this one, so redraw it
		mui_window_inval(last, NULL);
		if (last->wdef)
			win->wdef(last, MUI_WDEF_DESELECT, NULL);
	}
#if 0
	printf("%s %s res:%d stack is now:\n", __func__, win->title, res);
	TAILQ_FOREACH(w, &win->ui->windows, self) {
		printf("  L:%2d T:%s\n", w->flags.layer, w->title);
	}
#endif
	return res;
}

void
mui_window_lock(
		mui_window_t *win)
{
	if (!win)
		return;
	if (!win->lock.window) {
		mui_window_ref(&win->lock, win, FCC('l', 'o', 'c', 'k'));
		win->lock.ref.count = 10; // prevent it from being deleted
	} else {
		win->lock.ref.count += 10;
	}
//	printf("%s: window %s locked %2d\n",
//			__func__, win->title, win->lock.ref.count);
}

mui_window_t*
mui_window_unlock(
		mui_window_t *win)
{
	if (!win)
		return NULL;
	if (win->lock.window) {
		if (win->lock.ref.trace)
			printf("%s: window %s was locked\n",
				__func__, win->title);
		if (win->lock.ref.count > 10) {
			win->lock.ref.count -= 10;
		} else {	// window was disposed of
			int delete = win->lock.ref.count < 10;
			// we are the last one, remove the lock
			if (win->lock.ref.trace)
				printf("%s: window %s unlocked delete %d\n",
					__func__, win->title, delete);
			mui_window_deref(&win->lock);
			if (delete) {
				mui_window_dispose(win);
				win = NULL;
			}
		}
	} else {
	//	if (win->lock.ref.trace)
			printf("%s: window %s was not locked\n", __func__, win->title);
	}
	return win;
}

void
mui_window_action(
		mui_window_t * 	win,
		uint32_t 		what,
		void * 			param )
{
	if (!win)
		return;
	mui_window_lock(win);
	mui_action_t *a, *safe;
	STAILQ_FOREACH_SAFE(a, &win->actions, self, safe) {
		if (!a->window_cb)
			continue;
		a->window_cb(win, a->cb_param, what, param);
	}
	mui_window_unlock(win);
}

void
mui_window_set_action(
		mui_window_t * 	win,
		mui_window_action_p 	cb,
		void * 			param )
{
	if (!win || !cb)
		return;

	mui_action_t *a;
	STAILQ_FOREACH(a, &win->actions, self) {
		if (a->window_cb == cb && a->cb_param == param)
			return;
	}
	a = calloc(1, sizeof(*a));
	a->window_cb = cb;
	a->cb_param = param;
	STAILQ_INSERT_TAIL(&win->actions, a, self);
}

mui_window_t *
mui_window_get_by_id(
		struct mui_t *ui,
		uint32_t uid )
{
	mui_window_t *w;
	TAILQ_FOREACH(w, &ui->windows, self) {
		if (w->uid == uid)
			return w;
	}
	return NULL;
}

void
mui_window_set_id(
		mui_window_t *win,
		uint32_t uid)
{
	if (!win)
		return;
	win->uid = uid;
}
