/*
 * mui_menus.c
 *
 * Copyright (C) 2023 Michel Pollet <buserror@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 */

#include <stdio.h>
#include <stdlib.h>

#include "mui.h"
#include "mui_priv.h"
#include "cg.h"

#ifndef D
#define D(_x) //_x
#endif

enum {
	MUI_CONTROL_MENUTITLE			= FCC('m','t','i','t'),
	MUI_CONTROL_MENUITEM			= FCC('m','i','t','m'),
	MUI_CONTROL_SUBMENUITEM			= FCC('s','m','i','t'),
	MUI_CONTROL_POPUP				= FCC('p','o','p','u'),
	MUI_CONTROL_POPUP_MARK			= FCC('p','o','p','m'),
};

/* These are *window action* -- parameter 'target' is a mui_menu_t* */
enum mui_menu_action_e {
	MUI_MENU_ACTION_NONE 			= 0,
	MUI_MENU_ACTION_OPEN			= FCC('m','e','n','o'),
	MUI_MENU_ACTION_CLOSE			= FCC('m','e','n','c'),
	MUI_MENU_ACTION_SELECT			= FCC('m','e','n','s'),
	// BEFORE a menu/submenu get opened/created
	MUI_MENU_ACTION_PREPARE 		= FCC('m','e','p','r'),
};

struct mui_menu_control_t;
struct mui_menubar_t;


typedef struct mui_menu_t {
	mui_window_t 			win;
	uint					click_inside : 1,
							drag_ev : 1,
							timer_call_count : 2; 	// used by mui_menu_close_timer_cb
	mui_control_ref_t		highlighted;	// mui_menuitem_control_t *
	mui_time_t				sub_open_stamp;
	// currently open menu, if any
	mui_control_ref_t 		sub;		// mui_menu_control_t *
	mui_window_ref_t		menubar;	// mui_menubar_t * window
} mui_menu_t;

typedef struct mui_menubar_t {
	mui_window_t 			win;
	uint					click_inside : 1,
							drag_ev : 1,
							was_highlighted : 1,
							timer_call_count : 2; 	// used by mui_menu_close_timer_cb

	// currently open menu title
	mui_control_ref_t		selected_title;		// mui_menu_control_t *
	// keep track of the menus, and their own submenus as they are being opened
	// this is to keep track of the 'hierarchy' of menus, so that we can close
	// them all when the user clicks outside of them, or release the mouse.
	mui_window_ref_t 		open[8];
	uint 					open_count;
	bool 					delayed_closing;
} mui_menubar_t;

/* These are *control action* -- parameter is a menu title or popup,
 * parameter is a mui_menu_control_t*
 */
enum mui_menu_control_action_e {
	MUI_MENU_CONTROL_ACTION_NONE 		= 0,
	// called *before* the window is opened, you can change the items state/list
	MUI_MENU_CONTROL_ACTION_PREPARE 	= FCC('m','c','p','r'),
	// when an item is selected.
	MUI_MENU_CONTROL_ACTION_SELECT		= FCC('m','c','s','l'),
};


static mui_window_t *
_mui_menu_create(
		mui_t *					ui,
		mui_menubar_t * 		mbar,
		c2_pt_t 				origin,
		mui_menu_item_t* 		items );
static void
mui_menu_close(
		mui_window_t * win );
static bool
mui_wdef_menu(
		struct mui_window_t * 	win,
		uint8_t 				what,
		void * 					param);
static bool
mui_wdef_menubar(
		struct mui_window_t * 	win,
		uint8_t 				what,
		void * 					param);
static bool
mui_cdef_popup(
		struct mui_control_t * 	c,
		uint8_t 				what,
		void * 					param);

#define MUI_MENU_CLOSE_BLINK_DELAY 	(MUI_TIME_SECOND / 20)


// The menu bar title is a control, reset it's state to normal, and close
// any open menus (and their submenus, if any)
static bool
_mui_menubar_close_menu(
		mui_menubar_t *mbar )
{
	if (mbar->delayed_closing)
		return false;
	mbar->click_inside = false;
	mui_menu_control_t * m = (mui_menu_control_t*)mbar->selected_title.control;
	D(printf("%s %s\n", __func__, m ? ((mui_control_t*)m)->title : "???");)
	mui_control_set_state((mui_control_t*)m, 0);
	if (m)
		mui_window_deref(&m->menu_window);
	if (!mbar->open_count)
		return false;
	mui_control_deref(&mbar->selected_title);
	for (uint i = 0; i < mbar->open_count; i++) {
		mui_menu_close(mbar->open[i].window);
		mui_window_deref(&mbar->open[i]);
	}
	mbar->open_count = 0;
	return true;
}

