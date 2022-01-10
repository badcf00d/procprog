CURRENT_PATH := $(subst $(lastword $(notdir $(MAKEFILE_LIST))),,$(subst $(space),\$(space),$(shell realpath '$(strip $(MAKEFILE_LIST))')))
CC_S := $(shell which clang)
ifeq ($(CC_S),)
	CC := gcc
else
	CC := clang
endif
EXE := procprog
SRC_DIR := ./
OBJ_DIR := ./obj
$(shell mkdir -p $(CURRENT_PATH)$(SRC_DIR) $(CURRENT_PATH)$(OBJ_DIR))

ifeq ($(OS),Windows_NT)
	LIBS := -lrt -lpthread
else
	UNAME_S := $(shell uname -s)
	ifeq ($(UNAME_S),Linux)
		LIBS := -lrt -lpthread
	endif
	ifeq ($(UNAME_S),Darwin)
		LIBS := -lrt -lpthread
	endif
endif
ifeq ($(PREFIX),)
    PREFIX := /usr
endif
HEADER := $(subst //,/,$(wildcard $(SRC_DIR)/*.h))
SRC := $(subst //,/,$(wildcard $(SRC_DIR)/*.c))
OBJ := $(SRC:$(SRC_DIR)%.c=$(OBJ_DIR)/%.o)
DOT := $(EXE).ltrans0.231t.optimized.dot
CFLAGS := -Wall -Wextra -std=gnu99 -fpie -D_FORTIFY_SOURCE=2 -g3
LDFLAGS := $(CFLAGS) -pie

TIDY_CHECKS := clang-analyzer-*,performance-*,portability-*,misc-*,cert-*
TIDY_IGNORE := -clang-analyzer-valist.Uninitialized,-cert-err34-c
CPPCHECK_IGNORE := --inline-suppr -U__APPLE__ -i ./time --suppress=variableScope --suppress=missingIncludeSystem --suppress=localtimeCalled
CPPCHECK_CHECKS := --max-ctu-depth=4 --inconclusive --enable=all --platform=unix64 --std=c99 --library=posix
IWYU_FLAGS := -Xiwyu --no_fwd_decls

.PHONY: clean all install uninstall iwyu tidy format cppcheck checks debug


all: CFLAGS += -flto
all: LDFLAGS += -flto
all: $(EXE)

$(EXE): $(OBJ)
	$(CC) $^ $(LIBS) $(LDFLAGS) -o $(EXE)
	@sync
	$(info Executable compiled to $(shell realpath --relative-to=$(shell pwd) $@))

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@$(CC) $(CFLAGS) -c $< -o $@
	$(info $(CC): $(CFLAGS) $(notdir $<))


debug: CFLAGS += -Og
debug: LDFLAGS += -Og
debug: $(EXE)

clean:
	@rm -f ./$(OBJ_DIR)/* ./$(EXE) ./$(EXE).graph.svg $(EXE).1 $(wildcard $(SRC_DIR)/*.dot) $(wildcard $(SRC_DIR)/*.optimized)

install: $(EXE)
	@mkdir -p $(DESTDIR)$(PREFIX)/bin
	@install $< $(DESTDIR)$(PREFIX)/bin/$(EXE)
	$(info Executable installed to $(DESTDIR)$(PREFIX)/bin/$(EXE))

uninstall:
	@rm -f $(DESTDIR)$(PREFIX)/bin/$(EXE)
	$(info Executable deleted from $(DESTDIR)$(PREFIX)/bin/$(EXE))


checks: check-format tidy cppcheck iwyu

iwyu: $(SRC:%.c=%.iwyu)
%.iwyu: $(SRC_DIR)/%.c
	@echo -n "\ninclude-what-you-use: $<"
	@include-what-you-use $(IWYU_FLAGS) $(CFLAGS) -c $< || true

tidy: $(SRC)
	$(info clang-tidy: $^)
	@clang-tidy --format-style=file -checks=$(TIDY_CHECKS),$(TIDY_IGNORE) $^ -- $(CFLAGS)

check-format: $(SRC) $(HEADER)
	$(info clang-format: $^)
	@clang-format --dry-run -Werror --style=file $^

format: $(SRC) $(HEADER)
	$(info clang-format: $^)
	@clang-format -i --style=file $^

cppcheck: $(SRC) $(HEADER)
	$(info cppcheck: $^)
	@cppcheck --force --quiet $(CPPCHECK_CHECKS) $(CPPCHECK_IGNORE) $(SRC_DIR)

graph: CC := gcc
graph: CFLAGS += -fdump-tree-optimized-graph
graph: clean all
graph: $(EXE).graph.svg

%.graph.svg: $(DOT)
	dot -Tsvg $< -o $@


manual: $(EXE).1

$(EXE).1: $(EXE)
	$(info help2man: $@)
	@help2man --name="Runs programs with a quieter output, and shows system usage data" --output=$@ ./$(EXE)

release:
	@echo "Running uscan"
	@uscan --verbose --force-download
	@echo "Running signed build"
	@debuild -S -sa
	@echo "Check the build and upload with dput ppa:frosticles/procprog ../build-area/<source.changes>"
