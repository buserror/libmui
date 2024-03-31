# Makefile
#
# Copyright (C) 2024 Michel Pollet <buserror@gmail.com>
#
# SPDX-License-Identifier: MIT

# Check to see if we are a submodule of the MII emulator
MII_PLUG 	:= ${wildcard ../../ui_gl/*.c}

ifneq ($(MII_PLUG),)
PLUGS  		+= mii_ui
endif
PLUGS		+= mui_widgets_demo

all :
	for plug in $(PLUGS); do \
		$(MAKE) -C $$plug; \
	done

