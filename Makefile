CC := clang
EXE := procprog
SRC_DIR := ./
OBJ_DIR := ./obj

ifeq ($(OS),Windows_NT)
	LIBS := -lm -lrt -lpthread
else
	UNAME_S := $(shell uname -s)
	ifeq ($(UNAME_S),Linux)
		LIBS := -lm -lrt -lpthread
	endif
	ifeq ($(UNAME_S),Darwin)
		LIBS := -lm -lpthread
	endif
endif

SRC := $(wildcard $(SRC_DIR)/*.c)
OBJ := $(SRC:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)
CFLAGS := -Wall -Wextra -fverbose-asm -flto -std=c99

ifeq ($(PREFIX),)
    PREFIX := /usr/local
endif

CPPCHECK_IGNORE := --inline-suppr -U__APPLE__ -i ./time --suppress=variableScope --suppress=missingIncludeSystem
CPPCHECK_CHECKS := --max-ctu-depth=4 --inconclusive --enable=all --platform=unix64 --std=c99 --library=posix

.PHONY: clean all install uninstall iwyu tidy format cppcheck checks


all: $(EXE)

$(EXE): $(OBJ)
	@$(CC) $^ $(LIBS) $(CFLAGS) -o $(EXE)
	$(info Executable compiled to $(shell realpath --relative-to=$(shell pwd) $@))

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@$(CC) $(IWYU_FLAGS) $(CFLAGS) -c $< -o $@
	$(info $(CC): $(notdir $<))

clean:
	@rm -f ./$(OBJ_DIR)/* ./$(EXE)

install: $(EXE)
	@mkdir -p $(DESTDIR)$(PREFIX)/bin
	@install $< $(DESTDIR)$(PREFIX)/bin/$(EXE)
	$(info Executable installed to $(DESTDIR)$(PREFIX)/bin/$(EXE))

uninstall:
	@rm -f $(DESTDIR)$(PREFIX)/bin/$(EXE)
	$(info Executable deleted from $(DESTDIR)$(PREFIX)/bin/$(EXE))


checks:
	@make -k iwyu format tidy

iwyu: CC := include-what-you-use
iwyu: IWYU_FLAGS := -Xiwyu --no_fwd_decls
iwyu: clean
iwyu: $(OBJ)

tidy: $(SRC)
	@clang-tidy --format-style=file $^ -- $(CFLAGS)

format: $(SRC)
	@clang-format -i --style=file --verbose $^
cppcheck: $(SRC) $(HEADER)
	$(info cppcheck: $^)
	@cppcheck --force --quiet $(CPPCHECK_CHECKS) $(CPPCHECK_IGNORE) $(SRC_DIR)
