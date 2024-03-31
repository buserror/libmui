# Makefile
#
# Copyright (C) 2024 Michel Pollet <buserror@gmail.com>
#
# SPDX-License-Identifier: MIT

TARGET 			:= mui_widgets_demo

LIBMUI 			:= ../../

all 			: $(TARGET)

include $(LIBMUI)/Makefile.common

.PHONY			: $(TARGET)
$(TARGET) : $(LIB)/$(TARGET).so

$(LIB)/$(TARGET).so : $(OBJ)/$(TARGET).o
$(LIB)/$(TARGET).so : LDLIBS += $(LIB)/libmui.a

clean:
	rm -rf $(LIB)/$(TARGET).so

-include $(OBJ)/*.d
