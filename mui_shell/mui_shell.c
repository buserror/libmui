/*
 * mui_shell.c
 *
 * Copyright (C) 2023 Michel Pollet <buserror@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 */
#define MUI_HAS_XCB 1
#define MUI_HAS_XKB 1

#if MUI_HAS_XCB
#include <sys/stat.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <libgen.h>
#include <ctype.h>
#include <fcntl.h>
#include <dlfcn.h>

#include <sys/ipc.h>
#include <sys/shm.h>
#include <xcb/xcb.h>
#include <xcb/shm.h>
#include <xcb/xcb_image.h>
//#include <xcb/randr.h>

#if MUI_HAS_XKB
#include <xcb/xkb.h>
#include <xkbcommon/xkbcommon.h>
#include <xkbcommon/xkbcommon-x11.h>
#include <xkbcommon/xkbcommon-keysyms.h>
#else
struct xkb_state;
#endif

#include "mui.h"
#include "mui_shell_plugin.h"



typedef struct mui_xcb_t {
	mui_t 				ui;
	mui_plug_t * 		plug;
	void * 				plug_data;

	float 				ui_scale_x, ui_scale_y;
	c2_pt_t				size;
	xcb_connection_t *	xcb;
	xcb_image_t *		xcb_image;	// if no shared memory
	xcb_shm_segment_info_t shm;
	xcb_window_t 		window;
	xcb_pixmap_t 		xcb_pix;
	xcb_gcontext_t 		xcb_win_gc;
	struct xkb_state *	xkb_state;

	int 				redraw;
} mui_xcb_t;


int
_mui_xcb_init_keyboard(
		mui_xcb_t *ui)
{
#if MUI_HAS_XKB
	uint8_t xkb_event_base, xkb_error_base;
	if (!xkb_x11_setup_xkb_extension (ui->xcb,
					XKB_X11_MIN_MAJOR_XKB_VERSION, XKB_X11_MIN_MINOR_XKB_VERSION,
					XKB_X11_SETUP_XKB_EXTENSION_NO_FLAGS,
					NULL, NULL,
					&xkb_event_base, &xkb_error_base)) {
	    fprintf(stderr, "%s needs version %d.%d or newer\n", __func__,
	                XKB_X11_MIN_MAJOR_XKB_VERSION, XKB_X11_MIN_MINOR_XKB_VERSION);
		goto error;
	}
	struct xkb_context *ctx =
			xkb_context_new(XKB_CONTEXT_NO_FLAGS);
	int32_t device_id =
			xkb_x11_get_core_keyboard_device_id(ui->xcb);
	if (device_id == -1) {
		fprintf(stderr, "%s xkb_x11_get_core_keyboard_device_id\n", __func__);
		goto error;
	}
	struct xkb_keymap *keymap =
			xkb_x11_keymap_new_from_device(ctx, ui->xcb,
			device_id, XKB_KEYMAP_COMPILE_NO_FLAGS);
	ui->xkb_state =
			xkb_x11_state_new_from_device(keymap, ui->xcb, device_id);
	xkb_context_unref(ctx);
    return 0;
#endif
error:
	printf("XCB Keyboard initialisation: %s\n", ui->xkb_state ? "OK" : "Failed");
	ui->xkb_state = NULL;
	return -1;
}

/*
 * xmodmap -pke or -pk will print the list of keycodes
 */
