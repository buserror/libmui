/*
 * mui_controls.c
 *
 * Copyright (C) 2023 Michel Pollet <buserror@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 */

#include <stdio.h>
#include <stdlib.h>

#include "mui.h"
#include "mui_priv.h"

const mui_control_color_t mui_control_color[MUI_CONTROL_STATE_COUNT] = {
	[MUI_CONTROL_STATE_NORMAL] = {
		.fill = MUI_COLOR(0xeeeeeeff),
		.frame = MUI_COLOR(0x000000ff),
		.text = MUI_COLOR(0x000000ff),
	},
	[MUI_CONTROL_STATE_HOVER] = {
		.fill = MUI_COLOR(0xaaaaaaff),
		.frame = MUI_COLOR(0x000000ff),
		.text = MUI_COLOR(0x0000ffff),
	},
	[MUI_CONTROL_STATE_CLICKED] = {
		.fill = MUI_COLOR(0x777777ff),
		.frame = MUI_COLOR(0x000000ff),
		.text = MUI_COLOR(0xffffffff),
	},
	[MUI_CONTROL_STATE_DISABLED] = {
		.fill = MUI_COLOR(0xeeeeeeff),
		.frame = MUI_COLOR(0x666666ff),
		.text = MUI_COLOR(0xccccccff),
	},
};

static void
mui_control_dispose_actions(
		mui_control_t * 	c);

void
mui_control_draw(
		mui_window_t * 	win,
		mui_control_t * c,
		mui_drawable_t *dr )
{
	if (!c)
		return;
	if (c->cdef)
		c->cdef(c, MUI_CDEF_DRAW, dr);
}

mui_control_t *
mui_control_new(
		mui_window_t * 	win,
		uint32_t 		type,
		mui_cdef_p 		cdef,
		c2_rect_t 		frame,
		const char *	title,
		uint32_t 		uid,
		uint32_t 		instance_size )
{
	if (!win)
		return NULL;
	mui_control_t *c = calloc(1, instance_size >= sizeof(*c) ?
										instance_size : sizeof(*c));
	c->type = type;
	c->cdef = cdef;
	c->frame = frame;
	c->title = title ? strdup(title) : NULL;
	c->win = win;
	c->uid = uid;
	mui_refqueue_init(&c->refs);
	STAILQ_INIT(&c->actions);
	TAILQ_INSERT_TAIL(&win->controls, c, self);
	if (c->cdef)
		c->cdef(c, MUI_CDEF_INIT, NULL);
	// should we auto-focus the control? not sure..
//	mui_control_set_focus(c);
	return c;
}

void
_mui_control_free(
		mui_control_t * c )
{
	if (!c)
		return;
	if (c->title)
		free(c->title);
	c->title = NULL;
	free(c);
}


void
mui_control_dispose(
		mui_control_t * c )
{
	if (!c)
		return;
	if (c->win) {
		TAILQ_REMOVE(&c->win->controls, c, self);
		if (c->cdef)
			c->cdef(c, MUI_CDEF_DISPOSE, NULL);
		c->win = NULL;
		mui_control_dispose_actions(c);
	}
	if (mui_refqueue_dispose(&c->refs) != 0) {
	//	fprintf(stderr, "%s Warning: control %s still has a lock\n",
	//			__func__, c->title);
		return;
	}
	_mui_control_free(c);
}

uint32_t
mui_control_get_type(
		mui_control_t * c )
{
	if (!c)
		return 0;
	return c->type;
}
uint32_t
mui_control_get_uid(
		mui_control_t * c )
{
	if (!c)
		return 0;
	return c->uid;
}

mui_control_t *
mui_control_locate(
		mui_window_t * 	win,
		c2_pt_t 		pt )
{
	if (!win)
		return NULL;
	mui_control_t * c;
	TAILQ_FOREACH(c, &win->controls, self) {
		c2_rect_t f = c->frame;
		c2_rect_offset(&f, win->content.l, win->content.t);
		if (c2_rect_contains_pt(&f, &pt))
			return c;
	}
	return NULL;
}

