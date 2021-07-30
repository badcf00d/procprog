CC = clang
EXE = procprog
SRC_DIR = ./
OBJ_DIR = ./obj

ifeq ($(OS),Windows_NT)
	LIBS = -lm -lrt -lpthread
else
	UNAME_S := $(shell uname -s)
	ifeq ($(UNAME_S),Linux)
		LIBS = -lm -lrt -lpthread
	endif
	ifeq ($(UNAME_S),Darwin)
		LIBS = -lm -lpthread
	endif
endif

SRC = $(wildcard $(SRC_DIR)/*.c)
OBJ = $(SRC:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)
CFLAGS = -Wall -Wextra -fverbose-asm

ifeq ($(PREFIX),)
    PREFIX := /usr/local
endif


.PHONY: clean all install uninstall


all: $(EXE)

$(EXE): $(OBJ)
	@$(CC) $^ $(LIBS) $(CFLAGS) -o $(EXE)
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
