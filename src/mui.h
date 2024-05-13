/*
 * mui.h
 *
 * Copyright (C) 2023 Michel Pollet <buserror@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 */

/*
 * This is the main include file for the libmui UI library, it should be
 * the only one you need to include.
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <pixman.h>
#include "c2_arrays.h"

#ifdef __wasm__
typedef unsigned int uint;
#endif

#if 0 // only use to debug queue macros; do not enable
#define _KERNEL
#define INVARIANTS
#define	QUEUE_MACRO_DEBUG_TRACE
#define panic(...) \
	do { \
		fprintf(stderr, "PANIC: %s:%d\n", __func__, __LINE__); \
		fprintf(stderr, __VA_ARGS__); \
		*((int*)0) = 0xdead; \
	} while(0)
#endif

#include "bsd_queue.h"
#include "stb_ttc.h"

/* Four Character Constants are used everywhere. Wish this had become a standard,
 * as it is so handy -- but nope, thus the macro. Annoyingly, the little-
 * endianess of them makes it a pain to do a printf() with them, this is why
 * the values are reversed here.
 */
#include <ctype.h>
#define FCC(_a,_b,_c,_d) (((_d)<<24)|((_c)<<16)|((_b)<<8)|(_a))
/* These are made to allow FCC to have a numerical index, this is
 * mostly used for radio button, menu items and so on */
#define FCC_MASK		FCC(0xff,0xff,0xff,0)
/* number of bits to shift to get the fourth character of _fcc */
#define FCC_SHIFT(_fcc) ((_fcc)>>(ffs(~FCC_MASK)-1) & 0xff)
/* extract the index number of a fcc of type abcX where X is '0' - '9' */
#define FCC_INDEX(_fcc) (isdigit(FCC_SHIFT(_fcc)) ? \
							((FCC_SHIFT(_fcc)) - '0') : 0)
#define FCC_INDEXED(_fcc, _idx) \
			((_fcc & FCC_MASK) | ('0'+((_idx) & 0xff)) << (ffs(~FCC_MASK)-1))

typedef enum mui_event_e {
	MUI_EVENT_KEYUP = 0,
	MUI_EVENT_KEYDOWN,
	MUI_EVENT_BUTTONUP,
	MUI_EVENT_BUTTONDOWN,
	MUI_EVENT_BUTTONDBL,	// double click
	MUI_EVENT_WHEEL,
	MUI_EVENT_DRAG,
	// the following ones aren't supported yet
	MUI_EVENT_TEXT,			// UTF8 sequence [TODO]
	MUI_EVENT_MOUSEENTER,
	MUI_EVENT_MOUSELEAVE,
	MUI_EVENT_RESIZE,
	MUI_EVENT_CLOSE,

	MUI_EVENT_COUNT,
	// left, middle, right buttons for clicks
	MUI_EVENT_BUTTON_MAX = 3,
} mui_event_e;

typedef enum mui_key_e {
	// these are ASCII
	MUI_KEY_ESCAPE 	= 0x1b,
	MUI_KEY_SPACE 	= 0x20,
	MUI_KEY_RETURN 	= 0x0d,
	MUI_KEY_TAB 	= 0x09,
	MUI_KEY_BACKSPACE = 0x08,
	// these are not ASCII
	MUI_KEY_LEFT 	= 0x80,	MUI_KEY_UP,	MUI_KEY_RIGHT,	MUI_KEY_DOWN,
	MUI_KEY_INSERT,		MUI_KEY_DELETE,
	MUI_KEY_HOME,		MUI_KEY_END,
	MUI_KEY_PAGEUP,		MUI_KEY_PAGEDOWN,
	MUI_KEY_MODIFIERS = 0x90,
	MUI_KEY_LSHIFT 	= MUI_KEY_MODIFIERS,
	MUI_KEY_RSHIFT,
	MUI_KEY_LCTRL,		MUI_KEY_RCTRL,
	MUI_KEY_LALT,		MUI_KEY_RALT,
	MUI_KEY_LSUPER,		MUI_KEY_RSUPER,
	MUI_KEY_CAPSLOCK,
	MUI_KEY_MODIFIERS_LAST,
	MUI_KEY_F1 = 0x100,	MUI_KEY_F2,	MUI_KEY_F3,	MUI_KEY_F4,
	MUI_KEY_F5,			MUI_KEY_F6,	MUI_KEY_F7,	MUI_KEY_F8,
	MUI_KEY_F9,			MUI_KEY_F10,MUI_KEY_F11,MUI_KEY_F12,
} mui_key_e;

typedef enum mui_modifier_e {
	MUI_MODIFIER_LSHIFT 	= (1 << (MUI_KEY_LSHIFT - MUI_KEY_MODIFIERS)),
	MUI_MODIFIER_RSHIFT 	= (1 << (MUI_KEY_RSHIFT - MUI_KEY_MODIFIERS)),
	MUI_MODIFIER_LCTRL 		= (1 << (MUI_KEY_LCTRL - MUI_KEY_MODIFIERS)),
	MUI_MODIFIER_RCTRL 		= (1 << (MUI_KEY_RCTRL - MUI_KEY_MODIFIERS)),
	MUI_MODIFIER_LALT 		= (1 << (MUI_KEY_LALT - MUI_KEY_MODIFIERS)),
	MUI_MODIFIER_RALT 		= (1 << (MUI_KEY_RALT - MUI_KEY_MODIFIERS)),
	MUI_MODIFIER_RSUPER 	= (1 << (MUI_KEY_RSUPER - MUI_KEY_MODIFIERS)),
	MUI_MODIFIER_LSUPER 	= (1 << (MUI_KEY_LSUPER - MUI_KEY_MODIFIERS)),

	// special flag, trace events handling for this event
	MUI_MODIFIER_EVENT_TRACE= (1 << 15),
	MUI_MODIFIER_SHIFT 		= (MUI_MODIFIER_LSHIFT | MUI_MODIFIER_RSHIFT),
	MUI_MODIFIER_CTRL 		= (MUI_MODIFIER_LCTRL | MUI_MODIFIER_RCTRL),
	MUI_MODIFIER_ALT 		= (MUI_MODIFIER_LALT | MUI_MODIFIER_RALT),
	MUI_MODIFIER_SUPER 		= (MUI_MODIFIER_LSUPER | MUI_MODIFIER_RSUPER),
} mui_modifier_e;

/*
 * The following constants are in UTF8 format, and relate to glyphs in
 * the TTF fonts
 */