// close the submenu from a hierarchical menu item
static bool
_mui_menu_close_submenu(
		mui_menu_t * menu )
{
	mui_menu_control_t * sub = (mui_menu_control_t*)menu->sub.control;
	if (!sub)
		return false;
	mui_control_deref(&menu->sub);
	mui_control_set_state((mui_control_t*)sub, 0);
	if (sub->menu_window.window) {
		mui_menu_close(sub->menu_window.window);
	}
	mui_window_deref(&sub->menu_window);

	return true;
}

/*
 * This does the blinking of the selected item, and closes the menu
 */
static mui_time_t
mui_menu_close_timer_cb(
		struct mui_t * mui,
		mui_time_t now,
		void * param)
{
	mui_menu_t * menu = param;
	if (!menu->highlighted.control) {
		D(printf("%s: no selected item, closing\n", __func__);)
		mui_window_dispose(&menu->win);
		return 0;
	}
	menu->timer_call_count++;
	mui_control_set_state(menu->highlighted.control,
			menu->highlighted.control->state == MUI_CONTROL_STATE_CLICKED ?
					MUI_CONTROL_STATE_NORMAL : MUI_CONTROL_STATE_CLICKED);
	if (menu->timer_call_count == 3) {
		// we are done!
		mui_menuitem_control_t * item =
				(mui_menuitem_control_t*)menu->highlighted.control;
		mui_window_action(&menu->win, MUI_MENU_ACTION_SELECT,  &item->item);
	//	mui_menu_close_all(&menu->win);
		if (menu->menubar.window) {
			mui_menubar_t * mbar = (mui_menubar_t*)menu->menubar.window;
			mui_window_action(&mbar->win,
						MUI_MENUBAR_ACTION_SELECT, &item->item);
			mbar->delayed_closing = false;
			_mui_menubar_close_menu(mbar);
		} else
			mui_menu_close(&menu->win);
		return 0;
	}
	return MUI_MENU_CLOSE_BLINK_DELAY;
}
/*
 * This restore the menubar to normal after a keystroke/menu equivalent was hit
 */
static mui_time_t
mui_menubar_unhighlight_cb(
		struct mui_t * mui,
		mui_time_t now,
		void * param)
{
	mui_menubar_t * mbar = param;
	mui_menubar_highlight(&mbar->win, false);
	return 0;
}

static int
mui_menu_action_cb(
		mui_window_t *	win,
		void * 			cb_param,
		uint32_t 		what,
		void * 			param)	// menu item here
{
	mui_menubar_t * mbar = cb_param;
	D(printf("%s %s %4.4s\n", __func__, mbar->win.title, (char*)&what);)
	switch (what) {
		/*
		 * If a submenu is created, add ourselves as a callback to it as well,
		 * so we get notified when it is closed or an item is selected
		 */
		case MUI_MENU_ACTION_OPEN: {
			mui_menu_t * submenu = param;
			mui_window_set_action(&submenu->win, mui_menu_action_cb, mbar);
		}	break;
		case MUI_MENU_ACTION_SELECT: {
			mui_menu_item_t * item = param;
			D(printf("	%s calling menubar action: %s\n", __func__, item->title);)
			// this is typically application code!
			mui_window_action(&mbar->win, MUI_MENUBAR_ACTION_SELECT, item);
		}	break;
		case MUI_WINDOW_ACTION_CLOSE: {
			D(mui_menu_t * menu = (mui_menu_t*)win;)
			D(printf("%s closing %s\n", __func__, menu->win.title);)
//			mui_menu_close_all(&mbar->win);
		}	break;
	}
	return 0;
}

static int
mui_submenu_action_cb(
		mui_window_t *	win,
		void * 			cb_param,
		uint32_t 		what,
		void * 			param)	// menu item here
{
	mui_menu_t * menu = cb_param;
	D(printf("%s %s %4.4s\n", __func__, menu->win.title, (char*)&what);)
	switch (what) {
		case MUI_MENU_ACTION_SELECT: {
			mui_menu_item_t * item = param;
			D(printf("	%s calling menubar action: %s\n", __func__, item->title);)
			// this is typically application code!
			mui_window_action(&menu->win, MUI_MENUBAR_ACTION_SELECT, item);
		}	break;
	}
	return 0;
}