static bool
_mui_xcb_convert_keycode(
		mui_xcb_t *ui,
		xkb_keysym_t sym,
		mui_event_t *out )
{
	switch (sym) {
		case XKB_KEY_F1 ... XKB_KEY_F12:
			out->key.key = MUI_KEY_F1 + (sym - XKB_KEY_F1);
			break;
		case XKB_KEY_Escape: out->key.key = MUI_KEY_ESCAPE; break;
		case XKB_KEY_Left: out->key.key = MUI_KEY_LEFT; break;
		case XKB_KEY_Up: out->key.key = MUI_KEY_UP; break;
		case XKB_KEY_Right: out->key.key = MUI_KEY_RIGHT; break;
		case XKB_KEY_Down: out->key.key = MUI_KEY_DOWN; break;
		// XKB_KEY_Begin
		case XKB_KEY_Insert: out->key.key = MUI_KEY_INSERT; break;
		case XKB_KEY_Delete: out->key.key = MUI_KEY_DELETE; break;
		case XKB_KEY_Home: out->key.key = MUI_KEY_HOME; break;
		case XKB_KEY_End: out->key.key = MUI_KEY_END; break;
		case XKB_KEY_Page_Up: out->key.key = MUI_KEY_PAGEUP; break;
		case XKB_KEY_Page_Down: out->key.key = MUI_KEY_PAGEDOWN; break;

		case XKB_KEY_Shift_R: out->key.key = MUI_KEY_RSHIFT; break;
		case XKB_KEY_Shift_L: out->key.key = MUI_KEY_LSHIFT; break;
		case XKB_KEY_Control_R: out->key.key = MUI_KEY_RCTRL; break;
		case XKB_KEY_Control_L: out->key.key = MUI_KEY_LCTRL; break;
		case XKB_KEY_Alt_L: out->key.key = MUI_KEY_LALT; break;
		case XKB_KEY_Alt_R: out->key.key = MUI_KEY_RALT; break;
		case XKB_KEY_Super_L: out->key.key = MUI_KEY_LSUPER; break;
		case XKB_KEY_Super_R: out->key.key = MUI_KEY_RSUPER; break;
		default:
			out->key.key = sym & 0xff;
			break;
	}
//	printf("%s %08x to %04x\n", __func__, sym, out->key.key);
	return true;
}

struct mui_t *
mui_xcb_init(
		struct mui_t *mui,
		struct mui_pixmap_t * pix )
{
	mui_xcb_t *ui = (mui_xcb_t *)mui;

	pix->size.y = 720;
	pix->size.x = 1280;
	ui->ui.screen_size = pix->size;
	ui->ui_scale_x = ui->ui_scale_y = 1;
	printf("XCB: Starting on %dx%d window\n",
			pix->size.x, pix->size.y);

	pix->size.x *= ui->ui_scale_x;
	pix->size.y *= ui->ui_scale_y;
	ui->size = pix->size;

	uint32_t value_mask;
	uint32_t value_list[6];

	ui->xcb = xcb_connect(NULL, NULL);

	xcb_screen_iterator_t iter = xcb_setup_roots_iterator(
											xcb_get_setup(ui->xcb));
	printf("%s %d screens\n", __func__, iter.rem);
	xcb_screen_t *screen = NULL;
	while (iter.rem) {
		screen = iter.data;
		printf("%s screen %d: width: %d, height: %d\n", __func__,
				iter.index,
				screen->width_in_pixels, screen->height_in_pixels);
		xcb_screen_next(&iter);
	}
	printf("XCB Screen depth %d\n", screen->root_depth);

	xcb_shm_query_version_reply_t *xcb_shm_present;
	xcb_shm_present = xcb_shm_query_version_reply(
					ui->xcb, xcb_shm_query_version(ui->xcb), NULL);
	bool slow_path = false;
	if (!xcb_shm_present || !xcb_shm_present->shared_pixmaps) {
		printf("xcb_shm error... %p\n", xcb_shm_present);
		printf("If using nvidia driver, you need\n"
				"    Option	   \"AllowSHMPixmaps\" \"1\"\n"
				"  In your /etc/X11/xorg.conf file\n");
		slow_path = true;
	//	exit(0);
	} else
		printf("XCB Shared memory present\n");

	_mui_xcb_init_keyboard(ui);

	value_mask = XCB_CW_BACK_PIXEL |
					XCB_CW_BORDER_PIXEL |
					XCB_CW_OVERRIDE_REDIRECT |
					XCB_CW_EVENT_MASK;

	xcb_colormap_t cmap =  xcb_generate_id(ui->xcb);