/* These are from the icon font */
#define MUI_ICON_FOLDER 		""
#define MUI_ICON_FOLDER_OPEN 	""
#define MUI_ICON_ROOT 			""
#define MUI_ICON_FILE 			""
#define MUI_ICON_POPUP_ARROWS	""
#define MUI_ICON_HOME			""
#define MUI_ICON_SBAR_UP		""
#define MUI_ICON_SBAR_DOWN		""
#define MUI_ICON_FLOPPY5		""
#define MUI_ICON_HARDDISK		""

/* These are specific to our custom version of the Charcoal System font */
#define MUI_GLYPH_APPLE 		""	// solid apple
#define MUI_GLYPH_OAPPLE 		""	// open apple
#define MUI_GLYPH_COMMAND 		""
#define MUI_GLYPH_OPTION 		""
#define MUI_GLYPH_CONTROL 		""
#define MUI_GLYPH_SHIFT 		""
#define MUI_GLYPH_TICK			""	// tickmark for menus
#define MUI_GLYPH_SUBMENU		"▶"	// custom, for the hierarchical menus
#define MUI_GLYPH_IIE			""	// custom, IIe glyph
#define MUI_GLYPH_POPMARK		"▼"	// custom, popup menu marker
/* These are also from Charcoal System font (added to the original) */
#define MUI_GLYPH_F1			""
#define MUI_GLYPH_F2			""
#define MUI_GLYPH_F3			""
#define MUI_GLYPH_F4			""
#define MUI_GLYPH_F5			""
#define MUI_GLYPH_F6			""
#define MUI_GLYPH_F7			""
#define MUI_GLYPH_F8			""
#define MUI_GLYPH_F9			""
#define MUI_GLYPH_F10			""
#define MUI_GLYPH_F11			""
#define MUI_GLYPH_F12			""

typedef uint64_t mui_time_t;

/*
 * Event description. pretty standard stuff here -- the 'when' field is
 * only used really to detect double clicks so far.
 *
 * Even handlers should return true if the event was handled, (in which case
 * even processing stops for that event) or false to continue passing the even
 * down the chain.
 *
 * Events are passed to the top window first, and then down the chain of
 * windows, until one of them returns true.
 * Implicitely, it means the menubar gets to see the events first, even clicks,
 * even if the click wasn't in the menubar. This is also true of key events of
 * course, which allows the menu to detect key combos, first.
 */
typedef struct mui_event_t {
	mui_event_e 		type;
	mui_time_t 			when;
	mui_modifier_e 		modifiers;
	union {
		struct key {
			uint32_t 		key;	// ASCII or mui_key_e
			bool 			up;
		} 				key;
		struct {
			uint32_t 		button : 4,
							count : 2; // click count
			c2_pt_t 		where;
		} 				mouse;
		struct {
			int32_t 		delta;
			c2_pt_t 		where;
		} 				wheel;
		struct {	// MUI_EVENT_TEXT is of variable size!
			uint32_t 		size;
			uint8_t  		text[0];
		}				text;
	};
} mui_event_t;

/* Just a generic buffer for UTF8 text */
DECLARE_C_ARRAY(uint8_t, mui_utf8, 8);
IMPLEMENT_C_ARRAY(mui_utf8);

/*
 * Key equivalent, used to match key events to menu items
 * Might be extended to controls, right now only the 'key' is checked,
 * mostly for Return and ESC.
 */
typedef union mui_key_equ_t {
	struct {
		uint16_t 		mod;
		uint16_t 		key;
	};
	uint32_t 		value;
} mui_key_equ_t;

#define MUI_KEY_EQU(_mask, _key) \
		(mui_key_equ_t){ .mod = (_mask), .key = (_key) }

struct mui_t;
struct mui_control_t;
struct mui_window_t;


/*!
 * References allows arbitrary code to keep a 'handle' on either
 * a window or a control. This is used for example to keep track of
 * the currently focused control.
 * Client code Must Not keep random pointers on control and windows,
 * as they could get deleted and they will end up with a dangling
 * pointer.
 * Instead, client code create a reference, and use that reference
 * to keep track of the object. If an object is deleted, all it's
 * current references are reset to NULL, so the client code can
 * detect that the object is gone just by checking that its pointer
 * is still around. Otherwise, it's just gone.
 */
struct mui_ref_t;

typedef struct mui_refqueue_t {
	TAILQ_HEAD(head, mui_ref_t) 	head;
} mui_refqueue_t;

typedef void (*mui_deref_p)(
			struct mui_ref_t *		ref);

typedef struct mui_ref_t {
	// in refqueue's 'head'
	TAILQ_ENTRY(mui_ref_t) 		self;
	mui_refqueue_t * 			queue;
	// OPTIONAL arbitrary kind set when referencing an object.
	uint32_t 					kind;
	uint32_t 					alloc : 1, trace : 1, count : 8;
	// OPTIONAL: called if the object win/control get disposed or
	// otherwise dereferenced.
	mui_deref_p					 deref;
} _mui_ref_t;	// this is not a 'user' type.

/*
 * Window and Control references
 * While these two count technically be a union, I've deciced for separate
 * types to enforce the type checking.
 */
typedef struct mui_window_ref_t {
	_mui_ref_t 					ref;
	struct mui_window_t * 		window;
} mui_window_ref_t;

typedef struct mui_control_ref_t {
	_mui_ref_t 					ref;
	struct mui_control_t * 		control;
} mui_control_ref_t;

/*!
 * Initializes a reference to 'control', with the (optional) kind.
 * if 'ref' is NULL a new reference is allocated and returned, will be
 * freed on deref().
 * 'kind' is an optional arbitrary value that can be used to identify
 * the reference, it has no meaning to the library.
 */
mui_control_ref_t *
mui_control_ref(
		mui_control_ref_t *		ref,
		struct mui_control_t *	control,
		uint32_t 				kind);
void
mui_control_deref(
		mui_control_ref_t *		ref);
/*!
 * Initializes a reference to 'window', with the (optional) kind.
 * if 'ref' is NULL a new reference is allocated and returned, will be
 * freed on deref().
 * 'kind' is an optional arbitrary value that can be used to identify
 * the reference, it has no meaning to the library.
 */
mui_window_ref_t *
mui_window_ref(
		mui_window_ref_t *		ref,
		struct mui_window_t * 	win,
		uint32_t 				kind);
void
mui_window_deref(
		mui_window_ref_t *		ref);

typedef struct mui_listbox_elem_t {
	uint32_t 					disabled : 1;
	// currently this is a UTF8 string using the 'icons' font
	char 						icon[8];	// UTF8 icon
	// default 'LDEF' is to draw the 'elem' string
	void * 						elem; // char * or... ?
} mui_listbox_elem_t;

