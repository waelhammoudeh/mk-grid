# this "makefile" is for xrds2gps (cross roads to GPS) program. It is to be
# placed in the root directory along with src and myinclude directories.
# xrds2gps/
#   |--src/
#   |--myinclude/
#   |--makefile
# 
# This program requires "libcurl" to be installed in the system.
# Run make from the root directory, it will build "xrds2gps" executable there.

SRC_DIR := source
OBJ_DIR := obj
DST_DIR := .
EXEC := $(DST_DIR)/mk-grid

SRC := $(wildcard $(SRC_DIR)/*.c)
OBJ := $(SRC:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)

CPPFLAGS := -Imyinclude -MMD -MP
CFLAGS := -Wall -g
LDLIBS := -lcurl

.PHONY: all clean

all : $(EXEC)

$(EXEC) : $(OBJ)
	$(CC) $^ $(LDLIBS) -o $@

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c | $(OBJ_DIR)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(OBJ_DIR) :
	mkdir -p $@
	
clean:
	@$(RM) -rv $(OBJ_DIR) $(EXEC)

-include $(OBJ:.o=.d)
