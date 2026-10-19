#---------------------------------------------------------------------------------
# PC-side unit tests for the installer's portable code (no 3DS needed).
#
#   make -C installer/tests                  # with the system C compiler (cc)
#   make -C installer/tests HOSTCC=clang
#   make -C installer test                   # same, from the app Makefile
#
# HOSTCC, not CC: devkitPro's rules export CC as the ARM cross compiler.
# Only sources that use plain C + stdio are tested here; luma_system.c,
# main.c and probe.c need libctru and run on the console only.
#---------------------------------------------------------------------------------
HOSTCC	?=	cc
CFLAGS	:=	-std=c99 -g -O1 -Wall -Wextra -Werror -I../source
OUT	:=	../build/tests
EXE	:=	$(OUT)/run_tests

ifneq ($(OS)$(filter MINGW% MSYS% CYGWIN%,$(shell uname -s)),)
EXE	:=	$(EXE).exe
# MSVC's C library marks fopen() and friends as deprecated.
CFLAGS	+=	-D_CRT_SECURE_NO_WARNINGS
endif

SOURCES	:=	../source/luma_config.c ../source/fileops.c ../source/ips.c ../source/installer.c \
		../source/titles.c ../source/checks.c ../source/menu.c
TESTS	:=	run_tests.c test_luma_config.c test_ips.c test_fileops.c test_installer.c \
		test_checks.c test_menu.c

.PHONY: test clean

test: $(EXE)
	@rm -rf $(OUT)/tmp && mkdir -p $(OUT)/tmp
	$(EXE) $(OUT)/tmp/

$(EXE): $(SOURCES) $(TESTS) $(wildcard ../source/*.h) test.h
	@mkdir -p $(OUT)
	$(HOSTCC) $(CFLAGS) $(SOURCES) $(TESTS) -o $@

clean:
	rm -rf $(OUT)
