/*
 * mui_stdfile.c
 *
 * Copyright (C) 2023 Michel Pollet <buserror@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <errno.h>
#include <sys/stat.h>

#ifdef __linux__
#define MUI_HAS_REGEXP
#endif
#ifdef MUI_HAS_REGEXP
#include <regex.h>
#endif
#include "mui.h"
#include "c2_geometry.h"

#include <libgen.h>
#include <dirent.h>

DECLARE_C_ARRAY(char*, string_array, 2);
IMPLEMENT_C_ARRAY(string_array);

#define MUI_STDF_MAX_SUFFIX 16

typedef struct mui_stdfile_t {
	mui_window_t 		win;
	mui_control_t *		ok, *cancel, *home, *root;
	mui_control_t *		listbox, *popup, *recent;
	char * 				pref_file; // pathname we put last path used
	char * 				re_pattern;
	struct {
		char  			 	s[16];
		uint32_t 			hash;
	}				suffix[MUI_STDF_MAX_SUFFIX];
	char *				current_path;
	char *				selected_path;
	string_array_t		pop_path;
	struct {
		mui_control_t 		*save_name;
		mui_control_t 		*create_folder;
	}					save;
#ifdef MUI_HAS_REGEXP
	regex_t 			re;
#endif
} mui_stdfile_t;

enum {
	MUI_STD_FILE_PART_FRAME = 0,
	MUI_STD_FILE_PART_OK,
	MUI_STD_FILE_PART_CANCEL,
	MUI_STD_FILE_PART_HOME,
	MUI_STD_FILE_PART_ROOT,
	MUI_STD_FILE_PART_LISTBOX,
	MUI_STD_FILE_PART_POPUP,
	MUI_STD_FILE_PART_RECENT,
	MUI_STD_FILE_PART_NEW,
	MUI_STD_FILE_PART_SAVE_NAME,
	MUI_STD_FILE_PART_COUNT,
};

static int
_mui_stdfile_sort_cb(
		const void * a,
		const void * b)
{
	const mui_listbox_elem_t * ea = a;
	const mui_listbox_elem_t * eb = b;
	#if 0
	if (ea->icon == MUI_ICON_FOLDER && eb->icon != MUI_ICON_FOLDER)
		return -1;
	if (ea->icon != MUI_ICON_FOLDER && eb->icon == MUI_ICON_FOLDER)
		return 1;
	#endif
	return strcmp(ea->elem, eb->elem);
}

static uint32_t
mui_hash_nocase(
	const char * inString )
{
	if (!inString)
		return 0;
	/* Described http://papa.bretmulvey.com/post/124027987928/hash-functions */
	const uint32_t p = 16777619;
	uint32_t hash = 0x811c9dc5;
	while (*inString)
		hash = (hash ^ tolower(*inString++)) * p;
	hash += hash << 13;
	hash ^= hash >> 7;
	hash += hash << 3;
	hash ^= hash >> 17;
	hash += hash << 5;
	return hash;
}
static int
_mii_stdfile_check_dir(
	const char * path)
{
	const char *home = getenv("HOME");
	if (home && path[0] == '~' && path[1] == '/') {
		char * p = NULL;
		asprintf(&p, "%s%s", home, path + 1);
		path = p;
	} else
		path = strdup(path);
	struct stat st;
	int res = -1;
	if (stat(path, &st) == 0) {
		if (S_ISDIR(st.st_mode))
			res = 0;
	}
	free((void*)path);
	return res;
}