DECLARE_C_ARRAY(mui_listbox_elem_t, mui_listbox_elems, 2);
IMPLEMENT_C_ARRAY(mui_listbox_elems);

struct mui_control_t;
struct mui_window_t;
struct mui_listbox_elem_t;

/*
 * Window DEFinition -- Handle all related to a window, from drawing to
 * event handling.
 */
enum {
	MUI_WDEF_INIT = 0,	// param is NULL
	MUI_WDEF_DISPOSE,	// param is NULL
	MUI_WDEF_DRAW,		// param is mui_drawable_t*
	MUI_WDEF_EVENT,		// param is mui_event_t*
	MUI_WDEF_SELECT,	// param is NULL
	MUI_WDEF_DESELECT,	// param is NULL
};
typedef bool (*mui_wdef_p)(
			struct mui_window_t * win,
			uint8_t 		what,
			void * 			param);
enum mui_cdef_e {
	MUI_CDEF_INIT = 0,	// param is NULL
	MUI_CDEF_DISPOSE,	// param is NULL
	MUI_CDEF_DRAW,		// param is mui_drawable_t*
	MUI_CDEF_EVENT,		// param is mui_event_t*
	MUI_CDEF_SET_STATE,	// param is int*
	MUI_CDEF_SET_VALUE,	// param is int*
	MUI_CDEF_SET_FRAME,	// param is c2_rect_t*
	MUI_CDEF_SET_TITLE,	// param is char * (utf8)
	// Used when hot-key is pressed, change control value
	// to simulate a click
	MUI_CDEF_SELECT,
	// used when a window is selected, to set the focus to the
	// first control that can accept it
	MUI_CDEF_FOCUS,	// param is int* with 0,1
	MUI_CDEF_CAN_FOCUS,// param is NULL, return true or false
};
typedef bool (*mui_cdef_p)(
				struct mui_control_t * 	c,
				uint8_t 	what,
				void * 		param);

/* This is currently unused */
typedef void (*mui_ldef_p)(
				struct mui_control_t * 	c,
				uint32_t 	elem_index,
				struct mui_listbox_elem_t * elem);

/*!
 * Timer callback definition. Behaves in a pretty standard way; the timer
 * returns 0 to be cancelled (for one shot timers for example) or return
 * the delay to the next call (that will be added to 'now' to get the next)
 */
typedef mui_time_t (*mui_timer_p)(
				struct mui_t * mui,
				mui_time_t 	now,
				void * 		param);
/*!
 * Actions are the provided way to add custom response to events for the
 * application; action handlers are called for a variety of things, from clicks
 * in controls, to menu selections, to window close etc.
 *
 * The 'what' parameter is a 4 character code, that can be used to identify
 * the action, and the 'param' is a pointer to a structure that depends on
 * the 'what' action (hopefully documented with that action constant)
 *
 * the 'cb_param' is specific to this action function pointer and is passed as
 * is to the callback, this is the pointer you pass to mui_window_add_action()
 */
typedef int (*mui_window_action_p)(
				struct mui_window_t * win,
				void * 		cb_param,
				uint32_t 	what,
				void * 		param);
typedef int (*mui_control_action_p)(
				struct mui_control_t *c,
				void * 		cb_param,
				uint32_t 	what,
				void * 		param);
/*
 * This is a standardized way of installing 'action' handlers onto windows
 * and controls. The 'current' field is used to prevent re-entrance. This structure
 * is opaque and is not accessible by the application, typically.
 */
typedef struct mui_action_t {
	STAILQ_ENTRY(mui_action_t) 	self;
	uint32_t 					current; // prevents re-entrance
	union {
		mui_window_action_p 	window_cb;
		mui_control_action_p 	control_cb;
	};
	void *						cb_param;
} mui_action_t;

typedef STAILQ_HEAD(, mui_action_t) mui_action_queue_t;

struct cg_surface_t;
struct cg_ctx_t;

/*
 * Describes a pixmap.
 * And really, only bpp:32 for ARGB is supported if you want to use 'cg' to draw
 * on it,
 * 8bpp is also used for alpha masks, in which case only the pixman API is used.
 * (Alpha mask is used for text rendering)
 */
typedef struct mui_pixmap_t {
	uint8_t * 					pixels;
	uint32_t 					bpp : 8;
	c2_pt_t 					size;
	uint32_t 					row_bytes;
} mui_pixmap_t;

typedef pixman_region32_t 	mui_region_t;

DECLARE_C_ARRAY(mui_region_t, mui_clip_stack, 2);

/*
 * The Drawable is a drawing context. The important feature
 * of this is that it keeps a context for the pixman library destination
 * image, AND also the context for the 'cg' vectorial library.
 *
 * Furthermore it keeps track of a stack of clipping rectangles, and is able
 * to 'sync' the current clipping area for either (or both) cg and libpixman.
 *
 * Important note: the cg vectorial library coordinate system is placed on the
 * space *between* pixels, ie, if you moveto(1,1) and draw a line down, you
 * will light up pixels in columns zero AND one (at half transparency).
 * This differs significantly from for example, pixman that is uses pixel
 * coordinates on hard pixels.
 *
 * It's worth remembering as if you draw for example around the border of a
 * control, it will very likely be 'clipped' somewhat because half the pixels
 * are technically outside the control bounding/clipping rectangle.
 * You can easily adjust for this by adding 0.5 to the coordinates, if you
 * require it.
 *
 * Other imporant note: The clipping stack is only converted to pixman/cg when
 * the client code asks for the context. So you must make sure not to 'cache'
 * the context too early, otherwise the clipping won't work.
 * Bad:
 * 	struct cg_t * cg = mui_drawable_get_cg(dr);
 * 	mui_drawable_clip_push(dr, &r);
 * 	...
 * 	mui_drawable_clip_pop(dr);
 * Good:
 * 	mui_drawable_clip_push(dr, &r);
 * 	struct cg_t * cg = mui_drawable_get_cg(dr);
 * 	...
 * 	mui_drawable_clip_pop(dr);
 */
typedef struct mui_drawable_t {
	mui_pixmap_t				pix;	// *has* to be first in struct
	void * 						_pix_hash; // used to detect if pix has changed
	struct cg_surface_t * 		cg_surface;
	struct cg_ctx_t *			cg;		// Do not to use these directly
	union pixman_image *		pixman;	// Do not to use these directly
	uint						pixman_clip_dirty: 1,
								cg_clip_dirty : 1,
								dispose_pixels : 1,
								dispose_drawable : 1;
	// not used internally, but useful for the application
	struct {
		float 						opacity;
		c2_pt_t 					size;
		uint						id, kind;
	}							texture;
	// (default) position in destination when drawing (optional)
	c2_pt_t 					origin;
	mui_clip_stack_t			clip;
} mui_drawable_t;