static bool
mui_menubar_handle_mouse(
		mui_menubar_t * mbar,
		mui_event_t * ev )
{
	mui_window_t * win = &mbar->win;

	bool inside = c2_rect_contains_pt(&win->frame, &ev->mouse.where);
	mui_control_t * c = inside ?
							mui_control_locate(win, ev->mouse.where) : NULL;
	switch (ev->type) {
		case MUI_EVENT_BUTTONUP: {
			D(printf("%s up drag %d click in:%d high:%d was:%d\n", __func__,
						mbar->drag_ev, mbar->click_inside,
						mbar->selected_title.control ? 1 : 0,
						mbar->was_highlighted);)
			if (mbar->drag_ev == 0 && mbar->click_inside) {
				if (mbar->was_highlighted) {
					return _mui_menubar_close_menu(mbar);
				} else
					mbar->click_inside = false;
				return true;
			} else if (mbar->drag_ev == 0 && !mbar->click_inside) {
				return false;
			}
			return _mui_menubar_close_menu(mbar);
		}	break;
		case MUI_EVENT_DRAG:
			if (!mbar->click_inside)
				break;
			mbar->drag_ev = 1;
			// fallthrough
		case MUI_EVENT_BUTTONDOWN: {
		//	printf("%s button %d\n", __func__, ev->mouse.button);
			if (ev->mouse.button > 1)
				break;
			// save click, for detecting click+drag vs sticky menus
			if (ev->type == MUI_EVENT_BUTTONDOWN) {
				D(printf("%s click inside %d\n", __func__, inside);)
				mbar->drag_ev = 0;
				mbar->click_inside = inside;
				mbar->was_highlighted = mbar->selected_title.control != NULL;
			}
			if (c && mui_control_get_state(c) != MUI_CONTROL_STATE_DISABLED) {
				if (mbar->selected_title.control &&
							c != mbar->selected_title.control) {
					_mui_menubar_close_menu(mbar);
				}
				mbar->click_inside = true;
				mui_control_set_state(c, MUI_CONTROL_STATE_CLICKED);
				mui_menu_control_t *title = (mui_menu_control_t*)c;
				mui_control_deref(&mbar->selected_title);
				mui_control_ref(&mbar->selected_title, c, FCC('s','e','l','t'));
				if (mui_control_get_type(c) == MUI_CONTROL_MENUTITLE) {
					if (title->menu_window.window == NULL) {
						mui_window_t *new = _mui_menu_create(
								win->ui, mbar, C2_PT(c->frame.l, c->frame.b),
								title->menu.e);
						mui_window_ref(&title->menu_window, new,
								FCC('m','e','n','u'));
					}
				}
				return true;
			}
		}	break;
		default:
			break;
	}
	return false;
}

/*
 * Look into a menu item list for a key_equ match -- also handle
 * recursive submenus, return true the match itself, if found
 */
static mui_menu_item_t *
mui_menu_handle_keydown(
	mui_menu_item_t * items,
	mui_event_t *ev )
{
//	D(printf("%s\n", __func__);)
	for (int ii = 0; items[ii].title; ii++) {
		mui_menu_item_t * item = &items[ii];
		if (item->submenu) {
			mui_menu_item_t * sub = mui_menu_handle_keydown(item->submenu, ev);
			if (sub)
				return sub;
			continue;
		}
		if (!item->key_equ.value || item->disabled)
			continue;
		if (mui_event_match_key(ev, item->key_equ)) {
		//	printf("%s KEY MATCH %s\n", __func__, item->title);
			return item;
		}
	}
	return NULL;
}

static bool
mui_menubar_handle_keydown(
	mui_menubar_t * mbar,
	mui_event_t *ev )
{
	// iterate all the menus, check any with a key_equ that is
	// non-zero, if would, highlight that menu (temporarily)
	// and call the action function witht that menu item. score!
	mui_window_t * win = &mbar->win;
	for (mui_control_t * c = TAILQ_FIRST(&win->controls);
					c; c = TAILQ_NEXT(c, self)) {
		if (c->type != MUI_CONTROL_MENUTITLE)
			continue;
		mui_menu_control_t * title = (mui_menu_control_t*)c;
	//	printf("%s title %s\n", __func__, c->title);
		mui_menu_item_t * item = mui_menu_handle_keydown(
										title->menu.e, ev);
		if (item) {
		//	printf("%s KEY MATCH %s\n", __func__, item->title);
			mui_control_set_state(c, MUI_CONTROL_STATE_CLICKED);
			mui_window_action(&mbar->win,
						MUI_MENUBAR_ACTION_SELECT, item);
			mui_timer_register(win->ui, mui_menubar_unhighlight_cb, mbar,
					MUI_MENU_CLOSE_BLINK_DELAY);
			return true;
		}
	}
	return false;
}