	uint32_t w_mask[] = {
			screen->black_pixel,
			// Border Pixel; not really needed for anything, but needed
			// for ARGB window otherwise it doesn't get created properly
			0x88888888,
			// if we found a screen of the exact size, remove the border
			0,
			XCB_EVENT_MASK_EXPOSURE | XCB_EVENT_MASK_BUTTON_PRESS |
				XCB_EVENT_MASK_BUTTON_RELEASE | XCB_EVENT_MASK_POINTER_MOTION |
				XCB_EVENT_MASK_ENTER_WINDOW | XCB_EVENT_MASK_LEAVE_WINDOW |
				XCB_EVENT_MASK_KEY_PRESS | XCB_EVENT_MASK_KEY_RELEASE,
			cmap
	};
	ui->window = xcb_generate_id(ui->xcb);
	xcb_create_window(
			ui->xcb,
			XCB_COPY_FROM_PARENT,
			ui->window, screen->root,
			0,0,
			pix->size.x, pix->size.y, 0,
			XCB_WINDOW_CLASS_INPUT_OUTPUT,
			screen->root_visual,
			value_mask, w_mask);
	xcb_free_colormap(ui->xcb, cmap);

	const char * title = "MII UI Playground";
    xcb_change_property(ui->xcb, XCB_PROP_MODE_REPLACE,
    		ui->window, XCB_ATOM_WM_NAME, XCB_ATOM_STRING, 8,
			strlen(title), title);
	// create a graphic context
	value_mask = XCB_GC_FOREGROUND | XCB_GC_GRAPHICS_EXPOSURES;
	value_list[0] = screen->white_pixel;
	value_list[1] = 0;
	ui->xcb_win_gc = xcb_generate_id(ui->xcb);
	xcb_create_gc(
			ui->xcb, ui->xcb_win_gc, ui->window, value_mask, value_list);
	// map the window onto the screen
	xcb_map_window(ui->xcb, ui->window);
	// wont show unless I do this
	xcb_flush(ui->xcb);

	if (!slow_path) {
		xcb_generic_error_t *error;
		xcb_void_cookie_t cook;
		ui->shm.shmid = shmget(IPC_PRIVATE,
								pix->size.x * pix->size.y * 4, IPC_CREAT | 0666);
		ui->shm.shmaddr = shmat(ui->shm.shmid, 0, 0);
		ui->shm.shmseg = xcb_generate_id(ui->xcb);
		cook = xcb_shm_attach_checked(ui->xcb, ui->shm.shmseg, ui->shm.shmid, 0);
		error = xcb_request_check(ui->xcb, cook);
		if (error) {
			fprintf(stderr, "XCB: Error xcb_shm_attach_checked: %d\n",
					error->error_code);
			free(error);
//			xcb_disconnect(ui->xcb);
		//	return 1;
		}
		shmctl(ui->shm.shmid, IPC_RMID, 0);

		ui->xcb_pix = xcb_generate_id(ui->xcb);
		cook = xcb_shm_create_pixmap_checked(
					ui->xcb, ui->xcb_pix, ui->window,
					pix->size.x, pix->size.y,
					screen->root_depth,
					ui->shm.shmseg, 0);
		error = xcb_request_check(ui->xcb, cook);
		if (error) {
			fprintf(stderr, "XCB: Error xcb_shm_create_pixmap: %d\n",
					error->error_code);
			free(error);
		} else
			pix->pixels = ui->shm.shmaddr;
		pix->row_bytes = pix->size.x * 4;
	}
	if (!pix->pixels) {
		printf("XCB: Not using SHM, slow path\n");
		pix->pixels = malloc(pix->size.x * pix->size.y * 4);

		ui->xcb_pix = xcb_generate_id(ui->xcb);
		xcb_create_pixmap(
				ui->xcb, screen->root_depth, ui->xcb_pix, ui->window,
				pix->size.x, pix->size.y);
		ui->xcb_image = xcb_image_create_native(
				ui->xcb, pix->size.x, pix->size.y, XCB_IMAGE_FORMAT_Z_PIXMAP,
				screen->root_depth, pix->pixels,
				pix->size.x * pix->size.y * 4, pix->pixels);
		pix->row_bytes = pix->size.x * 4;
	}
	xcb_flush(ui->xcb);
//	printf("%s pix is %p\n", __func__, pix->pixels);
	ui->redraw = 1;
	return &ui->ui;
}

