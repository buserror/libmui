/*
 * mui.c
 *
 * Copyright (C) 2023 Michel Pollet <buserror@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 */

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#include "mui_priv.h"

void
mui_init(
		mui_t *ui)
{
	// do NOT clear we rely on screen_size being set, this is temporary we'll
	// eventually use an 'option' struct to pass that sort of data (and the
	// colors)
	//memset(ui, 0, sizeof(*ui));
	ui->color.clear 	= MUI_COLOR(0xccccccff);
	ui->color.highlight = MUI_COLOR(0xd6fcc0ff);
	ui->timer.map = 0;
	ui->carret_timer = 0xff;
	TAILQ_INIT(&ui->windows);
	TAILQ_INIT(&ui->fonts);
	mui_font_init(ui);
	pixman_region32_init(&ui->redraw);
	c2_rect_t whole = C2_RECT(0, 0, ui->screen_size.x, ui->screen_size.y);
	pixman_region32_reset(&ui->inval, (pixman_box32_t*)&whole);
}

void
mui_dispose(
		mui_t *ui)
{
	pixman_region32_fini(&ui->inval);
	pixman_region32_fini(&ui->redraw);
	mui_utf8_free(&ui->clipboard);
	mui_font_dispose(ui);
	mui_window_t *w;
	while ((w = TAILQ_FIRST(&ui->windows))) {
		mui_window_dispose(w);
	}
}

void
mui_draw(
		mui_t *ui,
		mui_drawable_t *dr,
		uint16_t all)
{
	if (!(all || pixman_region32_not_empty(&ui->inval)))
		return;
	if (all) {
	//	printf("%s: all\n", __func__);
		c2_rect_t whole = C2_RECT(0, 0, dr->pix.size.x, dr->pix.size.y);
		pixman_region32_reset(&ui->inval, (pixman_box32_t*)&whole);
	}
	mui_drawable_set_clip(dr, NULL);

	/*
	 * Windows are drawn top to bottom, their area/rectangle is added to the
	 * done region, the done region (any windows that are overlaping others)
	 * is substracted to any other windows update region before drawing...
	 * once all windows are done, the 'done' region (sum of all the windows),
	 * is substracted from the 'desk' area and erased.
	 */
	pixman_region32_t done = {};

	mui_window_t * win;
	TAILQ_FOREACH_REVERSE(win, &ui->windows, windows, self) {
		pixman_region32_intersect_rect(&win->inval, &win->inval,
					win->frame.l, win->frame.t,
					c2_rect_width(&win->frame), c2_rect_height(&win->frame));

		mui_drawable_set_clip(dr, NULL);
		if (!all)
			mui_drawable_clip_push_region(dr, &win->inval);
		else
			mui_drawable_clip_push(dr, &win->frame);
		pixman_region32_clear(&win->inval);

		mui_drawable_clip_substract_region(dr, &done);
		mui_window_draw(win, dr);
	//	printf("  %s : %s\n", win->title, c2_rect_as_str(&win->frame));
		pixman_region32_union_rect(&done, &done,
			win->frame.l, win->frame.t,
			c2_rect_width(&win->frame), c2_rect_height(&win->frame));
	}

	mui_drawable_set_clip(dr, NULL);
	pixman_region32_t sect = {};
	c2_rect_t desk = C2_RECT(0, 0, dr->pix.size.x, dr->pix.size.y);
	pixman_region32_inverse(&sect, &done, (pixman_box32_t*)&desk);

	mui_drawable_clip_push_region(dr, &sect);

	pixman_image_fill_boxes(
			ui->color.clear.value ? PIXMAN_OP_SRC : PIXMAN_OP_CLEAR,
			mui_drawable_get_pixman(dr),
			&PIXMAN_COLOR(ui->color.clear), 1, (pixman_box32_t*)&desk);
	pixman_region32_fini(&sect);
	pixman_region32_fini(&done);

	pixman_region32_union(&ui->redraw, &ui->redraw, &ui->inval);
	pixman_region32_clear(&ui->inval);
	if (ui->draw_debug) {
		// save a png of the current screen
		ui->draw_debug = 0;
		printf("%s: saving debug.png\n", __func__);
	//	mui_drawable_save_to_png(dr, "debug.png");
	}
}