static bool
mui_wdef_menubar(
		struct mui_window_t * 	win,
		uint8_t 				what,
		void * 					param)
{
	mui_menubar_t * mbar = (mui_menubar_t*)win;
	switch (what) {
		case MUI_WDEF_DISPOSE: {
			mui_control_deref(&mbar->selected_title);
			for (uint i = 0; i < mbar->open_count; i++) {
				mui_menu_close(mbar->open[i].window);
				mui_window_deref(&mbar->open[i]);
			}
		}	break;
		case MUI_WDEF_DRAW: {
			mui_drawable_t * dr = param;
			mui_wdef_menubar_draw(win, dr);
		}	break;
		case MUI_WDEF_EVENT: {
			mui_event_t *ev = param;
			switch (ev->type) {
				case MUI_EVENT_BUTTONUP:
				case MUI_EVENT_DRAG:
				case MUI_EVENT_BUTTONDOWN: {
					if (mui_menubar_handle_mouse(mbar, ev)) {
					//	printf("%s handled\n", __func__);
						return true;
					}
				}	break;
				case MUI_EVENT_KEYDOWN: {
				//	printf("%s keydown %c\n", __func__, ev->key.key);
					if (mui_menubar_handle_keydown(mbar, ev))
						return true;
				}	break;
				default:
					break;
			}
		}	break;
	}
	return false;
}

static bool
mui_menu_handle_mouse(
		mui_menu_t * menu,
		mui_event_t * ev )
{
	mui_window_t * win = &menu->win;

//	bool inside = c2_rect_contains_pt(&win->frame, &ev->mouse.where);
	bool is_front = mui_window_isfront(win);
	mui_control_t * c = mui_control_locate(win, ev->mouse.where);
	switch (ev->type) {
		case MUI_EVENT_BUTTONUP: {
			D(printf("%s (%s) up drag %d\n", __func__, win->title, menu->drag_ev);)
			if (menu->drag_ev == 0) {
		//		return true;
			}
			mui_menubar_t * mbar = (mui_menubar_t*)menu->menubar.window;
			if (menu->highlighted.control &&
					menu->highlighted.control->type != MUI_CONTROL_SUBMENUITEM) {
				/*
				 * This tells the normal closing code that we are
				 * taking care of the closing with the timer, and not *now*
				 */
				if (mbar)
					mbar->delayed_closing = true;
				menu->timer_call_count = 0;
				mui_timer_register(win->ui, mui_menu_close_timer_cb, menu,
						MUI_MENU_CLOSE_BLINK_DELAY);
			} else {
				mui_control_deref(&menu->highlighted);
				if (mbar)
					_mui_menubar_close_menu(mbar);
				else
					mui_menu_close(win);
			}
		}	break;
		case MUI_EVENT_DRAG:
			/*
			 * if we've just opened a submenu, make it 'sticky' for a while
			 * so that the user can drag the mouse to it without it closing
			 */
			if (!is_front &&
					(mui_get_time() - menu->sub_open_stamp) < (MUI_TIME_SECOND / 2))
				return false;
			menu->drag_ev = 1;
			// fallthrough
		case MUI_EVENT_BUTTONDOWN: {
			// save click, for detecting click+drag vs sticky menus
			if (ev->type == MUI_EVENT_BUTTONDOWN) {
				menu->drag_ev = 0;
			}
		//	printf("%s in:%d c:%s\n", __func__, inside, c ? c->title : "");
			if (c && mui_control_get_state(c) != MUI_CONTROL_STATE_DISABLED) {
				if (menu->sub.control && c != menu->sub.control)
					_mui_menu_close_submenu(menu);
				if (menu->highlighted.control &&
							c != menu->highlighted.control)
					mui_control_set_state(menu->highlighted.control, 0);
				mui_control_set_state(c, MUI_CONTROL_STATE_CLICKED);
				mui_control_deref(&menu->highlighted);
				mui_control_ref(&menu->highlighted, c, FCC('h','i','g','h'));
				if (c->type == MUI_CONTROL_SUBMENUITEM) {
					mui_menu_control_t *title = (mui_menu_control_t*)c;
					if (title->menu_window.window == NULL) {
						c2_pt_t where = C2_PT(c->frame.r, c->frame.t);
						c2_pt_offset(&where, win->content.l, win->content.t);
						mui_window_t *new = _mui_menu_create(
								win->ui,
								(mui_menubar_t*)menu->menubar.window, where,
								title->menu.e);
						mui_window_ref(&title->menu_window, new,
								FCC('m','e','n','u'));
						mui_control_ref(&menu->sub, c,
								FCC('s','u','b','m'));
						menu->sub_open_stamp = mui_get_time();
						mui_window_action(&menu->win, MUI_MENU_ACTION_OPEN,
												title->menu_window.window);
						mui_window_set_action(title->menu_window.window,
												mui_submenu_action_cb, menu);
					}
				}
			} else {
				if (!menu->sub.control) {
					if (menu->highlighted.control)
						mui_control_set_state(menu->highlighted.control, 0);
					mui_control_deref(&menu->highlighted);
				}
			}
		}	break;
		default:
			break;
	}
	return false;
}