static void
mui_read_clipboard(
		struct mui_t *mui)
{
	FILE *f = popen("xclip -selection clipboard -o", "r");
	if (!f)
		return;
	mui_utf8_t clip = {};
	char buf[1024];
	size_t r = 0;
	do {
		r = fread(buf, 1, sizeof(buf), f);
		if (r > 0)
			mui_utf8_append(&clip, (uint8_t*)buf, r);
	} while (r > 0);
	pclose(f);
	mui_utf8_free(&mui->clipboard);
	mui->clipboard = clip;
}

int
mui_xcb_poll(
		struct mui_t * mui,
		mui_drawable_t * dr,
		bool redrawn)
{
	mui_xcb_t * ui = (mui_xcb_t *)mui;
	int gameover = 0;
	xcb_generic_event_t *event;
	static bool buttondown = false;
	static mui_event_t key_ev;

	while (!gameover && (event = xcb_poll_for_event(ui->xcb)) != NULL) {
		switch (event->response_type & ~0x80) {
			case XCB_KEY_RELEASE: {
				xcb_key_release_event_t *key =
						(xcb_key_press_event_t*) event;
				xkb_state_update_key(ui->xkb_state,
						key->detail, XKB_KEY_UP);
				xkb_keysym_t keysym = xkb_state_key_get_one_sym(
						ui->xkb_state, key->detail);
				key_ev.type = MUI_EVENT_KEYUP;
				key_ev.key.up = 1;
				if (_mui_xcb_convert_keycode(ui, keysym, &key_ev)) {
					if (key_ev.key.key >= MUI_KEY_MODIFIERS &&
							key_ev.key.key <= MUI_KEY_MODIFIERS_LAST) {
						mui->modifier_keys &= ~(1 << (key_ev.key.key - MUI_KEY_MODIFIERS));
					}
					key_ev.modifiers = mui->modifier_keys;
			//		key_ev.modifiers |= MUI_MODIFIER_EVENT_TRACE;
					if (ui->plug && ui->plug->event)
						ui->plug->event(mui, ui->plug_data, &key_ev);
				}
			}	break;
			case XCB_KEY_PRESS: {
				xcb_key_press_event_t *key =
						(xcb_key_press_event_t*) event;
				if (key->same_screen == 0)	// repeat key
					break;
				xkb_state_update_key(ui->xkb_state,
						key->detail, XKB_KEY_DOWN);
				xkb_keysym_t keysym = xkb_state_key_get_one_sym(
						ui->xkb_state, key->detail);
				key_ev.type = MUI_EVENT_KEYDOWN;
				key_ev.key.up = 0;
			//	printf("%s %08x\n", __func__, keysym);
				if (_mui_xcb_convert_keycode(ui, keysym, &key_ev)) {
					if (key_ev.key.key >= MUI_KEY_MODIFIERS &&
							key_ev.key.key <= MUI_KEY_MODIFIERS_LAST) {
						mui->modifier_keys |= (1 << (key_ev.key.key - MUI_KEY_MODIFIERS));
					}
					if (toupper(key_ev.key.key) == 'V' &&
							(mui->modifier_keys & MUI_MODIFIER_CTRL)) {
						printf("Get CLIPBOARD\n");
						mui_read_clipboard(mui);
					}
					key_ev.modifiers = mui->modifier_keys;
			//		key_ev.modifiers |= MUI_MODIFIER_EVENT_TRACE;
				//	gameover = key_ev.key.key == 'q';
					if (ui->plug && ui->plug->event)
						ui->plug->event(mui, ui->plug_data, &key_ev);
				}
			}	break;
			case XCB_BUTTON_RELEASE:
				buttondown = false;
				// fall through
			case XCB_BUTTON_PRESS: {
				xcb_button_press_event_t *m =
						(xcb_button_press_event_t *)event;
#if 0
				printf("%s %s %02x %d at %4dx%4d\n", __func__,
						(event->response_type & ~0x80) == XCB_BUTTON_PRESS ?
								"down" : "up",
						event->response_type,
						m->detail, m->event_x, m->event_y);
#endif
				switch (m->detail) {
					case XCB_BUTTON_INDEX_1: {
					case XCB_BUTTON_INDEX_3:
						buttondown = (event->response_type & ~0x80) == XCB_BUTTON_PRESS;
						mui_event_t ev = {
							.type = buttondown ?
										MUI_EVENT_BUTTONDOWN :
										MUI_EVENT_BUTTONUP,
							.mouse.button = m->detail,
							.mouse.where.x = (float)m->event_x / ui->ui_scale_x,
							.mouse.where.y = (float)m->event_y / ui->ui_scale_y,
							.modifiers = mui->modifier_keys,
						};
						if (ui->plug && ui->plug->event)
							ui->plug->event(mui, ui->plug_data, &ev);
					}	break;
					case XCB_BUTTON_INDEX_4:
					case XCB_BUTTON_INDEX_5: {
						mui_event_t ev = {
							.type = MUI_EVENT_WHEEL,
							.wheel.delta = m->detail == XCB_BUTTON_INDEX_4 ?
											-1 : 1,
							.wheel.where.x = (float)m->event_x / ui->ui_scale_x,
							.wheel.where.y = (float)m->event_y / ui->ui_scale_y,
							.modifiers = mui->modifier_keys,
						};
						if (ui->plug && ui->plug->event)
							ui->plug->event(mui, ui->plug_data, &ev);
					}	break;
				}
			}	break;
			case XCB_MOTION_NOTIFY: {
				xcb_motion_notify_event_t *m =
						(xcb_motion_notify_event_t*)event;
//				if (buttondown) {
					// printf("x=%d y=%d\n", event.motion.x, event.motion.y);
					mui_event_t ev = {
						.type = MUI_EVENT_DRAG,
						.mouse.button = buttondown ? 1 : 0,
						.mouse.where.x = (float)m->event_x / ui->ui_scale_x,
						.mouse.where.y = (float)m->event_y / ui->ui_scale_y,
						.modifiers = mui->modifier_keys,
					};
					if (ui->plug && ui->plug->event)
						ui->plug->event(mui, ui->plug_data, &ev);
//				}
			}	break;
			case XCB_ENTER_NOTIFY:
			case XCB_LEAVE_NOTIFY:
				break;
			case XCB_EXPOSE: {
			//	xcb_expose_event_t *expose_event = (xcb_expose_event_t*) event;
//				ui->redraw++;
				xcb_expose_event_t *x = (xcb_expose_event_t*) event;
         		xcb_copy_area(
						ui->xcb, ui->xcb_pix,
						ui->window, ui->xcb_win_gc,
						x->x, x->y, x->x, x->y, x->width, x->height);
			}	break;
			default:
				// Handle other events
				break;
		}
		free(event);
	}
	if (redrawn || ui->redraw || pixman_region32_not_empty(&mui->redraw)) {
		// Handle window refresh event
		int rc = 0;
		c2_rect_t whole = C2_RECT(0, 0, ui->size.x, ui->size.y);
		c2_rect_t *ra = (c2_rect_t*)pixman_region32_rectangles(&mui->redraw, &rc);
		if (ui->redraw) {
			ui->redraw = 0;
			rc = 1;
			ra = &whole;
		}
		if (rc) {
	//		printf("XCB: %d rects to redraw\n", rc);
			for (int i = 0; i < rc; i++) {
				c2_rect_t r = ra[i];
			//	printf("XCB: %d,%d %dx%d\n", r.l, r.t, c2_rect_width(&r), c2_rect_height(&r));

				// cannot update the exact rectangle, just window-wide strips
				if (ui->xcb_image) {
					mui_pixmap_t * pix = &dr->pix;
					xcb_put_image(
							ui->xcb, XCB_IMAGE_FORMAT_Z_PIXMAP,
							ui->xcb_pix, ui->xcb_win_gc,
							pix->size.x, c2_rect_height(&r),
							0, r.t, 0, 24,
							c2_rect_height(&r) * pix->row_bytes,
							(uint8_t*)pix->pixels + (r.t * pix->row_bytes) /*+ r.l * 4*/);
				}
				xcb_copy_area(
						ui->xcb, ui->xcb_pix,
						ui->window, ui->xcb_win_gc,
						r.l, r.t, r.l, r.t,
						c2_rect_width(&r), c2_rect_height(&r));
			}
		}
		pixman_region32_clear(&mui->redraw);
	}
	xcb_flush(ui->xcb);
	return gameover;
}