static int
_mui_stdfile_populate(
	mui_stdfile_t * std,
	const char * path)
{
	if (std->current_path && !strcmp(std->current_path, path))
		return 0;
	const char *home = getenv("HOME");
	int dupped = 0;
	if (home && path[0] == '~' && path[1] == '/') {
		char * p = NULL;
		asprintf(&p, "%s%s", home, path + 1);
		path = p;
		dupped = 1;
	}
	printf("%s %s\n", __func__, path);
	errno = 0;
	DIR * dir = opendir(path);
	if (!dir) {
		// show an alert of some sort
		char * msg = NULL;
		asprintf(&msg, "%s\n%s", path,
					strerror(errno));
		mui_alert(std->win.ui, C2_PT(0,0),
					"Could not open directory",
					msg,
					MUI_ALERT_FLAG_OK);
		return -1;
	}
	if (std->current_path)
		free(std->current_path);
	std->current_path = dupped ? (char*)path : strdup(path);
	path = NULL; // this COULD be in the list we are now deleting!
	for (uint i = 0; i < std->pop_path.count; i++)
		free(std->pop_path.e[i]);
	std->pop_path.count = 0;

	mui_control_t *pop = std->popup;
	mui_menu_items_t * items = mui_popupmenu_get_items(pop);
	for (uint i = 0; i < items->count; i++)
		free(items->e[i].title);
	mui_menu_items_clear(items);
	char * p = strdup(std->current_path);
	char * d;
	int item_id = 1000;
	while ((d = basename(p)) != NULL) {
		mui_menu_item_t i = {
			.title = strdup(d),
			.uid = item_id++,
		};
		if (!strcmp(d, "/"))
			strcpy(i.icon, MUI_ICON_ROOT);
		else if (home && !strcmp(p, home))
			strcpy(i.icon, MUI_ICON_HOME);
		else
			strcpy(i.icon, MUI_ICON_FOLDER_OPEN);
		mui_menu_items_push(items, i);
	//	printf(" %s - %s\n", p, d);
		string_array_push(&std->pop_path, strdup(p));
		if (!strcmp(d, "/"))
			break;
		*d = 0;
	}
	free(p);
	mui_menu_item_t z = {};
	mui_menu_items_push(items, z);
	mui_popupmenu_prepare(pop);

	mui_control_t * lb = std->listbox;
	mui_listbox_elems_t * elems = mui_listbox_get_elems(lb);
	for (uint i = 0; i < elems->count; i++)
		free(elems->e[i].elem);	// free all the strings
	mui_listbox_elems_clear(elems);
	struct dirent * ent;
	while ((ent = readdir(dir))) {
		if (ent->d_name[0] == '.')
			continue;
		struct stat st;
		char * full_path = NULL;
		asprintf(&full_path, "%s/%s", std->current_path, ent->d_name);
		stat(full_path, &st);
		free(full_path);
		mui_listbox_elem_t e = {};

		// default to disable, unless we find a reason to enable
		e.disabled = S_ISDIR(st.st_mode) ? 0 : 1;
		// use the regex (if any)  to filter file names
		if (e.disabled && std->re_pattern) {
#ifdef MUI_HAS_REGEXP
			if (regexec(&std->re, ent->d_name, 0, NULL, 0) == 0)
				e.disabled = 0;
#endif
		}
		// handle case when no regexp is set, and no suffixes was set, this
		// we enable all the files by default.
		if (e.disabled && !std->re_pattern)
			e.disabled = std->suffix[0].s[0] ? 1 : 0;
		char *suf = strrchr(ent->d_name, '.');
		// handle the case we have a list of dot suffixes to filter
		if (e.disabled) {
			if (std->suffix[0].s[0] && suf) {
				suf++;
				uint32_t hash = mui_hash_nocase(suf);
				for (int i = 0; i < MUI_STDF_MAX_SUFFIX &&
							std->suffix[i].s[0]; i++) {
					if (hash == std->suffix[i].hash &&
							!strcasecmp(suf, std->suffix[i].s)) {
						e.disabled = 0;
						break;
					}
				}
			}
		}
		e.elem = strdup(ent->d_name);
		if (S_ISDIR(st.st_mode))
			strcpy(e.icon, MUI_ICON_FOLDER);
		else {
			strcpy(e.icon, MUI_ICON_FILE);
			if (suf) {
				if (!strcasecmp(suf, ".woz") || !strcasecmp(suf, ".nib") ||
							!strcasecmp(suf, ".do"))
					strcpy(e.icon, MUI_ICON_FLOPPY5);
				else if ((!strcasecmp(suf, ".dsk") ||
							!strcasecmp(suf, ".po"))) {
					if (st.st_size == 143360)
						strcpy(e.icon, MUI_ICON_FLOPPY5);
				}
			}
		}
		mui_listbox_elems_push(elems, e);
	}
	qsort(elems->e, elems->count,
			sizeof(mui_listbox_elem_t), _mui_stdfile_sort_cb);
	mui_control_set_value(lb, 0);
	mui_listbox_prepare(lb);
	closedir(dir);
	return 0;
}

