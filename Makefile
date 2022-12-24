CURRENT_PATH := $(subst $(lastword $(notdir $(MAKEFILE_LIST))),,$(subst $(space),\$(space),$(shell realpath '$(strip $(MAKEFILE_LIST))')))
EXE := procprog
SRC_DIR := .
OBJ_DIR := ./obj
$(shell mkdir -p $(CURRENT_PATH)$(SRC_DIR) $(CURRENT_PATH)$(OBJ_DIR))

include /usr/share/dpkg/pkg-info.mk
ifeq ($(DEB_VERSION),)
    DEB_VERSION := Unknown
endif

ifeq ($(PREFIX),)
    PREFIX := /usr
endif
HEADER := $(wildcard $(SRC_DIR)/*.h)
SRC := $(wildcard $(SRC_DIR)/*.c)
OBJ := $(SRC:$(SRC_DIR)%.c=$(OBJ_DIR)%.o)
DOT := $(EXE).ltrans0.231t.optimized.dot
LIBS := -lrt -lpthread
WARNINGS := -Wall -Wextra -Wduplicated-cond -Wduplicated-branches -Wlogical-op -Wrestrict -Wshadow -Wformat=2
CFLAGS := $(WARNINGS) -std=gnu99 -fpie -O2 -flto -gdwarf-4 -g3 -D_FORTIFY_SOURCE=2 -DVERSION=\"$(DEB_VERSION)\" 
LDFLAGS := -pie -Wl,-z,relro,-z,now

GCC_10 := $(shell expr `cc -dumpversion | cut -f1 -d.` \>= 10)
ifeq ($(GCC_10),1)
	CFLAGS += -fanalyzer
endif

TIDY_CHECKS := clang-analyzer-*,performance-*,portability-*,misc-*,cert-*
TIDY_IGNORE := -clang-analyzer-valist.Uninitialized,-cert-err34-c,-cert-err33-c
CPPCHECK_IGNORE := --inline-suppr -i ./time --suppress=variableScope --suppress=missingIncludeSystem --suppress=localtimeCalled
CPPCHECK_CHECKS := --max-ctu-depth=4 --inconclusive --enable=all --platform=unix64 --std=c99 --library=posix

.PHONY: clean all install uninstall iwyu tidy format cppcheck checks debug


all: $(EXE)
debug: CFLAGS += -Og
debug: $(EXE)
checks: check-format cppcheck
manual: $(EXE).1
graph: CFLAGS += -fdump-tree-optimized-graph
graph: clean all
graph: $(EXE).graph.svg

$(EXE): $(OBJ)
	$(CC) $^ $(LIBS) $(CFLAGS) $(LDFLAGS) -o $(EXE)
	$(info Executable compiled to $(shell realpath --relative-to=$(shell pwd) $@))

$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	@rm -f ./$(OBJ_DIR)/* ./$(EXE) ./$(EXE).graph.svg $(EXE).1 $(wildcard $(SRC_DIR)/*.dot) $(wildcard $(SRC_DIR)/*.optimized)

install: $(EXE)
	@mkdir -p $(DESTDIR)$(PREFIX)/bin
	@install $< $(DESTDIR)$(PREFIX)/bin/$(EXE)
	$(info Executable installed to $(DESTDIR)$(PREFIX)/bin/$(EXE))

uninstall:
	@rm -f $(DESTDIR)$(PREFIX)/bin/$(EXE)
	$(info Executable deleted from $(DESTDIR)$(PREFIX)/bin/$(EXE))


iwyu: $(SRC:%.c=%.iwyu)
%.iwyu: $(SRC_DIR)/%.c
	@echo -n "\ninclude-what-you-use: $<"
	@include-what-you-use $(CFLAGS) -Wno-everything -c $< || true

tidy: $(SRC) $(HEADER)
	$(info clang-tidy: $^)
	@clang-tidy --format-style=file -checks=$(TIDY_CHECKS),$(TIDY_IGNORE) $^ -- $(subst -fanalyzer,,$(CFLAGS))

check-format: $(SRC) $(HEADER)
	$(info clang-format: $^)
	@clang-format --dry-run -Werror --style=file $^

format: $(SRC) $(HEADER)
	$(info clang-format: $^)
	@clang-format -i --style=file $^

cppcheck: $(SRC) $(HEADER)
	$(info cppcheck: $^)
	@cppcheck --force --quiet $(CPPCHECK_CHECKS) $(CPPCHECK_IGNORE) $(SRC_DIR)

%.graph.svg: $(DOT)
	dot -Tsvg $< -o $@

$(EXE).1: $(EXE)
	$(info help2man: $@)
	@help2man --name="Monitor program output and system usage in a single terminal" --output=$@ ./$(EXE)

deb:
	uscan --verbose --force-download
	debuild -i -us -uc -Zlzma -z9 --lintian-opts -iEvI --pedantic --color=always
	rm -f ../procprog_*.orig.tar.gz

ppa-release:
	@echo "Running uscan"
	@uscan --verbose --force-download
	@echo "Building source package"
	@debuild -S -sa --lintian-opts -iEvI --pedantic --color=always
	@echo "Check the build and upload with dput frosticles-ppa ../procprog_*_source.changes"
