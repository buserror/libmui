# Makefile
#
# Copyright (C) 2020 Michel Pollet <buserror@gmail.com>
#
# SPDX-License-Identifier: MIT

CC				= gcc

LIBMUI 			=
MUI_SRC			:= $(wildcard $(LIBMUI)src/*.c)

vpath %.c $(LIBMUI)src $(LIBMUI)mui_shell

# this is just so we compile the tests if not a submodule
IS_SUBMODULE 	:= ${wildcard ../ui_gl/mii_mui.h}

all				: static mui_shell

include $(LIBMUI)Makefile.common

ifeq ($(IS_SUBMODULE),)
all				: tests
endif

MUI_OBJ			:= ${patsubst %, $(OBJ)/%, ${notdir ${MUI_SRC:.c=.o}}}
TARGET_LIB		:= $(LIB)/libmui.a

.PHONY			: mui_shell static tests
mui_shell		: $(BIN)/mui_shell
static			: $(TARGET_LIB)
tests			: | $(TARGET_LIB)
	$(MAKE) -j -C tests
	@echo " ** To launch the demo, run "
	@echo "   $(BIN)/mui_shell -f $(LIB)/mui_widgets_demo.so"

$(TARGET_LIB) : $(MUI_OBJ) | $(LIB)
	@echo "  AR      $@"
	$(Q)$(AR) rcs $@ $^

#
# The shell program is used to test the UI library using plugins
# It is made using XCB and XKB libraries to have a minimal dependency
# on X11. Also, allows partial updates to be tested properly
#
$(OBJ)/mui_shell.o : CPPFLAGS += -DUI_HAS_XCB=1 -DUI_HAS_XKB=1
ifeq ($(shell uname),NetBSD)
# NetBSD requirements
$(OBJ)/mui_shell.o : CPPFLAGS += $(shell pkg-config --cflags xorg-server xkbcommon)
$(BIN)/mui_shell : LDLIBS += $(shell pkg-config --libs xorg-server)
endif

$(BIN)/mui_shell : LDLIBS += $(shell pkg-config --libs \
								xcb xcb-shm xcb-image \
								xkbcommon xkbcommon-x11)
$(BIN)/mui_shell : LDLIBS += -lm
ifeq ($(shell uname),Linux)
$(BIN)/mui_shell : LDLIBS += -ldl
endif
$(BIN)/mui_shell : $(OBJ)/mui_shell.o $(LIB)/libmui.a

clean :
	rm -rf $(O)

# This is for development purpose. This will recompile the project
# everytime a file is modified.
watch			:
	while true; do \
		clear; $(MAKE) -j all tests; \
		inotifywait -qre close_write src mui_shell tests/* ../ui_gl; \
	done

compile_commands.json: lsp
lsp:
	{ $$(which gmake) CC=gcc V=1 --always-make --dry-run all tests ; } | \
		sh ../utils/clangd_gen.sh >compile_commands.json

-include $(OBJ)/*.d
