# Makefile
#
# Copyright (C) 2024 Michel Pollet <buserror@gmail.com>
#
# SPDX-License-Identifier: MIT

TARGET 			:= mii_ui_tests

LIBMUI 			:= ../../
MII 			:= ../../../

all 			: $(TARGET)

include $(LIBMUI)/Makefile.common

vpath %.c $(MII)/ui_gl

CPPFLAGS		+= -I$(MII)/ui_gl

.PHONY			: $(TARGET)
$(TARGET) : $(LIB)/$(TARGET).so

MII_UI_OBJ := $(OBJ)/mii_mui_slots.o \
				$(OBJ)/mii_mui_loadbin.o \
				$(OBJ)/mii_mui_1mb.o \
				$(OBJ)/mii_mui_2dsk.o \
				$(OBJ)/mii_mui_about.o \
				$(OBJ)/mii_mui_ssc.o \
				$(OBJ)/mii_mui_utils.o

$(LIB)/$(TARGET).so : $(OBJ)/$(TARGET).o $(MII_UI_OBJ)
$(LIB)/$(TARGET).so : LDLIBS += $(LIB)/libmui.a

$(OBJ)/mii_mui_about.o : CPPFLAGS+=-DMII_ICON64_DEFINE

clean:
	rm -rf $(LIB)/$(TARGET).so $(MII_UI_OBJ)

-include $(OBJ)/*.d