static bool
mui_wdef_menu(
		struct mui_window_t * 	win,
		uint8_t 				what,
		void * 					param)
{
	mui_menu_t * menu = (mui_menu_t*)win;
	switch (what) {
		case MUI_WDEF_DISPOSE: {
			mui_window_deref(&menu->menubar);
			_mui_menu_close_submenu(menu);
		}	break;
		case MUI_WDEF_DRAW: {
			mui_drawable_t * dr = param;
			// shared drawing code for the menubar and menu
			mui_wdef_menubar_draw(win, dr);
		}	break;
		case MUI_WDEF_EVENT: {
			mui_event_t *ev = param;
			switch (ev->type) {
				case MUI_EVENT_BUTTONUP:
				case MUI_EVENT_DRAG:
				case MUI_EVENT_BUTTONDOWN: {
					if (mui_menu_handle_mouse(menu, ev))
						return true;
				}	break;
				default:
					break;
			}
		}	break;
	}
	return false;
}


mui_window_t *
mui_menubar_new(
		mui_t * ui )
{
	mui_font_t * main = mui_font_find(ui, "main");
	c2_rect_t mbf = C2_RECT_WH(0, 0, ui->screen_size.x, main->size + 4);

//	printf("%s frame %s\n", __func__, c2_rect_as_str(&mbf));
	mui_menubar_t * mbar = (mui_menubar_t*)mui_window_create(
								ui, mbf,
								mui_wdef_menubar, MUI_WINDOW_MENUBAR_LAYER,
								"Menubar", sizeof(*mbar));
	mbar->win.flags.style = MUI_MENU_STYLE_MBAR;
	mui_window_ref(&ui->menubar, &mbar->win, FCC('m','b','a','r'));
	return &mbar->win;
}

mui_window_t *
mui_menubar_get(
		mui_t * ui )
{
	return ui->menubar.window;
}

bool
mui_menubar_window(
		mui_window_t * win)
{
	return win && win->wdef == mui_wdef_menubar;
}

mui_control_t *
mui_menubar_add_simple(
		mui_window_t *win,
		const char * title,
		uint32_t menu_uid,
		mui_menu_item_t * items )
{
	mui_font_t * main = mui_font_find(win->ui, "main");
	stb_ttc_measure m = {};
	mui_font_text_measure(main, title, &m);

	int title_width = m.x1 - m.x0 + (main->size / 2);
	c2_rect_t title_rect = { .t = 2 };

	mui_control_t * last = TAILQ_LAST(&win->controls, controls);
	if (last) {
		c2_rect_offset(&title_rect, last->frame.r, 0);
	} else
		title_rect.l = 4;
	title_rect.r = title_rect.l + title_width + 6;
	title_rect.b = win->content.b + 2;// title_rect.t + m.ascent - m.descent;

	mui_control_t * c = mui_control_new(
				win, MUI_CONTROL_MENUTITLE, mui_cdef_popup,
				title_rect, title, menu_uid,
				sizeof(mui_menu_control_t));
	//printf("%s title %s rect %s\n", __func__, title, c2_rect_as_str(&title_rect));

	mui_menu_control_t *menu = (mui_menu_control_t*)c;
	mui_window_ref(&menu->menubar, win, FCC('m','b','a','r'));
	/*
	 * We do not clone the items, so they must be static
	 * from somewhere -- we do not free them either.
	 */
	int sub_count = 0;
	for (int ii = 0; items[ii].title; ii++)
		sub_count++;
	menu->menu.count = sub_count;
	menu->menu.e = items;
	menu->menu.read_only = 1;

	return c;
}

/* 'count' can be zero, in which case *requires* NULL termination */
mui_control_t *
mui_menubar_add_menu(
		mui_window_t *		win,
		uint32_t 			menu_uid,
		mui_menu_item_t * 	items,
		uint 				count  )
{
	c2_rect_t parts[MUI_MENUTITLE_PART_COUNT];
	mui_menutitle_get_part_locations(win->ui, NULL, items, parts);

	int title_width = c2_rect_width(&parts[MUI_MENUTITLE_PART_ALL]);
	c2_rect_t title_rect = { .t = 2 };

	mui_control_t * last = TAILQ_LAST(&win->controls, controls);
	if (last) {
		c2_rect_offset(&title_rect, last->frame.r, 0);
	} else
		title_rect.l = 4;
	title_rect.r = title_rect.l + title_width + 6;
	title_rect.b = win->content.b + 0;// title_rect.t + m.ascent - m.descent;

	mui_control_t * c = mui_control_new(
				win, MUI_CONTROL_MENUTITLE, mui_cdef_popup,
				title_rect, items[0].title, menu_uid,
				sizeof(mui_menu_control_t));
	//printf("%s title %s rect %s\n", __func__, title, c2_rect_as_str(&title_rect));

	mui_menu_control_t *menu = (mui_menu_control_t*)c;
	mui_window_ref(&menu->menubar, win, FCC('m','b','a','r'));
	menu->item.item = items[0];
	/*
	 * We do not clone the items, so they must be static
	 * from somewhere -- we do not free them either.
	 */
	int sub_count = count ? count - 1 : 0;

	for (int ii = 1; items[ii].title; ii++)
		sub_count++;
	menu->menu.count = sub_count ;
	menu->menu.e = items + 1;
	menu->menu.read_only = 1;

	return c;
}