static mui_time_t
_mui_control_highlight_timer_cb(
		struct mui_t * mui,
		mui_time_t now,
		void * param)
{
	mui_control_ref_t *ref = param;
	mui_control_t * c = ref->control;
	if (!c) {
		mui_control_deref(ref);
		return 0;
	}
//	printf("%s: %s\n", __func__, c->title);
	mui_control_set_state(c, MUI_CONTROL_STATE_NORMAL);
	if (c->cdef)
		c->cdef(c, MUI_CDEF_SELECT, NULL);
	mui_control_action(c, MUI_CONTROL_ACTION_SELECT, NULL);
	mui_control_deref(ref);

	return 0;
}

int32_t
mui_control_get_value(
		mui_control_t * c)
{
	if (!c)
		return 0;
	return c->value;
}

int32_t
mui_control_set_value(
		mui_control_t * c,
		int32_t 		value)
{
	if (!c)
		return 0;
	if (c->cdef && c->cdef(c, MUI_CDEF_SET_VALUE, &value))
		return c->value;
	if (value != (int)c->value)
		mui_control_inval(c);
	c->value = value;
	return c->value;
}

bool
mui_control_event(
		mui_control_t * c,
		mui_event_t * 	ev )
{
	if (!c)
		return false;
	bool res = false;

	res = c->cdef && c->cdef(c, MUI_CDEF_EVENT, ev);
	if (res || !c->key_equ.key)
		return res;
	switch (ev->type) {
		case MUI_EVENT_KEYDOWN:
			if (c->state != MUI_CONTROL_STATE_DISABLED &&
					mui_event_match_key(ev, c->key_equ)) {
				mui_control_set_state(c, MUI_CONTROL_STATE_CLICKED);

				mui_control_ref_t * ref = mui_control_ref(NULL, c,
											FCC('h', 'i', 'g', 'h'));
				mui_timer_register(
						c->win->ui, _mui_control_highlight_timer_cb,
						ref, MUI_TIME_SECOND / 10);
				res = true;
			}
			break;
		default: break;
	}
	return res;
}

void
mui_control_set_frame(
		mui_control_t * c,
		c2_rect_t *		frame )
{
	if (!c || !frame)
		return;
	c2_rect_t old = c->frame;
	if (c2_rect_equal(&old, frame))
		return;
	mui_control_inval(c);	// old position
	c->frame = *frame;
	if (c->cdef && c->cdef(c, MUI_CDEF_SET_FRAME, frame))
		return;
	mui_control_inval(c);	// new position
}

void
mui_control_inval(
		mui_control_t * c )
{
	if (!c)
		return;
	mui_window_inval(c->win, &c->frame);
}

void
mui_control_set_state(
		mui_control_t * c,
		uint32_t 		state )
{
	if (!c)
		return;
	if (c->cdef && c->cdef(c, MUI_CDEF_SET_STATE, &state))
		return;
	if (c->state == state)
		return;
	c->state = state;
	mui_control_inval(c);
}

uint32_t
mui_control_get_state(
		mui_control_t * c )
{
	if (!c)
		return 0;
	return c->state;
}

const char *
mui_control_get_title(
		mui_control_t * c )
{
	if (!c)
		return NULL;
	return c->title;
}

void
mui_control_set_title(
		mui_control_t * c,
		const char * 	text )
{
	if (!c)
		return;
	if (c->cdef && c->cdef(c, MUI_CDEF_SET_TITLE, (void*)text))
		return;
	if (text && c->title && !strcmp(text, c->title))
		return;
	if (c->title)
		free(c->title);
	c->title = text ? strdup(text) : NULL;
	mui_control_inval(c);
}


void
mui_control_lock(
		mui_control_t *c)
{
	if (!c)
		return;
	if (!c->lock.control) {
		mui_control_ref(&c->lock, c, FCC('l', 'o', 'c', 'k'));
		c->lock.ref.count = 10; // prevent it from being deleted
	} else {
		c->lock.ref.count += 10;
	}
}