void
mui_xcb_terminate(
		struct mui_t * mui)
{
	mui_xcb_t * ui = (mui_xcb_t *)mui;
    xcb_shm_detach(ui->xcb, ui->shm.shmseg);
    shmdt(ui->shm.shmaddr);
    xcb_free_pixmap(ui->xcb, ui->xcb_pix);
    xcb_destroy_window(ui->xcb, ui->window);
    xcb_disconnect(ui->xcb);
}

#endif

int
main(
		int argc,
		const char * argv[])
{
	const char * filename = NULL;// "build-x86_64-linux-gnu/lib/ui_tests.so";

	for (int i = 1; i < argc; i++) {
		if (argv[i][0] == '-') {
			switch (argv[i][1]) {
				case 'f':
					filename = argv[++i];
					break;
			}
		} else {
			filename = argv[i];
		}
	}
	if (!filename) {
		fprintf(stderr, "Usage: %s -f <filename>\n", basename((char*)argv[0]));
		return 1;
	}

	mui_xcb_t xcb_ui = {};
	mui_drawable_t dr = {};

	// note: the mui_t is *not* initialized yet, it will be done
	// in the _init() of the plugin. This is just to get the
	// init/dispose done in the plugin to check for leaks etc
	mui_t *mui = mui_xcb_init((struct mui_t *)&xcb_ui, &dr.pix);
	mui_xcb_t * ui = &xcb_ui;
	void * dynload = NULL;
	struct stat st_current = {}, st = {};

	mui_time_t stamp = 0;
	do {
		if (stat(filename, &st) == 0 && st.st_mtime != st_current.st_mtime) {
			st_current = st;
			if (dynload) {
				if (ui->plug_data && ui->plug && ui->plug->dispose) {
					ui->plug->dispose(ui->plug_data);
					ui->plug = NULL;
					ui->plug_data = NULL;
				}
				printf("Closed %s\n", filename);
				dlclose(dynload);
				dynload = NULL;
			}
		}
		if (!dynload) {
			dynload = dlopen(filename, RTLD_NOW);
			printf("Loading %s\n", filename);
			if (!dynload) {
				printf("Failed to load %s : %s\n", filename, dlerror());
				perror(filename);
				sleep(2);
				continue;
			}
			ui->plug = dlsym(dynload, "mui_plug");
			if (!ui->plug) {
				printf("Failed to find mui_plug in %s\n", filename);
				dlclose(dynload);
				dynload = NULL;
				sleep(10);
				continue;
			}
			if (ui->plug->init) {
				ui->plug_data = ui->plug->init(mui, ui->plug, &dr);
				if (!ui->plug_data) {
					printf("Failed to init plugin %s\n", filename);
					dlclose(dynload);
					dynload = NULL;
					sleep(10);
					continue;
				}
			}
			stamp = mui_get_time();
		}
		bool draw = false;
		mui_run(mui);
		if (ui->plug && ui->plug->draw)
			draw = ui->plug->draw(mui, ui->plug_data, &dr, false);
		if (mui_xcb_poll(mui, &dr, draw))
			break;
		mui_time_t now = mui_get_time();
		while (stamp < now)
			stamp += (MUI_TIME_SECOND / 60);
		usleep(stamp-now);
	} while (!mui->quit_request);
	if (dynload) {
		if (ui->plug_data && ui->plug && ui->plug->dispose) {
			ui->plug->dispose(ui->plug_data);
			ui->plug = NULL;
			ui->plug_data = NULL;
		}
		printf("Closed %s\n", filename);
		// no need to dlclose, it prevents valgrind --leak-check=yes to find
		// the symbols we want as they have been unloaded!
	//	dlclose(dynload);
	}
	mui_drawable_dispose(&dr);
	mui_xcb_terminate(mui);
	return 0;
}