// Use IMPLEMENT_C_ARRAY(mui_drawable_array); if you need this
DECLARE_C_ARRAY(mui_drawable_t *, mui_drawable_array, 4);

/*
 * Drawable related
 */
/* create a new mui_drawable of size w x h, bpp depth.
 * Optionally allocate the pixels if pixels is NULL. Allocated pixels
 * are not cleared to white/zero. */
mui_drawable_t *
mui_drawable_new(
		c2_pt_t 		size,
		uint8_t 		bpp,
		void * 			pixels, // if NULL, will allocate
		uint32_t 		row_bytes);
/* initialize a mui_drawable_t structure with the given parameters
 * note it is not assumed 'd' contains anything valid, it will be
 * overwritten */
mui_drawable_t *
mui_drawable_init(
		mui_drawable_t * d,
		c2_pt_t 		size,
		uint8_t 		bpp,
		void * 			pixels, // if NULL, will allocate
		uint32_t 		row_bytes);
void
mui_drawable_dispose(
		mui_drawable_t * dr);
// Clear, but do not dispose of the drawable
void
mui_drawable_clear(
		mui_drawable_t * dr);

// get/allocate a pixman structure for this drawable
union pixman_image *
mui_drawable_get_pixman(
		mui_drawable_t * dr);
// get/allocate a cg drawing context for this
struct cg_ctx_t *
mui_drawable_get_cg(
		mui_drawable_t * dr);
// return 0 (no intersect), 1: fully contained and 2: partial contains
int
mui_drawable_clip_intersects(
		mui_drawable_t * dr,
		c2_rect_p r );
void
mui_drawable_set_clip(
		mui_drawable_t * dr,
		c2_rect_array_p clip );
int
mui_drawable_clip_push(
		mui_drawable_t * dr,
		c2_rect_p 		r );
int
mui_drawable_clip_push_region(
		mui_drawable_t * dr,
		pixman_region32_t * rgn );
int
mui_drawable_clip_substract_region(
		mui_drawable_t * dr,
		pixman_region32_t * rgn );
void
mui_drawable_clip_pop(
		mui_drawable_t * dr );
pixman_region32_t *
mui_drawable_clip_get(
		mui_drawable_t * dr);


/*
 * Your typical ARGB color. Note that the components are NOT
 * alpha-premultiplied at this stage.
 * This struct should be able to be passed as a value, not a pointer
 */
typedef union mui_color_t {
	struct {
		uint8_t a,r,g,b;
	} __attribute__((packed));
	uint32_t value;
	uint8_t v[4];
} mui_color_t;

typedef struct mui_control_color_t {
	mui_color_t fill, frame, text;
} mui_control_color_t;

#define MUI_COLOR(_v) ((mui_color_t){ .value = (_v)})

#define CG_COLOR(_c) (struct cg_color_t){ \
			.a = (_c).a / 255.0, .r = (_c).r / 255.0, \
			.g = (_c).g / 255.0, .b = (_c).b / 255.0 }
/*
 * Pixman use premultiplied alpha values
 */
#define PIXMAN_COLOR(_c) (pixman_color_t){ \
			.alpha = (_c).a * 257, .red = (_c).r * (_c).a, \
			.green = (_c).g * (_c).a, .blue = (_c).b * (_c).a }

typedef struct mui_font_t {
	mui_drawable_t			 	font;	// points to ttc pixels!
	char * 						name;	// not filename, internal name, aka 'main'
	uint		 				size;	// in pixels
	TAILQ_ENTRY(mui_font_t) 	self;
	struct stb_ttc_info  		ttc;
} mui_font_t;

/*
 * Font related
 */
void
mui_font_init(
		struct mui_t *	ui);
void
mui_font_dispose(
		struct mui_t *	ui);

mui_font_t *
mui_font_find(
		struct mui_t *	ui,
		const char *	name);
mui_font_t *
mui_font_from_mem(
		struct mui_t *	ui,
		const char *	name,
		uint		 	size,
		const void *	font_data,
		uint		 	font_size );
/*
 * Draw a text string at 'where' in the drawable 'dr' with the
 * given color. This doesn't handle line wrapping, or anything,
 * it just draws the text at the given position.
 * If you want more fancy text drawing, use mui_font_textbox()
 */
void
mui_font_text_draw(
		mui_font_t *	font,
		mui_drawable_t *dr,
		c2_pt_t 		where,
		const char *	text,
		uint		 	text_len,
		mui_color_t 	color);
/*
 * This is a low level function to measure a text string, it returns
 * the width of the string in pixels, and fills the 'm' structure with
 * the position of each glyph in the string. Note that the returned
 * values are all in FIXED POINT format.
 */
int
mui_font_text_measure(
		mui_font_t *	font,
		const char *	text,
		struct stb_ttc_measure *m );

typedef enum mui_text_e {
	// 2 bits for horizontal alignment, 2 bits for vertical alignment
	MUI_TEXT_ALIGN_LEFT 	= 0,
	MUI_TEXT_ALIGN_CENTER	= (1 << 0),
	MUI_TEXT_ALIGN_RIGHT	= (1 << 1),
	MUI_TEXT_ALIGN_TOP		= 0,
	MUI_TEXT_ALIGN_MIDDLE	= (MUI_TEXT_ALIGN_CENTER << 2),
	MUI_TEXT_ALIGN_BOTTOM	= (MUI_TEXT_ALIGN_RIGHT << 2),
	MUI_TEXT_ALIGN_FULL		= (1 << 5),
	MUI_TEXT_ALIGN_COMPACT	= (1 << 6),	// compact line spacing
	MUI_TEXT_DEBUG			= (1 << 7),
	MUI_TEXT_STYLE_BOLD		= (1 << 8),	// Synthetic (ugly) bold
	MUI_TEXT_STYLE_ULINE	= (1 << 9), // Underline
	MUI_TEXT_STYLE_NARROW	= (1 << 10),// Synthetic narrow
	MUI_TEXT_FLAGS_COUNT	= 11,
} mui_text_e;

/*
 * Draw a text string in a bounding box, with the given color. The
 * 'flags' parameter is a combination of MUI_TEXT_ALIGN_* flags.
 * This function will handle line wrapping, and will draw as much text
 * as it can in the given box.
 * The 'text' parameter can be a UTF8 string, and the 'text_len' is
 * the number of bytes in the string (or zero), not the number of
 * glyphs.
 * The 'text' string can contain '\n' to force a line break.
 */