static int
_mui_stdfile_load_pref(
		mui_stdfile_t * std)
{
	int res = 1;
	FILE * f = fopen(std->pref_file, "r");
	char * path = NULL;
	size_t len = 0;
	if (!f)
		return res;	// populate

	mui_control_t *pop = std->recent;
	mui_menu_items_t * items = mui_popupmenu_get_items(pop);
	for (uint i = 0; i < items->count; i++)
		free(items->e[i].title);
	mui_menu_items_clear(items);
	int line_count = 0;
	const char *home = getenv("HOME");
	do {
		errno = 0;
		if (getline(&path, &len, f) == -1)
			break;
		char *nl = strrchr(path, '\n');
		if (nl)
			*nl = 0;
		if (line_count == 0) {
			if (_mui_stdfile_populate(std, path) == 0) {
				printf("%s last path[%s]: %s\n", __func__,
					std->re_pattern, path);
				res = 0;
			}
		}
		if (home && !strncmp(home, path, strlen(home)) &&
				path[strlen(home)] == '/') {
			path[0] = '~';
			memmove(path + 1, path + strlen(home),
					strlen(path) - strlen(home) + 1);
		}
		int add = 1;
		for (uint ii = 0; ii < items->count; ii++) {
			if (!strcmp(items->e[ii].title, path)) {
				add = 0;
				break;
			}
		}
		if (add)
			add = !_mii_stdfile_check_dir(path);
		if (add && items->count > 10)	// limit to 10 recent paths
			add = 0;
		if (add) {
			int item_id = 10000 + line_count;
			mui_menu_item_t i = {
				.title = strdup(path),
				.uid = item_id,
			};
			char *d = path;
			if (!strcmp(d, "/"))
				strcpy(i.icon, MUI_ICON_ROOT);
			else if (home && !strcmp(d, home))
				strcpy(i.icon, MUI_ICON_HOME);
			else
				strcpy(i.icon, MUI_ICON_FOLDER);
			mui_menu_items_push(items, i);
		}
		line_count++;
	} while (!feof(f) && line_count < 6);
	mui_menu_item_t z = {};
	mui_menu_items_push(items, z);
	mui_popupmenu_prepare(pop);
	fclose(f);
	if (path)
		free(path);
	return res;
}


static int
_mui_stdfile_window_action(
		mui_window_t * 	win,
		void * 			cb_param,
		uint32_t 		what,
		void * 			param)
{
	mui_stdfile_t * std = (mui_stdfile_t*)win;

	switch (what) {
		case MUI_WINDOW_ACTION_CLOSE: {
			// dispose of anything we had allocated
		//	printf("%s close\n", __func__);
			if (std->pref_file)
				free(std->pref_file);
			if (std->re_pattern)
				free(std->re_pattern);
			if (std->current_path)
				free(std->current_path);
			if (std->selected_path)
				free(std->selected_path);
#ifdef MUI_HAS_REGEXP
			regfree(&std->re);
#endif
			mui_control_t *pop = std->popup;
			mui_menu_items_t * items = mui_popupmenu_get_items(pop);
			for (uint i = 0; i < items->count; i++)
				free(items->e[i].title);
			for (uint i = 0; i < std->pop_path.count; i++)
				free(std->pop_path.e[i]);
			// free all the strings for all teh elems, its our responsibility
			mui_listbox_elems_t * elems = mui_listbox_get_elems(std->listbox);
			for (uint i = 0; i < elems->count; i++)
				free(elems->e[i].elem);	// free all the strings

			string_array_free(&std->pop_path);
			std->pop_path.count = 0;
		}	break;
	}
	return 0;
}