#if 0
void
mui_menu_set_title(
		mui_control_t * c,
		const mui_menu_item_t *title )
{
	mui_menu_control_t * menu = (mui_menu_control_t*)c;
	mui_control_set_title(c, title);
	mui_window_set_title(&menu->menu_window, title);
}
#endif
mui_window_t *
mui_menubar_highlight(
		mui_window_t * win,
		bool ignored )
{
//	mui_menubar_t * mbar = (mui_menubar_t*)win;
	mui_control_t * c = NULL;
	TAILQ_FOREACH(c, &win->controls, self) {
		if (c->type == MUI_CONTROL_MENUTITLE ||
						mui_control_get_state(c)) {
			mui_control_set_state(c, 0);
		}
	}
	return win;
}


static c2_rect_t
mui_menu_get_enclosing_rect(
	mui_t * ui,
	mui_menu_item_t * items)
{
	c2_rect_t frame = {};
	if (!items)
		return frame;
	frame.b = 1;	// space for outside frame
	mui_font_t * main = mui_font_find(ui, "main");
	stb_ttc_measure m = {};
	for (int i = 0; items[i].title; i++) {
		items[i].location = frame.b;
		if (items[i].title && items[i].title[0] != '-') {
			mui_font_text_measure(main, items[i].title, &m);
			m.x0 = 0;
			int title_width = main->size + m.x1 - m.x0 ;

			if (items[i].kcombo[0]) {
				mui_font_text_measure(main, items[i].kcombo, &m);
				title_width += m.x1 - m.x0 + main->size;
			} else
				title_width += main->size;

			if (title_width > frame.r)
				frame.r = title_width;
			items[i].height = main->size + 4;
		} else {
			items[i].height = main->size / 4;
		}
		frame.b += items[i].height;
	}
	frame.b += 1;	// space for outside frame
	return frame;
}

/*
 * Given a list of items, create the actual windows & controls to show
 * it onscreen
 */
static mui_window_t *
_mui_menu_create(
		mui_t *					ui,
		mui_menubar_t * 		mbar,
		c2_pt_t 				origin,
		mui_menu_item_t * 		items )
{
	if (mbar)
		mui_window_action(&mbar->win,
							MUI_MENUBAR_ACTION_PREPARE,
							items);

	c2_rect_t frame = mui_menu_get_enclosing_rect(ui, items);
	c2_rect_t on_screen = frame;
	c2_rect_offset(&on_screen, origin.x, origin.y);

	/* Make sure the whole popup is on screen */
	/* TODO: handle very large popups, that'll be considerably more
	 * complicated to do, with the arrows, scrolling and stuff */
	c2_rect_t screen = C2_RECT_WH(0, 0, ui->screen_size.x, ui->screen_size.y);
	if (on_screen.r > screen.r)
		c2_rect_offset(&on_screen, screen.r - on_screen.r, 0);
	if (on_screen.b > screen.b)
		c2_rect_offset(&on_screen, 0, screen.b - on_screen.b);
	if (on_screen.t < screen.t)
		c2_rect_offset(&on_screen, 0, screen.t - on_screen.t);
	if (on_screen.l < screen.l)
		c2_rect_offset(&on_screen, screen.l - on_screen.l, 0);

	mui_menu_t * menu = (mui_menu_t*)mui_window_create(
								ui, on_screen,
								mui_wdef_menu, MUI_WINDOW_MENU_LAYER,
								items[0].title, sizeof(*menu));
	menu->win.flags.style = MUI_MENU_STYLE_MENU;
	if (mbar) {
		mui_window_ref(&mbar->open[mbar->open_count], &menu->win,
				FCC('m','e','n','u'));
		mbar->open_count++;
		mui_window_ref(&menu->menubar, &mbar->win, FCC('m','b','a','r'));
	}
	/* Walk all the items in out static structure, and create the controls
	 * for each of them with their own corresponding item */
	for (int i = 0; items[i].title; i++) {
		mui_menu_item_t * item = &items[i];
		item->index = i;
		c2_rect_t title_rect = frame;
		title_rect.t = item->location;
		title_rect.b = title_rect.t + item->height;

		mui_control_t * c = NULL;
		if (item->submenu) {
		//	printf("%s submenu %s\n", __func__, item->title);
			c = mui_control_new(
					&menu->win,
						MUI_CONTROL_SUBMENUITEM,
					mui_cdef_popup,
					title_rect, item->title, item->uid,
					sizeof(mui_menu_control_t));
			mui_menu_control_t *sub = (mui_menu_control_t*)c;
			/* Note: menuitems aren't copied, we use the same static structure */
			sub->menu.count = 0;
			for (int i = 0; item->submenu[i].title; i++)
				sub->menu.count++;
			sub->menu.e = item->submenu;
			sub->menu.read_only = 1;
		} else {
			c = mui_control_new(
					&menu->win,
						MUI_CONTROL_MENUITEM,
					mui_cdef_popup,
					title_rect, item->title, item->uid,
					sizeof(mui_menuitem_control_t));
		}
		if (item->disabled)
			mui_control_set_state(c, MUI_CONTROL_STATE_DISABLED);
		// both item and submenu share a menuitem control
		mui_menuitem_control_t *mic = (mui_menuitem_control_t*)c;
		mic->item = *item;
	}
	return &menu->win;
}