void
mui_font_textbox(
		mui_font_t *	font,
		mui_drawable_t *dr,
		c2_rect_t 		bbox,
		const char *	text,
		uint		 	text_len,
		mui_color_t 	color,
		mui_text_e 		flags );

// this is what is returned by mui_font_measure()
typedef struct mui_glyph_t {
	uint32_t 	pos; 	// position in text, in *bytes*
	uint32_t 	w;		// width of the glyph, in *pixels*
	float		x;		// x position in *pixels*
	uint32_t	index;	// cache index, for internal use, do not change
	uint32_t 	glyph;  // Unicode codepoint
} mui_glyph_t;

DECLARE_C_ARRAY(mui_glyph_t, mui_glyph_array, 8,
		uint line_break : 1;
		int x, y, t, b; float w;);
DECLARE_C_ARRAY(mui_glyph_array_t, mui_glyph_line_array, 8,
		uint margin_left, margin_right,	// minimum x, and max width
		height; );

/*
 * Measure a text string, return the number of lines, and each glyphs
 * position already aligned to the MUI_TEXT_ALIGN_* flags.
 * Note that the 'compact', 'narrow' flags are used here,
 * the 'compact' flag is used to reduce the line spacing, and the
 * 'narrow' flag is used to reduce the advance between glyphs.
 */
void
mui_font_measure(
		mui_font_t *	font,
		c2_rect_t 		bbox,
		const char *	text,
		uint		 	text_len,
		mui_glyph_line_array_t *lines,
		mui_text_e 		flags);
/*
 * to be used exclusively with mui_font_measure.
 * Draw the lines and glyphs returned by mui_font_measure, with the
 * given color and flags.
 * The significant flags here are no longer the text aligment, but
 * how to render them:
 * + MUI_TEXT_STYLE_BOLD will draw each glyphs twice, offset by 1 pixel
 * + MUI_TEXT_STYLE_ULINE will draw a line under the text glyphs, unless
 *   they have a descent that is lower than the underline.
 */
void
mui_font_measure_draw(
		mui_font_t *	font,
		mui_drawable_t *dr,
		c2_rect_t 		bbox,
		mui_glyph_line_array_t *lines,
		mui_color_t 	color,
		mui_text_e 		flags);
// clear all the lines, and glyph lists. Use it after mui_font_measure
void
mui_font_measure_clear(
		mui_glyph_line_array_t *lines);


enum mui_window_layer_e {
	MUI_WINDOW_LAYER_NORMAL 	= 0,
	MUI_WINDOW_LAYER_MODAL 		= 3,
	MUI_WINDOW_LAYER_ALERT 		= 5,
	MUI_WINDOW_LAYER_TOP 		= 15,
	// Menubar and Menus (popups) are also windows
	MUI_WINDOW_MENUBAR_LAYER 	= MUI_WINDOW_LAYER_TOP - 1,
	MUI_WINDOW_MENU_LAYER,
};

enum mui_window_action_e {
	MUI_WINDOW_ACTION_NONE 		= 0,
	MUI_WINDOW_ACTION_CLOSE		= FCC('w','c','l','s'),
};

/*
 * A window is basically 2 rectangles in 'screen' coordinates. The
 *   'frame' rectangle that encompasses the whole of the window, and the
 *   'content' rectangle that is the area where the controls are drawn.
 * * The 'content' rectangle is always fully included in the 'frame'
 *   rectangle.
 * * The 'frame' rectangle is the one that is used to move the window
 *   around.
 * * All controls coordinates are related to the 'content' rectangle.
 *
 * The window 'layer' is used to determine the order of the windows on the
 * screen, the higher the layer, the more in front the window is.
 * Windows can be 'selected' to bring them to the front -- that brings
 * them to the front of their layer, not necessarily the topmost window.
 *
 * Windows contain an 'action' list that are called when the window
 * wants to signal the application; for example when the window is closed,
 * but it can be used for other things as application requires.
 *
 * Mouse clicks are handled by the window, and the window by first
 * checking if the click is in a control, and if so, passing the event
 * to the control.
 * Any control that receives the 'mouse' down will ALSO receive the
 * mouse DRAG and UP events, even if the mouse has moved outside the
 * control. This is the meaning of the 'control_clicked' field.
 *
 * The 'control_focus' field is used to keep track of the control that
 * has the keyboard focus. This is used to send key events to the
 * control that has the focus. That control can still 'refuse' the event,
 * in which case is it passed in turn to the others, and to the window.
 */
typedef struct mui_window_t {
	TAILQ_ENTRY(mui_window_t)	self;
	struct mui_t *				ui;
	mui_wdef_p 					wdef;
	uint32_t					uid;		// optional, pseudo unique id
	struct {
		uint						hidden: 1,
									disposed : 1,
									layer : 4,
									style: 4,	// specific to the WDEF
									hit_part : 8;
	}							flags;
	c2_pt_t 					click_loc;
	// both these rectangles are in screen coordinates, even tho
	// 'contents' is fully included in 'frame'
	c2_rect_t					frame, content;
	char *						title;
	mui_action_queue_t			actions;
	TAILQ_HEAD(controls, mui_control_t) controls;
	mui_refqueue_t				refs;
	mui_window_ref_t 			lock;
	mui_control_ref_t 			control_clicked;
	mui_control_ref_t	 		control_focus;
	mui_region_t				inval;
} mui_window_t;

/*
 * Window related
 */
/*
 * This is the main function to create a window. The
 * * 'wdef' is the window definition (or NULL for a default window).
 *   see mui_wdef_p for the callback definition.
 * * 'layer' layer to put it in (default to zero for normal windows)
 * * 'instance_size' zero (for default) or the size of the window instance
 *   object that is returned, you can therefore have your own custom field
 *   attached to a window.
 */
mui_window_t *
mui_window_create(
		struct mui_t *	ui,
		c2_rect_t 		frame,
		mui_wdef_p 		wdef,
		uint8_t 		layer,
		const char *	title,
		uint32_t 		instance_size);
// Dispose of a window and it's content (controls).
/*
 * Note: if the window is 'locked' the window is not freed immediately.
 * This is to prevent re-entrance problems. This allows window actions to
 * delete their own window without crashing.
 */
void
mui_window_dispose(
		mui_window_t *	win);
// Invalidate 'r' in window coordinates, or the whole window if 'r' is NULL
void
mui_window_inval(
		mui_window_t *	win,
		c2_rect_t * 	r);