static int
_mui_stdfile_control_action(
		mui_control_t * c,
		void * 			cb_param,
		uint32_t 		what,
		void * 			param)
{
	mui_stdfile_t * std = cb_param;
	switch (c->uid) {
		case MUI_STD_FILE_PART_OK: {
			mui_listbox_elem_t * e = mui_listbox_get_elems(std->listbox)->e;
			int idx = mui_control_get_value(std->listbox);
			if (idx < 0 || idx >= (int)mui_listbox_get_elems(std->listbox)->count)
				return 0;
			mui_listbox_elem_t * elem = &e[idx];
			if (elem->disabled)
				break;
			// save pref file
			if (std->pref_file) {
				FILE * f = fopen(std->pref_file, "w");
				if (f) {
					fprintf(f, "%s\n", std->current_path);
					// write recent paths popup back in
					const char *home = getenv("HOME");
					mui_menu_items_t * items = mui_popupmenu_get_items(std->recent);
					for (uint i = 0; i < items->count; i++) {
						if (items->e[i].title &&
								strcmp(items->e[i].title, std->current_path) &&
								strcmp(items->e[i].title, home) &&
								strcmp(items->e[i].title, "/")) {
							fprintf(f, "%s\n", items->e[i].title);
						}
					}
					fclose(f);
				}
			}
			_mui_stdfile_control_action(std->listbox, std,
					MUI_CONTROL_ACTION_SELECT, elem);
		}	break;
		case MUI_STD_FILE_PART_CANCEL:
			mui_window_action(&std->win, MUI_STDF_ACTION_CANCEL, NULL);
			break;
		case MUI_STD_FILE_PART_HOME:
		//	printf("%s Home\n", __func__);
			_mui_stdfile_populate(std, getenv("HOME"));
			break;
		case MUI_STD_FILE_PART_ROOT:
		//	printf("%s Root\n", __func__);
			_mui_stdfile_populate(std, "/");
			break;
		case MUI_STD_FILE_PART_LISTBOX: {
		//	printf("%s Listbox\n", __func__);
			if (what == MUI_CONTROL_ACTION_SELECT ||
					what == MUI_CONTROL_ACTION_DOUBLECLICK) {
				mui_listbox_elem_t * e = param;
				if (e->disabled)
					break;
				char * full_path = NULL;
				asprintf(&full_path, "%s/%s",
						std->current_path, (char*)e->elem);
				char *dbl;
				while ((dbl = strstr(full_path, "//")) != NULL) {
					memmove(dbl, dbl + 1, strlen(dbl)); // include zero
				}
				struct stat st;
				stat(full_path, &st);
				if (S_ISDIR(st.st_mode)) {
					_mui_stdfile_populate(std, full_path);
				} else {
					printf("Selected: %s\n", full_path);
					mui_window_action(&std->win, MUI_STDF_ACTION_SELECT, NULL);
				}
				free(full_path);
			}
		}	break;
		case MUI_STD_FILE_PART_POPUP:
		//	printf("%s POPUP\n", __func__);
			if (what == MUI_CONTROL_ACTION_VALUE_CHANGED) {
				int idx = mui_control_get_value(c);
				printf("Selected: %s\n", std->pop_path.e[idx]);
				_mui_stdfile_populate(std, std->pop_path.e[idx]);
			}
			break;
		case MUI_STD_FILE_PART_RECENT:
		//	printf("%s POPUP\n", __func__);
			if (what == MUI_CONTROL_ACTION_VALUE_CHANGED) {
				int idx = mui_control_get_value(c);
				mui_menu_item_t * items = mui_popupmenu_get_items(c)->e;
				printf("Recent Selected: %s\n", items[idx].title);
				_mui_stdfile_populate(std, items[idx].title);
			}
			break;
	}
	return 0;
}