static void
mui_menu_close(
		mui_window_t * win )
{
	if (!win)
		return;
	mui_menu_t * menu = (mui_menu_t*)win;
	mui_menubar_t * mbar = (mui_menubar_t*)menu->menubar.window; // can be NULL

	mui_control_deref(&menu->highlighted);
	if (mbar && mbar->open_count) {
		mbar->open_count--;
		mui_window_deref(&mbar->open[mbar->open_count]);
	}
	mui_window_dispose(win);
}


static int
mui_popupmenu_action_cb(
		mui_window_t *	win,
		void * 			cb_param,
		uint32_t 		what,
		void * 			param)	// popup control here
{
	mui_menu_control_t * pop = cb_param;
	D(printf("%s %4.4s\n", __func__, (char*)&what);)
	switch (what) {
		case MUI_MENU_ACTION_SELECT: {
			mui_menu_item_t * item = param;
			pop->item.control.value = item->index;
			mui_control_inval(&pop->item.control);
			mui_control_action(&pop->item.control,
						MUI_CONTROL_ACTION_VALUE_CHANGED, NULL);
		}	break;
	}
	return 0;
}

static bool
mui_popupmenu_handle_mouse(
		mui_menu_control_t * pop,
		mui_event_t * ev )
{
	mui_control_t * c = &pop->item.control;
	switch (ev->type) {
		case MUI_EVENT_BUTTONUP: {
			D(printf("%s up has popup %d\n", __func__,
					pop->menu_window.window != NULL);)
			if (pop->menu_window.window) {
//				mui_menu_close(pop->menu_window);
				mui_window_deref(&pop->menu_window);
			}
			mui_control_set_state(c, 0);
		}	break;
		case MUI_EVENT_BUTTONDOWN: {
			mui_control_set_state(c,
					ev->type != MUI_EVENT_BUTTONUP ?
							MUI_CONTROL_STATE_CLICKED : 0);
			if (!pop->menu_window.window && pop->menu.e) {
				c2_pt_t loc = pop->menu_frame.tl;
				c2_pt_offset(&loc, c->win->content.l, c->win->content.t);
				if (c->type == MUI_CONTROL_POPUP)
					c2_pt_offset(&loc, 0, -pop->menu.e[c->value].location);
				else if (c->type == MUI_CONTROL_POPUP_MARK)
					c2_pt_offset(&loc, 0, c2_rect_height(&c->frame));
				mui_window_t *new = _mui_menu_create(
						c->win->ui, NULL, loc,
						pop->menu.e);
				// override the default style to make it specific to a popup
				new->flags.style = MUI_MENU_STYLE_POPUP;
				mui_window_ref(&pop->menu_window, new,
						FCC('m','e','n','u'));
				mui_window_set_action(pop->menu_window.window,
						mui_popupmenu_action_cb, pop);
				// pass the mousedown to the new popup
		//		mui_window_handle_mouse(pop->menu_window, ev);
			}
			mui_control_inval(c);
		}	break;
		default:
			break;
	}
	return true;
}