bool
mui_handle_event(
		mui_t *ui,
		mui_event_t *ev)
{
	bool res = false;
	if (!ev->when)
		ev->when = mui_get_time();
	switch (ev->type) {
		case MUI_EVENT_KEYUP:
		case MUI_EVENT_KEYDOWN: {
			if (ev->modifiers & MUI_MODIFIER_EVENT_TRACE)
				printf("%s modifiers %04x key %x\n", __func__,
							ev->modifiers, ev->key.key);
			mui_window_t *w, *safe;
			TAILQ_FOREACH_REVERSE_SAFE(w, &ui->windows, windows, self, safe) {
				if ((res = mui_window_handle_keyboard(w, ev))) {
					if (ev->modifiers & MUI_MODIFIER_EVENT_TRACE)
						printf("    window:%s handled it\n",
										w->title);
					break;
				}
			}
			if (ev->modifiers & MUI_MODIFIER_EVENT_TRACE)
				if (!res)
					printf("    no window handled it\n");
		}	break;
		case MUI_EVENT_BUTTONUP:
		case MUI_EVENT_BUTTONDOWN:
		case MUI_EVENT_WHEEL:
		case MUI_EVENT_DRAG: {
			if (ev->type == MUI_EVENT_BUTTONDOWN && ev->mouse.button > 1) {
				printf("%s: button %d not handled\n", __func__,
								ev->mouse.button);
				ui->draw_debug++;
				c2_rect_t whole = C2_RECT(0, 0, ui->screen_size.x, ui->screen_size.y);
				pixman_region32_reset(&ui->inval, (pixman_box32_t*)&whole);
			}
			if (ev->modifiers & MUI_MODIFIER_EVENT_TRACE)
				printf("%s %d mouse %d %3dx%3d capture:%s\n", __func__,
								ev->type, ev->mouse.button,
								ev->mouse.where.x, ev->mouse.where.y,
								ui->event_capture.window ?
										ui->event_capture.window->title :
										"(none)");
			/* Handle double click detection */
			if (ev->mouse.button < MUI_EVENT_BUTTON_MAX &&
					ev->type == MUI_EVENT_BUTTONDOWN) {
				int click_delta = ev->when - ui->last_click_stamp[ev->mouse.button];
				if (ui->last_click_stamp[ev->mouse.button] &&
						click_delta < (500 * MUI_TIME_MS)) {
					ui->last_click_stamp[ev->mouse.button] = 0;
					ev->mouse.count = 2;
				} else {
					ui->last_click_stamp[ev->mouse.button] = ev->when;
				}
			}
			if (ui->event_capture.window) {
				res = mui_window_handle_mouse(
						ui->event_capture.window, ev);
				break;
			} else {
				/* We can't use the REVERSE_SAFE macro here, as the window
				 * list can change quite a bit, especially when menus are
				 * involved */
				mui_window_t *w;
				w = TAILQ_LAST(&ui->windows, windows);
				int done = 0;
				while (w && !done) {
					mui_window_lock(w);
					// in case 'prev' gets deleted, keep a ref on it.
					mui_window_ref_t prev = {};
					mui_window_ref(&prev, TAILQ_PREV(w, windows, self),
								FCC('H', 'O', 'L', 'D'));
					if ((res = mui_window_handle_mouse(w, ev))) {
						if (ev->modifiers & MUI_MODIFIER_EVENT_TRACE)
							printf("    window:%s handled it\n",
											w->title);
						done = 1;
					}
					mui_window_unlock(w);	// COULD delete window here
					w = prev.window;		// this might have been NULLed
					mui_window_deref(&prev);
				}
			}
			if (ev->modifiers & MUI_MODIFIER_EVENT_TRACE)
				if (!res)
					printf("    no window handled it\n");
		}	break;
		default:
			break;
	}
	return res;
}