mui_window_t *
mui_stdfile_make_window(
		struct mui_t * 	ui,
		c2_pt_t 		where,
		const char * 	prompt,
		const char * 	pattern,
		const char * 	start_path,
		const char * 	save_filename,
		uint16_t 		flags )
{
	float base_size = mui_font_find(ui, "main")->size;
	float margin = base_size * 0.7;
	c2_rect_t wpos = C2_RECT_WH(where.x, where.y, 700, 400);
	if (where.x == 0 && where.y == 0)
		c2_rect_offset(&wpos,
			(ui->screen_size.x / 2) - (c2_rect_width(&wpos) / 2),
			(ui->screen_size.y * 0.4) - (c2_rect_height(&wpos) / 2));
	mui_window_t *w = mui_window_create(ui,
					wpos,
					NULL, MUI_WINDOW_LAYER_MODAL,
					prompt, sizeof(mui_stdfile_t));
	mui_window_set_action(w, _mui_stdfile_window_action, NULL);
	mui_stdfile_t *std = (mui_stdfile_t *)w;
	if (pattern && *pattern && (flags & MUI_STDF_FLAG_REGEXP)) {
#ifdef MUI_HAS_REGEXP
		std->re_pattern = strdup(pattern);
		int re = regcomp(&std->re, std->re_pattern, REG_EXTENDED|REG_ICASE);
		if (re) {
			char * msg = NULL;
			asprintf(&msg, "%s\n%s", std->re_pattern,
						strerror(errno));
			mui_alert(std->win.ui, C2_PT(0,0),
						"Could not compile regexp",
						msg,
						MUI_ALERT_FLAG_OK);
			free(std->re_pattern);
			std->re_pattern = NULL;
		}
#else
		printf("%s: Regexp not supported\n", __func__);
#endif
	} else if (pattern && *pattern) {
		char * dup = strdup(pattern);
		char * w = dup;
		char * suf;
		int di = 0;
		while ((suf = strsep(&w, ",")) != NULL) {
			if (!*suf)
				continue;
			if (di >= MUI_STDF_MAX_SUFFIX) {
				printf("%s Too many suffixes, ignoring: %s\n", __func__, suf);
				break;
			}
			uint32_t hash = mui_hash_nocase(suf);
			snprintf(std->suffix[di].s, sizeof(std->suffix[di].s), "%s", suf);
			std->suffix[di].hash = hash;
			di++;
		}
		free(dup);
	}
	bool save_box = false;

	mui_control_t * c = NULL;
	c2_rect_t cf;
	cf = C2_RECT_WH(0, 0, 120, 40);
	c2_rect_left_of(&cf, c2_rect_width(&w->content), margin);
	c2_rect_top_of(&cf, c2_rect_height(&w->content), margin);
	std->cancel = c = mui_button_new(w,
					cf, MUI_BUTTON_STYLE_NORMAL,
					"Cancel", MUI_STD_FILE_PART_CANCEL);
	c2_rect_top_of(&cf, cf.t, margin);
	std->ok = c = mui_button_new(w,
					cf, MUI_BUTTON_STYLE_DEFAULT,
					save_box ? "Save" : "Select", MUI_STD_FILE_PART_OK);

	std->ok->key_equ = MUI_KEY_EQU(0, 13); // return
	std->cancel->key_equ = MUI_KEY_EQU(0, 27); // ESC

	c2_rect_t t = cf;
	t.b = t.t + 1;
	c2_rect_top_of(&t, cf.t, 25);
	c = mui_separator_new(w, t);

	int button_spacer = save_box ? margin * 0.7 : margin;
	int listbox_height = save_box ? 250 : 300;

	c2_rect_top_of(&cf, cf.t, 40);
	std->home = c = mui_button_new(w,
					cf, MUI_BUTTON_STYLE_NORMAL,
					"Home", MUI_STD_FILE_PART_HOME);
	c->key_equ = MUI_KEY_EQU(MUI_MODIFIER_ALT, 'h');

	c2_rect_top_of(&cf, cf.t, button_spacer);
	std->root = c = mui_button_new(w,
					cf, MUI_BUTTON_STYLE_NORMAL,
					"Root", MUI_STD_FILE_PART_ROOT);
	c->key_equ = MUI_KEY_EQU(MUI_MODIFIER_ALT, '/');

	if (save_box) {
		c2_rect_top_of(&cf, cf.t, button_spacer);
		std->save.create_folder = c = mui_button_new(w,
						cf, MUI_BUTTON_STYLE_NORMAL,
						"New…", MUI_STD_FILE_PART_ROOT);
		c->key_equ = MUI_KEY_EQU(MUI_MODIFIER_ALT, 'n');
		c->uid = MUI_STD_FILE_PART_NEW;
	}
	cf = C2_RECT_WH(margin, 0, c2_rect_width(&wpos)-185, 35);
	c2_rect_top_of(&cf, c2_rect_height(&w->content), margin);
	if (save_box) {
		std->save.save_name = c = mui_textedit_control_new(w,
									cf, MUI_CONTROL_TEXTBOX_FRAME);
		c->uid = MUI_STD_FILE_PART_SAVE_NAME;
		mui_textedit_set_text(c,
			"Fulling Mill Online Return Center.pdf");
		mui_textedit_set_selection(c, 0, 255);
	}
	cf = C2_RECT_WH(margin, 45, c2_rect_width(&wpos)-185, listbox_height);
	std->listbox = c = mui_listbox_new(w, cf,
					MUI_STD_FILE_PART_LISTBOX);

	cf = C2_RECT_WH(margin, 0, c2_rect_width(&wpos)-185, 34);
	c2_rect_top_of(&cf, std->listbox->frame.t, 6);
	std->popup = c = mui_popupmenu_new(w, cf,
					"Popup", MUI_STD_FILE_PART_POPUP,
					MUI_TEXT_ALIGN_CENTER);
	cf.r = c2_rect_width(&w->content) - margin;
	cf.l = cf.r - 34;
	std->recent = c = mui_popupmenu_new(w, cf,
					MUI_GLYPH_POPMARK, MUI_STD_FILE_PART_RECENT,
					MUI_TEXT_ALIGN_RIGHT);
//	mui_control_set_state(c, MUI_CONTROL_STATE_DISABLED);
//	printf("Popup: %p\n", c);
	c = NULL;
	TAILQ_FOREACH(c, &w->controls, self) {
		if (mui_control_get_uid(c))
			mui_control_set_action(c, _mui_stdfile_control_action, std);
	}
	int dopop = 1; // populate to start_path by default
	if (!(flags & MUI_STDF_FLAG_NOPREF) && ui->pref_directory) {
		uint32_t hash = std->re_pattern ? mui_hash(std->re_pattern) : 0;
		asprintf(&std->pref_file, "%s/std_path_%04x", ui->pref_directory, hash);
		printf("%s pref file: %s\n", __func__, std->pref_file);

		dopop = _mui_stdfile_load_pref(std);
	}
	if (dopop)
		_mui_stdfile_populate(std, start_path);

	return w;
}