// return true if the window is the frontmost window (in that window's layer)
bool
mui_window_isfront(
		mui_window_t *	win);
// return the top (non menubar/menu) window
mui_window_t *
mui_window_front(
		struct mui_t *ui);

// move win to the front (of its layer), return true if it was moved
bool
mui_window_select(
		mui_window_t *	win);
// call the window action callback(s), if any
void
mui_window_action(
		mui_window_t * 	c,
		uint32_t 		what,
		void * 			param );
// Add an action callback for this window
void
mui_window_set_action(
		mui_window_t * 	c,
		mui_window_action_p	cb,
		void * 			param );
// return the window whose UID is 'uid', or NULL if not found
mui_window_t *
mui_window_get_by_id(
		struct mui_t *	ui,
		uint32_t 		uid );
// set the window UID
void
mui_window_set_id(
		mui_window_t *	win,
		uint32_t 		uid);

struct mui_menu_items_t;

/*
 * This is a menu item descriptor (also used for the titles, bar a few bits).
 * This is not a *control* in the window, instead this is used to describe
 * the menus and menu item controls that are created when the menu becomes
 * visible.
 */
typedef struct mui_menu_item_t {
	uint32_t 					disabled : 1,
								hilited : 1,
								is_menutitle : 1;
	uint32_t 					index: 9;
	uint32_t 					uid;
	char * 						title;
	// currently only supported for menu titles
	const uint32_t *			color_icon;		// optional, ARGB colors
	char  						mark[8];		// UTF8 -- Charcoal
	char						icon[8];		// UTF8 -- Wider, icon font
	char 						kcombo[16];		// UTF8 -- display only
	mui_key_equ_t 				key_equ;		// keystroke to select this item
	struct mui_menu_item_t *	submenu;
	c2_coord_t					location;		// calculated by menu creation code
	c2_coord_t					height;
} mui_menu_item_t;

/*
 * The menu item array is atypical as the items ('e' field) are not allocated
 * by the array, but by the menu creation code. This is because the menu
 * reuses the pointer to the items that is passed when the menu is added to
 * the menubar.
 * the 'read only' field is used to prevent the array from trying to free the
 * items when being disposed.
 */
DECLARE_C_ARRAY(mui_menu_item_t, mui_menu_items, 2,
				bool read_only; );
IMPLEMENT_C_ARRAY(mui_menu_items);

enum mui_menubar_action_e {
	// parameter is a mui_menu_item_t* for the first item of the menu,
	// this is exactly the parameter passed to add_simple()
	// you can use this to disable/enable menu items etc
	MUI_MENUBAR_ACTION_PREPARE 		= FCC('m','b','p','r'),
	// parameter 'target' is a mui_menuitem_t*
	MUI_MENUBAR_ACTION_SELECT 		= FCC('m','b','a','r'),
};
/*
 * Menu related.
 * Menubar, and menus/popups are windows as well, in a layer above the
 * normal ones.
 */
mui_window_t *
mui_menubar_new(
		struct mui_t *	ui );
// return the previously created menubar (or NULL)
mui_window_t *
mui_menubar_get(
		struct mui_t *	ui );

/*
 * Add a menu to the menubar. 'items' is an array of mui_menu_item_t
 * terminated by an element with a NULL title.
 *
 * Note: The array is NOT const, it will be tweaked for storing items
 * position, it can also be tweaked to set/reset the disabled state,
 * check marks etc
 *
 * Once created, you can do a mui_popupmenu_get_items() to get the array,
 * modify it (still make sure there is a NULL item at the end) then
 * call mui_popupmenu_prepare() to update the menu.
 */
struct mui_control_t *
mui_menubar_add_simple(
		mui_window_t *		win,
		const char * 		title,
		uint32_t 			menu_uid,
		mui_menu_item_t * 	items );
struct mui_control_t *
mui_menubar_add_menu(
		mui_window_t *		win,
		uint32_t 			menu_uid,
		mui_menu_item_t * 	items,
		uint 				count );

/* Turn off any highlighted menu titles */
mui_window_t *
mui_menubar_highlight(
		mui_window_t *	win,
		bool 			ignored );


enum mui_button_style_e {
	MUI_BUTTON_STYLE_NORMAL = 0,
	MUI_BUTTON_STYLE_DEFAULT = 1,
	MUI_BUTTON_STYLE_RADIO,
	MUI_BUTTON_STYLE_CHECKBOX,
};

enum mui_control_state_e {
	MUI_CONTROL_STATE_NORMAL = 0,
	MUI_CONTROL_STATE_HOVER,
	MUI_CONTROL_STATE_CLICKED,
	MUI_CONTROL_STATE_DISABLED,
	MUI_CONTROL_STATE_COUNT
};

enum mui_control_action_e {
	MUI_CONTROL_ACTION_NONE = 0,
	MUI_CONTROL_ACTION_VALUE_CHANGED	= FCC('c','v','a','l'),
	MUI_CONTROL_ACTION_CLICKED			= FCC('c','l','k','d'),
	MUI_CONTROL_ACTION_SELECT			= FCC('c','s','e','l'),
	MUI_CONTROL_ACTION_DOUBLECLICK		= FCC('c','d','c','l'),
};

/*
 * Control record... this are the 'common' fields, most of the controls
 * have their own 'extended' record using their own fields.
 */
typedef struct mui_control_t {
	TAILQ_ENTRY(mui_control_t) 	self;
	struct mui_window_t *		win;
	mui_refqueue_t				refs;
	mui_control_ref_t 			lock;
	mui_cdef_p 					cdef;
	uint32_t					state;
	uint32_t 					type;
	uint32_t					style;
	struct {
		uint		 			hidden : 1,
								hit_part : 8;
	}							flags;
	uint32_t					value;
	uint32_t 					uid;
	uint32_t 					uid_mask;		// for radio buttons
	c2_rect_t					frame;
	mui_key_equ_t				key_equ; // keystroke to select this control
	char *						title;
	mui_action_queue_t			actions;
} mui_control_t;

/*
 * Control related
 */
/*
 * This is the 'low level' control creation function, you can pass the
 * 'cdef' function pointer and a control 'type' that will be passed to it,
 * so you can implement variants of controls.
 * The instance_size is the size of the extended control record, if any.
 */
mui_control_t *
mui_control_new(
		mui_window_t * 	win,
		uint32_t 		type,	// specific to the CDEF
		mui_cdef_p 		cdef,
		c2_rect_t 		frame,
		const char *	title,
		uint32_t 		uid,
		uint32_t 		instance_size );
