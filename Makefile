CC = gcc
EXE = procprog
SRC_DIR = ./
OBJ_DIR = ./obj
INC_DIRS = -Iinclude

SRC = $(wildcard $(SRC_DIR)/*.c)
OBJ = $(SRC:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)
LIBS = -lm -lrt
CFLAGS = -Wall -Wvla -fverbose-asm

ifeq ($(PREFIX),)
    PREFIX := /usr/local
endif


.PHONY: clean all install uninstall


all: $(EXE)

$(EXE): $(OBJ)
	@$(CC) $^ $(INC_DIRS) $(LIBS) $(CFLAGS) -o $(EXE)
	$(info Executable compiled to $(shell realpath --relative-to=$(shell pwd) $@))

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@$(CC) $(INC_DIRS) $(CFLAGS) -c $< -o $@
	$(info $(CC): $(notdir $<))

clean:
	@rm -f ./$(OBJ_DIR)/* ./$(EXE)

install: $(EXE)
	@mkdir -p $(DESTDIR)$(PREFIX)/bin
	@cp $< $(DESTDIR)$(PREFIX)/bin/$(EXE)
	$(info Executable installed to $(DESTDIR)$(PREFIX)/bin/$(EXE))

uninstall: 
	@rm -f $(DESTDIR)$(PREFIX)/bin/$(EXE)
	$(info Executable deleted from $(DESTDIR)$(PREFIX)/bin/$(EXE))
