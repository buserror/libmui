/*
 * mui_shell_plugin.h
 *
 * Copyright (C) 2023 Michel Pollet <buserror@gmail.com>
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>

/*
 * Quck and dirty plugin interface for mui_shell, just allows a quick reload
 * of a plugin without having to restart the whole shell.
 * In combination with 'make watch' you have have 1s turnaround time for
 * your plugin development, helps a ton when tweaking a dialog etc.
 */
struct mui_t;
struct mui_drawable_t;

typedef struct mui_plug_t {
	const char * 		name;		// 'Human' name of the plugin
	const uint32_t * 	icon;		// optional
	// return a 'handle' to some data for this plugin.
	void * (*init)(
					struct mui_t * 			ui,
					struct mui_plug_t * 	plug,
					struct mui_drawable_t * dr );
	void (*dispose)(
					void * plug );
	int (*draw)(
					struct mui_t *			ui,
					void * 					plug,
					struct mui_drawable_t * dr,
					uint16_t 				all );
	int (*event)(
					struct mui_t *			ui,
					void * 					plug,
					struct mui_event_t *	event );
} mui_plug_t;