void
mui_control_dispose(
		mui_control_t * c );
uint32_t
mui_control_get_type(
		mui_control_t * c );
uint32_t
mui_control_get_uid(
		mui_control_t * c );
mui_control_t *
mui_control_locate(
		mui_window_t * 	win,
		c2_pt_t 		pt );
mui_control_t *
mui_control_get_by_id(
		mui_window_t * 	win,
		uint32_t 		uid );
void
mui_control_inval(
		mui_control_t * c );
void
mui_control_set_frame(
		mui_control_t * c,
		c2_rect_t *		frame );
void
mui_control_action(
		mui_control_t * c,
		uint32_t 		what,
		void * 			param );
void
mui_control_set_action(
		mui_control_t * c,
		mui_control_action_p	cb,
		void * 			param );
void
mui_control_set_state(
		mui_control_t * c,
		uint32_t 		state );
uint32_t
mui_control_get_state(
		mui_control_t * c );

int32_t
mui_control_get_value(
		mui_control_t * c);
int32_t
mui_control_set_value(
		mui_control_t * c,
		int32_t 		selected);
const char *
mui_control_get_title(
		mui_control_t * c );
void
mui_control_set_title(
		mui_control_t * c,
		const char * 	text );
/* Sets the focus to control 'c' in that window, return true if that
 * control was able to take the focus, or false if it wasn't (for example any
 * control that are not focusable will return false)
 */
bool
mui_control_set_focus(
		mui_control_t * c );
/* Returns true if the control has the focus */
bool
mui_control_has_focus(
		mui_control_t * c );
/* Switch focus to the next/previous control in the window */
mui_control_t *
mui_control_switch_focus(
		mui_window_t * win,
		int 			dir );

/* Drawable control is just an offscreen buffer (icon, pixel view) */
mui_control_t *
mui_drawable_control_new(
		mui_window_t * 	win,
		c2_rect_t 		frame,
		mui_drawable_t * dr,
		mui_drawable_t * mask,
		uint16_t 		flags);
mui_drawable_t *
mui_drawable_control_get_drawable(
		mui_control_t * c);

mui_control_t *
mui_button_new(
		mui_window_t * 	win,
		c2_rect_t 		frame,
		uint8_t			style,	// one of mui_button_style_e
		const char *	title,
		uint32_t 		uid );
/*
 * Create a static text box. Font is optional (default to the system main font),
 * flags corresponds to the MUI_TEXT_ALIGN_* * PLUS the extra(s) listed below.
 */
enum mui_textbox_e {
	// draw the frame around the text box
	MUI_CONTROL_TEXTBOX_FRAME		= (1 << (MUI_TEXT_FLAGS_COUNT+1)),
	MUI_CONTROL_TEXTBOX_FLAGS_COUNT = (MUI_TEXT_FLAGS_COUNT+1),
};
mui_control_t *
mui_textbox_new(
		mui_window_t * 	win,
		c2_rect_t 		frame,
		const char *	text,
		const char * 	font,
		uint32_t		flags );
mui_control_t *
mui_groupbox_new(
		mui_window_t * 	win,
		c2_rect_t 		frame,
		const char *	title,
		uint32_t		flags );
/*
 * Text editor control
 */
enum {
	// do we handle multi-line text? If zero, we only handle one line
	MUI_CONTROL_TEXTEDIT_VERTICAL	= 1 << (MUI_CONTROL_TEXTBOX_FLAGS_COUNT+1),
	MUI_CONTROL_TEXTEDIT_FLAGS_COUNT = (MUI_CONTROL_TEXTBOX_FLAGS_COUNT+1),
};

mui_control_t *
mui_textedit_control_new(
		mui_window_t * 	win,
		c2_rect_t 		frame,
		uint32_t 		flags);
void
mui_textedit_set_text(
		mui_control_t * c,
		const char * text);
void
mui_textedit_set_selection(
		mui_control_t * c,
		uint			start,
		uint			end);

/* Page step and line step are optional, they default to '30' pixels and
 * the 'visible' area of the scrollbar, respectively.
 * If you want to for example have a scrollbar that scrolls by 5 when you
 * click the arrows, and by 20 when you click the bar, you would set the
 * line_step to 5, and the page_step to 20.
 */
mui_control_t *
mui_scrollbar_new(
		mui_window_t * 	win,
		c2_rect_t 		frame,
		uint32_t 		uid,
		uint32_t 		line_step,
		uint32_t 		page_step);
uint32_t
mui_scrollbar_get_max(
		mui_control_t * c);
void
mui_scrollbar_set_max(
		mui_control_t * c,
		uint32_t 		max);
void
mui_scrollbar_set_page(
		mui_control_t * c,
		uint32_t 		page);

mui_control_t *
mui_listbox_new(
		mui_window_t * 	win,
		c2_rect_t 		frame,
		uint32_t 		uid );
void
mui_listbox_prepare(
		mui_control_t * c);
mui_listbox_elems_t *
mui_listbox_get_elems(
		mui_control_t * c);

mui_control_t *
mui_separator_new(
		mui_window_t * 	win,
		c2_rect_t 		frame);
/* Popup menu control.
 * flags are MUI_TEXT_ALIGN_* -- however this corresponds to the margins
 * of the popup control itself when placed into it's 'frame' -- the
 * popup will be placed left,right,center of the frame rectangle depending.
 */
mui_control_t *
mui_popupmenu_new(
		mui_window_t *	win,
		c2_rect_t 		frame,
		const char * 	title,
		uint32_t 		uid,
		uint32_t		flags);
mui_menu_items_t *
mui_popupmenu_get_items(
		mui_control_t * c);
void
mui_popupmenu_prepare(
		mui_control_t * c);

/*
 * Standard file dialog
 */
enum mui_std_action_e {
	MUI_STDF_ACTION_NONE 		= 0,
	// parameter 'target' is a char * with full pathname of selected file
	MUI_STDF_ACTION_SELECT 		= FCC('s','t','d','s'),
	MUI_STDF_ACTION_CANCEL 		= FCC('s','t','d','c'),
};
enum mui_std_flags_e {
	// 'pattern' is a GNU extended regexp applied to filenames.
	MUI_STDF_FLAG_REGEXP 	= (1 << 0),
	// don't use the 'pref_directory', load, or same preference files
	MUI_STDF_FLAG_NOPREF 	= (1 << 1),
};