mui_control_t *
mui_control_unlock(
		mui_control_t *c)
{
	if (!c)
		return NULL;
	if (c->lock.control) {
		if (c->lock.ref.trace)
			printf("%s: control %s was locked\n",
				__func__, c->title);
		if (c->lock.ref.count > 10) {
			c->lock.ref.count -= 10;
		} else {	// control was disposed of
			int delete = c->lock.ref.count < 10;
			// we are the last one, remove the lock
			if (c->lock.ref.trace)
				printf("%s: control %s unlocked delete %d\n",
					__func__, c->title, delete);
			mui_control_deref(&c->lock);
			if (delete) {
				mui_control_dispose(c);
				c = NULL;
			}
		}
	} else {
	//	if (c->lock.ref.trace)
			printf("%s: control %s was not locked\n", __func__, c->title);
	}
	return c;
}

static void
mui_control_dispose_actions(
		mui_control_t * 	c)
{
	if (!c)
		return;
	mui_action_t *a;
	while ((a = STAILQ_FIRST(&c->actions)) != NULL) {
		STAILQ_REMOVE_HEAD(&c->actions, self);
		free(a);
	}
}

void
mui_control_action(
		mui_control_t * c,
		uint32_t 		what,
		void * 			param )
{
	if (!c)
		return;
	// this prevents the callbacks from disposing of the control
	// the control is locked until the last callback is done
	// then it's disposed of
	mui_control_lock(c);
	mui_action_t *a, *safe;
	STAILQ_FOREACH_SAFE(a, &c->actions, self, safe) {
		if (!a->control_cb)
			continue;
		a->control_cb(c, a->cb_param, what, param);
	}
	mui_control_unlock(c);
}

void
mui_control_set_action(
		mui_control_t * c,
		mui_control_action_p 	cb,
		void * 			param )
{
	if (!c)
		return;
	mui_action_t *a = calloc(1, sizeof(*a));
	a->control_cb = cb;
	a->cb_param = param;
	STAILQ_INSERT_TAIL(&c->actions, a, self);
}

mui_control_t *
mui_control_get_by_id(
		mui_window_t * 	win,
		uint32_t 		uid )
{
	if (!win)
		return NULL;
	mui_control_t *c;
	TAILQ_FOREACH(c, &win->controls, self) {
		if (c->uid == uid)
			return c;
	}
	return NULL;
}

bool
mui_control_set_focus(
		mui_control_t * c )
{
	mui_window_t * 	win = c->win;
	if (!win || !c)
		return false;
	if (!c->cdef || !c->cdef(c, MUI_CDEF_CAN_FOCUS, NULL))
		return false;
	if (win->control_focus.control == c)
		return true;
	if (win->control_focus.control) {
		win->control_focus.control->cdef(
				win->control_focus.control, MUI_CDEF_FOCUS, &(int){0});
		mui_control_inval(win->control_focus.control);
		mui_control_deref(&win->control_focus);
	}
	mui_control_inval(c);
	c->cdef(c, MUI_CDEF_FOCUS, &(int){1});
	mui_control_ref(&c->win->control_focus, c, FCC('T','e','a','c'));
	return true;
}

bool
mui_control_has_focus(
		mui_control_t * c )
{
	if (!c)
		return false;
	return c->win->control_focus.control == c;
}

mui_control_t *
mui_control_switch_focus(
		mui_window_t * win,
		int 			dir )
{
	if (!win)
		return NULL;
	mui_control_t *c = win->control_focus.control;
	if (!c)
		c = TAILQ_FIRST(&win->controls);
	if (!c)
		return c;
	mui_control_t * start = c;
	do {
		c = dir > 0 ? TAILQ_NEXT(c, self) : TAILQ_PREV(c, controls, self);
		if (!c)
			c = dir > 0 ? TAILQ_FIRST(&win->controls) :
							TAILQ_LAST(&win->controls, controls);
		if (c->cdef && c->cdef(c, MUI_CDEF_CAN_FOCUS, NULL))
			break;
	} while (c != start);
	mui_control_set_focus(c);
	printf("focus %4.4s %s\n", (char*)&c->type, c->title);
	return c;
}