static bool
mui_cdef_popup(
		struct mui_control_t * 	c,
		uint8_t 				what,
		void * 					param)
{
	switch (what) {
		case MUI_CDEF_INIT: {
		}	break;
		case MUI_CDEF_DISPOSE:
			switch (c->type) {
				case MUI_CONTROL_POPUP:
				case MUI_CONTROL_POPUP_MARK:
				case MUI_CONTROL_MENUTITLE: {
					mui_menu_control_t *pop = (mui_menu_control_t*)c;
					if (pop->menu_window.window) {
						mui_menu_close(pop->menu_window.window);
						mui_window_deref(&pop->menu_window);
					}
					mui_menu_items_clear(&pop->menu);
					if (!pop->menu.read_only)
						mui_menu_items_free(&pop->menu);
					if (pop->item.color_icon)
						mui_drawable_dispose(pop->item.color_icon);
				}	break;
				case MUI_CONTROL_MENUITEM:
				case MUI_CONTROL_SUBMENUITEM: {
					mui_menuitem_control_t *mic = (mui_menuitem_control_t*)c;
					if (mic->color_icon)
						mui_drawable_dispose(mic->color_icon);
				}	break;
			}
			break;
		case MUI_CDEF_DRAW: {
			mui_drawable_t * dr = param;
			switch (c->type) {
				case MUI_CONTROL_POPUP:
					mui_popuptitle_draw(c->win, c, dr);
					break;
				case MUI_CONTROL_POPUP_MARK:
					mui_popupmark_draw(c->win, c, dr);
					break;
				case MUI_CONTROL_MENUTITLE:
					mui_menutitle_draw(c->win, c, dr);
					break;
				case MUI_CONTROL_MENUITEM:
				case MUI_CONTROL_SUBMENUITEM:
					mui_menuitem_draw(c->win, c, dr);
					break;
			}
		}	break;
		case MUI_CDEF_EVENT: {
			mui_event_t *ev = param;
		//	printf("%s event %d\n", __func__, ev->type);
			switch (ev->type) {
				case MUI_EVENT_BUTTONUP:
				case MUI_EVENT_DRAG:
				case MUI_EVENT_BUTTONDOWN: {
					switch (c->type) {
						case MUI_CONTROL_POPUP_MARK:
						case MUI_CONTROL_POPUP: {
							mui_menu_control_t *pop = (mui_menu_control_t*)c;
							return mui_popupmenu_handle_mouse(pop, ev);
						}	break;
						case MUI_CONTROL_MENUTITLE:
					//		mui_menutitle_draw(c->win, c, dr);
							break;
					}
				}	break;
				default:
					break;
			}
		}	break;
	}
	return false;
}

mui_control_t *
mui_popupmenu_new(
	mui_window_t *	win,
	c2_rect_t 		frame,
	const char * 	title,
	uint32_t 		uid,
	uint32_t		flags )
{
	int kind = MUI_CONTROL_POPUP;
	if (!title || !strcmp(title, MUI_GLYPH_POPMARK))
		kind = MUI_CONTROL_POPUP_MARK;
	mui_control_t *c =  mui_control_new(
				win, kind, mui_cdef_popup,
				frame, title, uid, sizeof(mui_menu_control_t));
	c->style = flags;
	return c;
}

mui_menu_items_t *
mui_popupmenu_get_items(
		mui_control_t * c)
{
	if (!c)
		return NULL;
	if (c->type != MUI_CONTROL_POPUP &&
				c->type != MUI_CONTROL_POPUP_MARK &&
				c->type != MUI_CONTROL_MENUTITLE ) {
		D(printf("%s: not a popup or menutitle\n", __func__);)
		return NULL;
	}
	mui_menu_control_t *pop = (mui_menu_control_t*)c;
	return &pop->menu;
}

void
mui_popupmenu_prepare(
		mui_control_t * c)
{
	mui_menu_control_t *pop = (mui_menu_control_t*)c;
	if (pop->menu_window.window) {
		mui_window_dispose(pop->menu_window.window);
		mui_window_deref(&pop->menu_window);
	}
	c2_rect_t frame = mui_menu_get_enclosing_rect(c->win->ui, pop->menu.e);
//	pop->menu_frame = frame;
	c2_rect_offset(&frame, c->frame.l, c->frame.t);
	switch (c->type) {
		case MUI_CONTROL_POPUP:
			frame.r +=  32; // add the popup symbol width
			break;
		case MUI_CONTROL_POPUP_MARK:
			// little tweak so the popup appears over the bottom border of button
			c2_rect_offset(&frame, 0, -2);
			break;
	}
	if (c->style & MUI_TEXT_ALIGN_CENTER) {
		if (c2_rect_width(&frame) < c2_rect_width(&c->frame)) {
			c2_rect_offset(&frame, (c2_rect_width(&c->frame) / 2) -
									(c2_rect_width(&frame) / 2), 0);
		}
	} else if (c->style & MUI_TEXT_ALIGN_RIGHT) {
		c2_rect_offset(&frame, c2_rect_width(&c->frame) - c2_rect_width(&frame), 0);
	}
	pop->menu_frame = frame;
	c->value = 0;
	mui_control_inval(c);
}