/*
 * Standard file dialog related
 *
 * Presents a standard 'get' file dialog, with optional prompt, regexp and
 * start path. The return value is a pointer to a window, you can add your own
 * 'action' function to get MUI_STDF_ACTION_* events.
 * Once in the action function, you can call mui_stdfile_get_selected_path()
 * to get the selected path, and free it when done.
 * NOTE: The dialog does not auto-close, your own action function should close
 * the dialog using mui_window_dispose().
 *
 * The dialog will attempt to remember the last directory used *for this
 * particular pattern* and will use it as the default start path when called
 * again. This is optional, it requires a mui->pref_directory to be set.
 * You can also disable this feature by setting the MUI_STDF_FLAG_NOPREF flag.
 *
 * + 'pattern' is a regular expression to filter the files, or NULL for no
 *    filter.
 * + if 'start_path' is NULL, the $HOME directory is used.
 * + 'where' is the location of the dialog, (0,0) will center it.
 */
mui_window_t *
mui_stdfile_get(
		struct mui_t * 	ui,
		c2_pt_t 		where,
		const char * 	prompt,
		const char * 	pattern,
		const char * 	start_path,
		uint16_t 		flags );
// return the curently selected pathname -- caller must free() it
char *
mui_stdfile_get_selected_path(
		mui_window_t * w );

/*
 * Alert dialog
 */
enum mui_alert_flag_e {
	MUI_ALERT_FLAG_OK 		= (1 << 0),
	MUI_ALERT_FLAG_CANCEL 	= (1 << 1),

	MUI_ALERT_ICON_INFO 	= (1 << 8),

	MUI_ALERT_INFO 			= (MUI_ALERT_FLAG_OK | MUI_ALERT_ICON_INFO),
	MUI_ALERT_WARN 			= (MUI_ALERT_FLAG_OK | MUI_ALERT_FLAG_CANCEL),
};

enum {
	MUI_ALERT_BUTTON_OK		= FCC('o','k',' ',' '),
	MUI_ALERT_BUTTON_CANCEL = FCC('c','a','n','c'),
};

mui_window_t *
mui_alert(
		struct mui_t * 	ui,
		c2_pt_t 		where, // (0,0) will center it
		const char * 	title,
		const char * 	message,
		uint16_t 		flags );

enum mui_time_e {
	MUI_TIME_RES		= 1,
	MUI_TIME_SECOND		= 1000000,
	MUI_TIME_MS			= (MUI_TIME_SECOND/1000),
};
mui_time_t
mui_get_time();

#define MUI_TIMER_COUNT 	64
#define MUI_TIMER_NONE		0xff

typedef uint8_t mui_timer_id_t;

typedef struct mui_timer_group_t {
	uint64_t 					map;
	struct {
		mui_time_t 					when;
		mui_timer_p 				cb;
		void * 						param;
	} 						timers[MUI_TIMER_COUNT];
} mui_timer_group_t;

/*
 * Register 'cb' to be called after 'delay'. Returns a timer id (0 to 63)
 * or MUI_TIMER_NONE if no timer is available.
 * The timer function cb can return 0 for a one shot timer, or another
 * delay that will be added to the current stamp for a further call
 * of the timer.
 * 'param' will be also passed to the timer callback.
 */
mui_timer_id_t
mui_timer_register(
		struct mui_t *	ui,
		mui_timer_p 	cb,
		void *			param,
		uint32_t 		delay);
/*
 * Reset timer 'id' if 'cb' matches what was registered. Set a new delay,
 * or cancel the timer if delay is 0.
 * Returns the time that was left on the timer, or 0 if the timer was
 * not found.
 */
mui_time_t
mui_timer_reset(
		struct mui_t *	ui,
		mui_timer_id_t 	id,
		mui_timer_p 	cb,
		mui_time_t 		delay);

/*
 * This is the head of the mui library, it contains the screen size,
 * the color scheme, the list of windows, the list of fonts, and the
 * clipboard.
 * Basically this is the primary parameter that you keep around.
 */
typedef struct mui_t {
	c2_pt_t 					screen_size;
	struct {
		mui_color_t					clear;
		mui_color_t 				highlight;
	}							color;
	uint16_t 					modifier_keys;
	mui_time_t 					last_click_stamp[MUI_EVENT_BUTTON_MAX];
	int 						draw_debug;
	int							quit_request;
	// this is the sum of all the window's dirty regions, inc moved windows etc
	mui_region_t 				inval;
	// once the pixels have been refreshed, 'inval' is copied to 'redraw'
	// to push the pixels to the screen.
	mui_region_t 				redraw;

	TAILQ_HEAD(, mui_font_t) 	fonts;
	TAILQ_HEAD(windows, mui_window_t) 	windows;
	mui_window_ref_t 			menubar;
	mui_window_ref_t 			event_capture;
	mui_utf8_t 					clipboard;
	mui_timer_group_t			timer;
	// only used by the text editor, as we can only have one carret
	mui_timer_id_t				carret_timer;
	char * 						pref_directory; /* optional */
} mui_t;

void
mui_init(
		mui_t *			ui);
void
mui_dispose(
		mui_t *			ui);
void
mui_draw(
		mui_t *			ui,
		mui_drawable_t *dr,
		uint16_t 		all);
void
mui_run(
		mui_t *			ui);

/* If you want this notification, attach an action function to the
 * menubar */
enum {
	// note this will also be send if the application sets the
	// clipboard with the system's clipboard, so watch out for
	// recursion problems!
	MUI_CLIPBOARD_CHANGED = FCC('c','l','p','b'),
	// this is sent when the user type 'control-v', this gives
	// a chance to the application to do a mui_clipboard_set()
	// with the system's clipboard before it gets pasted.
	// the default is of course to use our internal clipboard
	MUI_CLIPBOARD_REQUEST = FCC('c','l','p','r'),
};
// This will send a notification that the clipboard was set,
// the notification is sent to the menubar, and the menubar will
// send it to the application.
void
mui_clipboard_set(
		mui_t * 		ui,
		const uint8_t * utf8,
		uint		 	len);
const uint8_t *
mui_clipboard_get(
		mui_t * 		ui,
		uint		 *	len);

// return true if the event was handled by the ui
bool
mui_handle_event(
		mui_t *			ui,
		mui_event_t *	ev);
// return true if event 'ev' is a key combo matching key_equ
bool
mui_event_match_key(
		mui_event_t *	ev,
		mui_key_equ_t 	key_equ);
/* Return true if the ui has any active windows, ie, not hidden, zombie;
 * This does not include the menubar, but it does include any menus or popups
 *
 * This is used to decide wether to hide the mouse cursor or not
 */
bool
mui_has_active_windows(
		mui_t *			ui);

/* Return a hash value for string inString */
uint32_t
mui_hash(
	const char * 		inString );