mui_window_t *
mui_stdfile_get(
		struct mui_t * 	ui,
		c2_pt_t 		where,
		const char * 	prompt,
		const char * 	pattern,
		const char * 	start_path,
		uint16_t 		flags )
{
	mui_window_t *w = mui_stdfile_make_window(ui, where,
						prompt, pattern, start_path, NULL, flags);
	return w;
}

char *
mui_stdfile_get_path(
		mui_window_t * w )
{
	mui_stdfile_t * std = (mui_stdfile_t *)w;
	return std->current_path;
}

char *
mui_stdfile_get_selected_path(
		mui_window_t * w )
{
	mui_stdfile_t * std = (mui_stdfile_t *)w;

	mui_listbox_elem_t * e = mui_listbox_get_elems(std->listbox)->e;
	int idx = mui_control_get_value(std->listbox);
	if (idx < 0 || idx >= (int)mui_listbox_get_elems(std->listbox)->count)
		return NULL;
	mui_listbox_elem_t * elem = &e[idx];
	char * full_path = NULL;
	asprintf(&full_path, "%s/%s",
			std->current_path, (char*)elem->elem);
	char *dbl;
	while ((dbl = strstr(full_path, "//")) != NULL) {
		memmove(dbl, dbl + 1, strlen(dbl)); // include zero
	}
	return full_path;
}