static uint16_t
_mui_simplify_mods(
		uint16_t mods)
{
	uint16_t res = 0;
	if (mods & MUI_MODIFIER_SHIFT)
		res |= MUI_MODIFIER_RSHIFT;
	if (mods & MUI_MODIFIER_CTRL)
		res |= MUI_MODIFIER_RCTRL;
	if (mods & MUI_MODIFIER_ALT)
		res |= MUI_MODIFIER_RALT;
	if (mods & MUI_MODIFIER_SUPER)
		res |= MUI_MODIFIER_RSUPER;
	return res;
}

bool
mui_event_match_key(
		mui_event_t *ev,
		mui_key_equ_t key_equ)
{
	if (ev->type != MUI_EVENT_KEYUP && ev->type != MUI_EVENT_KEYDOWN)
		return false;
	if (toupper(ev->key.key) != toupper(key_equ.key))
		return false;
	if (_mui_simplify_mods(ev->modifiers) !=
				_mui_simplify_mods(key_equ.mod))
		return false;
	return true;
}

mui_timer_id_t
mui_timer_register(
		mui_t *ui,
		mui_timer_p cb,
		void *param,
		uint32_t delay)
{
	if (ui->timer.map == (uint64_t)-1L) {
		fprintf(stderr, "%s ran out of timers\n", __func__);
		return MUI_TIMER_NONE;
	}
	mui_timer_id_t ti = __builtin_ffsl(~ui->timer.map) - 1;
//	printf("%s:%d delay %d\n", __func__, ti, delay);
	ui->timer.map |= 1 << ti;
	ui->timer.timers[ti].cb = cb;
	ui->timer.timers[ti].param = param;
	ui->timer.timers[ti].when = mui_get_time() + delay;
	return ti;
}

mui_time_t
mui_timer_reset(
		struct mui_t *	ui,
		mui_timer_id_t 	id,
		mui_timer_p 	cb,
		mui_time_t 		delay)
{
	if (id >= MUI_TIMER_COUNT)
		return 0;
	if (!(ui->timer.map & (1L << id)) ||
				ui->timer.timers[id].cb != cb) {
	//	printf("%s:%d not active\n", __func__, id);
		return 0;
	}
	mui_time_t res = 0;
	uint64_t now = mui_get_time();
	if (ui->timer.timers[id].when > now)
		res = ui->timer.timers[id].when - now;
	ui->timer.timers[id].when = now + delay;
	if (delay == 0) {
		ui->timer.map &= ~(1L << id);
	//	printf("%s: %d removed\n", __func__, id);
	}
	return res;
}

void
mui_timers_run(
		mui_t *ui )
{
	uint64_t now = mui_get_time();
	uint64_t map = ui->timer.map;
	while (map) {
		int ti = __builtin_ffsl(map) - 1;
		map &= ~(1 << ti);
		if (ui->timer.timers[ti].when > now)
			continue;
		mui_time_t r = ui->timer.timers[ti].cb(ui, now,
							ui->timer.timers[ti].param);
		if (r == 0)
			ui->timer.map &= ~(1 << ti);
		else
			ui->timer.timers[ti].when += r;
	}
}

void
mui_run(
		mui_t *ui)
{
	mui_timers_run(ui);
}

bool
mui_has_active_windows(
		mui_t *ui)
{
	mui_window_t *win;
	TAILQ_FOREACH(win, &ui->windows, self) {
		if (mui_menubar_window(win) || win->flags.hidden)
			continue;
		return true;
	}
	return false;
}

void
mui_clipboard_set(
		mui_t * 		ui,
		const uint8_t * utf8,
		uint		 	len)
{
	len = len ? len : strlen((char*)utf8);
	mui_utf8_clear(&ui->clipboard);
	mui_utf8_insert(&ui->clipboard, 0, utf8, len);
	mui_utf8_add(&ui->clipboard, 0);
	mui_window_action(
			ui->menubar.window, MUI_CLIPBOARD_CHANGED, NULL);
}

const uint8_t *
mui_clipboard_get(
		mui_t * 		ui,
		uint		 *	len)
{
	mui_window_action(
			ui->menubar.window, MUI_CLIPBOARD_REQUEST, NULL);
	if (len)
		*len = ui->clipboard.count > 1 ? ui->clipboard.count - 1 : 0;
	return ui->clipboard.count ? ui->clipboard.e : NULL;
}


void
mui_refqueue_init(
		mui_refqueue_t *queue)
{
	TAILQ_INIT(&queue->head);
}

uint
mui_refqueue_dispose(
		mui_refqueue_t *queue)
{
	uint res = 0;
	struct mui_ref_t *ref, *safe;
	TAILQ_FOREACH_SAFE(ref, &queue->head, self, safe) {
		if (ref->count) {
			ref->count--;
			if (ref->count) {
			//	printf("%s: ref %4.4s count %2d\n", __func__,
			//				(char*)&ref->kind, ref->count);
				res++;
				continue;
			}
		}
		TAILQ_REMOVE(&queue->head, ref, self);
		ref->queue = NULL;
		if (ref->deref)
			ref->deref(ref);
	}
	return res;
}

/* Remove reference 'ref' from it's reference queue */
void
mui_ref_deref(
		struct mui_ref_t *	ref)
{
	if (!ref)
		return;
	if (ref->queue)
		TAILQ_REMOVE(&ref->queue->head, ref, self);
	if (ref->alloc) {
		free(ref);
		return;
	}
	ref->queue = NULL;
	ref->deref = NULL;
	ref->count = 0;
}

static void
mui_ref_deref_control(
		struct mui_ref_t *	_ref)
{
	mui_control_ref_t *		ref = (mui_control_ref_t*)_ref;
	ref->control = NULL;
}

mui_control_ref_t *
mui_control_ref(
		mui_control_ref_t *		ref,
		struct mui_control_t *	control,
		uint32_t 				kind)
{
	if (!control)
		return NULL;
	if (ref && ref->ref.queue) {
		printf("%s Warning: ref %p %4.4s already in queue\n",
				__func__, ref, (char*)&kind);
		if (ref->control != control) {
			printf("%s ERROR: ref %p control %p != %p\n",
					__func__, ref, ref->control, control);
		}
		return NULL;
	}
	struct mui_ref_t * res = ref ? ref : calloc(1, sizeof(*ref));
	res->alloc = !ref;
	res->queue = &control->refs;
	res->kind = kind;
	res->deref = mui_ref_deref_control;
	res->count = 1;
	ref = (mui_control_ref_t*)res;
	ref->control = control;
	TAILQ_INSERT_TAIL(&control->refs.head, res, self);
	return ref;
}

void
mui_control_deref(
		mui_control_ref_t *		ref)
{
	ref->control = NULL;
	mui_ref_deref(&ref->ref);
}

static void
mui_ref_deref_window(
		struct mui_ref_t *	_ref)
{
	mui_window_ref_t *		ref = (mui_window_ref_t*)_ref;
	ref->window = NULL;
}

mui_window_ref_t *
mui_window_ref(
		mui_window_ref_t *		ref,
		struct mui_window_t * 	win,
		uint32_t 				kind)
{
	if (!win)
		return NULL;
	if (ref && ref->ref.queue) {
		printf("%s Warning: ref %p %4.4s already in queue\n",
				__func__, ref, (char*)&kind);
		return NULL;
	}
	struct mui_ref_t * res = ref ? ref : calloc(1, sizeof(*ref));
	res->alloc = !ref;
	res->queue = &win->refs;
	res->kind = kind;
	res->deref = mui_ref_deref_window;
	res->count = 1;
	ref = (mui_window_ref_t*)res;
	ref->window = win;
	TAILQ_INSERT_TAIL(&win->refs.head, res, self);
	return ref;
}

void
mui_window_deref(
		mui_window_ref_t *		ref)
{
	ref->window = NULL;
	mui_ref_deref(&ref->ref);
}
