#---------------------------------------------------------------------------------
# Badge Arcade Reclawed installer (3DS homebrew, libctru)
#
#   make        -> reclawed-installer.3dsx  (run from the Homebrew Launcher)
#   make cia    -> reclawed-installer.cia   (install with FBI)
#   make test   -> PC unit tests (tests/)
#   make compile_commands  -> clean build + build/compile_commands.json
#   make clean
#
# The patches must be built first (tools/build_ips.py); they are bundled from
# PATCH_BUILD (default ../build).
# Needs devkitARM + libctru, and makerom + bannertool on the PATH for `make cia`.
# Based on the devkitPro 3ds template; the project-specific parts are marked
# with "Reclawed:".
#---------------------------------------------------------------------------------
.SUFFIXES:
#---------------------------------------------------------------------------------

ifeq ($(strip $(DEVKITARM)),)
$(error "Please set DEVKITARM in your environment. export DEVKITARM=<path to>devkitARM")
endif

TOPDIR ?= $(CURDIR)
include $(DEVKITARM)/3ds_rules

#---------------------------------------------------------------------------------
# TARGET is the name of the output
# BUILD is the directory where object files & intermediate files will be placed
# SOURCES is a list of directories containing source code
# DATA is a list of directories containing data files
# INCLUDES is a list of directories containing header files
# GRAPHICS is a list of directories containing graphics files
# GFXBUILD is the directory where converted graphics files will be placed
#   If set to $(BUILD), it will statically link in the converted
#   files as if they were data files.
#
# NO_SMDH: if set to anything, no SMDH file is generated.
# ROMFS is the directory which contains the RomFS, relative to the Makefile (Optional)
# APP_TITLE is the name of the app stored in the SMDH file (Optional)
# APP_DESCRIPTION is the description of the app stored in the SMDH file (Optional)
# APP_AUTHOR is the author of the app stored in the SMDH file (Optional)
# ICON is the filename of the icon (.png), relative to the project folder.
#   If not set, it attempts to use one of the following (in this order):
#     - <Project name>.png
#     - icon.png
#     - <libctru folder>/default_icon.png
#---------------------------------------------------------------------------------
TARGET		:=	reclawed-installer
BUILD		:=	build
SOURCES		:=	source
DATA		:=	data
INCLUDES	:=	include
GRAPHICS	:=	gfx
GFXBUILD	:=	$(BUILD)

# Reclawed: app identity shown in the HOME Menu and stored in the CIA.
APP_TITLE	:=	Reclawed Installer
APP_DESCRIPTION	:=	Badge Arcade Reclawed patch installer
APP_AUTHOR	:=	Badge Arcade Reclawed
# Product code and unique ID must not clash with other installed titles.
# 0xBADA0 is in the homebrew range (0x00300-0xF7FFF): title ID 000400000BADA000.
APP_PRODUCT_CODE	:=	CTR-H-RCLW
APP_UNIQUE_ID	:=	0xBADA0

# Reclawed: app version, shown on screen and written into the CIA.
# Note: makerom 0.19 writes major and micro into the CIA title version but
# always leaves minor at 0, so FBI shows 0.1.0 as "0.0.0". The on-screen
# version is the reliable one.
VERSION_MAJOR	:=	0
VERSION_MINOR	:=	3
VERSION_MICRO	:=	0
VERSION		:=	$(VERSION_MAJOR).$(VERSION_MINOR).$(VERSION_MICRO)

# Reclawed: `make clean && make FAKE_GAME=1` pretends Badge Arcade EUR v1.3.1 is
# installed (see source/probe.c), to try the install flow in an emulator.
# The version gets a "-fake" suffix so such a build is never mistaken for a real one.
ifneq ($(FAKE_GAME),)
VERSION		:=	$(VERSION)-fake
FAKE_DEFINE	:=	-DRECLAWED_FAKE_GAME
endif

META		:=	meta
ICON		:=	meta/icon.png
# Reclawed: the RomFS is assembled at build time from the generated patches,
# so it lives under build/ and is never committed.
ROMFS		:=	$(BUILD)/romfs
# Where tools/build_ips.py (or .github/scripts/build_all.sh) wrote the patches:
# <PATCH_BUILD>/<REGION>/luma/titles/<title id>/code.ips
PATCH_BUILD	?=	../build

#---------------------------------------------------------------------------------
# options for code generation
#---------------------------------------------------------------------------------
ARCH	:=	-march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft

CFLAGS	:=	-g -Wall -Wextra -O2 -mword-relocations \
			-ffunction-sections \
			$(ARCH)

CFLAGS	+=	$(INCLUDE) -D__3DS__ -DAPP_VERSION=\"$(VERSION)\" $(FAKE_DEFINE)

CXXFLAGS	:= $(CFLAGS) -fno-rtti -fno-exceptions -std=gnu++11

ASFLAGS	:=	-g $(ARCH)
LDFLAGS	=	-specs=3dsx.specs -g $(ARCH) -Wl,-Map,$(notdir $*.map)

LIBS	:= -lctru -lm

#---------------------------------------------------------------------------------
# list of directories containing libraries, this must be the top level containing
# include and lib
#---------------------------------------------------------------------------------
LIBDIRS	:= $(CTRULIB)


#---------------------------------------------------------------------------------
# no real need to edit anything past this point unless you need to add additional
# rules for different file extensions
#---------------------------------------------------------------------------------
ifneq ($(BUILD),$(notdir $(CURDIR)))
#---------------------------------------------------------------------------------

export OUTPUT	:=	$(CURDIR)/$(TARGET)
export TOPDIR	:=	$(CURDIR)

export VPATH	:=	$(foreach dir,$(SOURCES),$(CURDIR)/$(dir)) \
			$(foreach dir,$(GRAPHICS),$(CURDIR)/$(dir)) \
			$(foreach dir,$(DATA),$(CURDIR)/$(dir))

export DEPSDIR	:=	$(CURDIR)/$(BUILD)

CFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
CPPFILES	:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp)))
SFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.s)))
PICAFILES	:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.v.pica)))
SHLISTFILES	:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.shlist)))
GFXFILES	:=	$(foreach dir,$(GRAPHICS),$(notdir $(wildcard $(dir)/*.t3s)))
BINFILES	:=	$(foreach dir,$(DATA),$(notdir $(wildcard $(dir)/*.*)))

#---------------------------------------------------------------------------------
# use CXX for linking C++ projects, CC for standard C
#---------------------------------------------------------------------------------
ifeq ($(strip $(CPPFILES)),)
#---------------------------------------------------------------------------------
	export LD	:=	$(CC)
#---------------------------------------------------------------------------------
else
#---------------------------------------------------------------------------------
	export LD	:=	$(CXX)
#---------------------------------------------------------------------------------
endif
#---------------------------------------------------------------------------------

#---------------------------------------------------------------------------------
ifeq ($(GFXBUILD),$(BUILD))
#---------------------------------------------------------------------------------
export T3XFILES :=  $(GFXFILES:.t3s=.t3x)
#---------------------------------------------------------------------------------
else
#---------------------------------------------------------------------------------
export ROMFS_T3XFILES	:=	$(patsubst %.t3s, $(GFXBUILD)/%.t3x, $(GFXFILES))
export T3XHFILES		:=	$(patsubst %.t3s, $(BUILD)/%.h, $(GFXFILES))
#---------------------------------------------------------------------------------
endif
#---------------------------------------------------------------------------------

export OFILES_SOURCES 	:=	$(CPPFILES:.cpp=.o) $(CFILES:.c=.o) $(SFILES:.s=.o)

export OFILES_BIN	:=	$(addsuffix .o,$(BINFILES)) \
			$(PICAFILES:.v.pica=.shbin.o) $(SHLISTFILES:.shlist=.shbin.o) \
			$(addsuffix .o,$(T3XFILES))

export OFILES := $(OFILES_BIN) $(OFILES_SOURCES)

export HFILES	:=	$(PICAFILES:.v.pica=_shbin.h) $(SHLISTFILES:.shlist=_shbin.h) \
			$(addsuffix .h,$(subst .,_,$(BINFILES))) \
			$(GFXFILES:.t3s=.h)

export INCLUDE	:=	$(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) \
			$(foreach dir,$(LIBDIRS),-I$(dir)/include) \
			-I$(CURDIR)/$(BUILD)

export LIBPATHS	:=	$(foreach dir,$(LIBDIRS),-L$(dir)/lib)

export _3DSXDEPS	:=	$(if $(NO_SMDH),,$(OUTPUT).smdh)

ifeq ($(strip $(ICON)),)
	icons := $(wildcard *.png)
	ifneq (,$(findstring $(TARGET).png,$(icons)))
		export APP_ICON := $(TOPDIR)/$(TARGET).png
	else
		ifneq (,$(findstring icon.png,$(icons)))
			export APP_ICON := $(TOPDIR)/icon.png
		endif
	endif
else
	export APP_ICON := $(TOPDIR)/$(ICON)
endif

ifeq ($(strip $(NO_SMDH)),)
	export _3DSXFLAGS += --smdh=$(CURDIR)/$(TARGET).smdh
endif

ifneq ($(ROMFS),)
	export _3DSXFLAGS += --romfs=$(CURDIR)/$(ROMFS)
endif

.PHONY: all clean cia romfs test compile_commands

#---------------------------------------------------------------------------------
all: $(BUILD) $(GFXBUILD) $(DEPSDIR) romfs $(ROMFS_T3XFILES) $(T3XHFILES)
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

$(BUILD):
	@mkdir -p $@

# Reclawed: bundle every built patch as romfs:/luma/titles/<title id>/code.ips,
# the same layout it gets on the SD card. Rebuilt on every `make` so the app
# always carries the latest patches; the .3dsx is removed so it gets repacked
# with the new RomFS even when no source changed.
romfs: | $(BUILD)
	@rm -rf $(ROMFS) $(TARGET).3dsx && mkdir -p $(ROMFS)/luma/titles
	@found=0; \
	for f in $(PATCH_BUILD)/*/luma/titles/*/code.ips; do \
		[ -f "$$f" ] || continue; \
		tid=$$(basename $$(dirname "$$f")); \
		mkdir -p $(ROMFS)/luma/titles/$$tid && cp "$$f" $(ROMFS)/luma/titles/$$tid/ || exit 1; \
		echo "bundling $$f"; found=1; \
	done; \
	[ $$found = 1 ] || { echo "No patches in $(PATCH_BUILD)/: run tools/build_ips.py first"; exit 1; }

ifneq ($(GFXBUILD),$(BUILD))
$(GFXBUILD):
	@mkdir -p $@
endif

ifneq ($(DEPSDIR),$(BUILD))
$(DEPSDIR):
	@mkdir -p $@
endif

#---------------------------------------------------------------------------------
# CIA packaging (makerom + bannertool from tools/bin)
#---------------------------------------------------------------------------------
cia: all
	@bannertool makebanner -i $(META)/banner.png -a $(META)/banner.wav -o $(BUILD)/banner.bnr
	@bannertool makesmdh -s "$(APP_TITLE)" -l "$(APP_DESCRIPTION)" -p "$(APP_AUTHOR)" -i $(META)/icon.png -o $(BUILD)/icon.icn
	@makerom -f cia -o $(TARGET).cia -target t -exefslogo -elf $(TARGET).elf -rsf $(META)/app.rsf \
		-banner $(BUILD)/banner.bnr -icon $(BUILD)/icon.icn \
		-DAPP_TITLE="$(APP_TITLE)" -DAPP_PRODUCT_CODE="$(APP_PRODUCT_CODE)" -DAPP_UNIQUE_ID="$(APP_UNIQUE_ID)" \
		-DAPP_ROMFS="$(ROMFS)" -DAPP_SYSTEM_MODE=64MB -DAPP_SYSTEM_MODE_EXT=Legacy \
		-DAPP_CATEGORY=Application -DAPP_USE_ON_SD=true -DAPP_MEMORY_TYPE=Application -DAPP_CPU_SPEED=268MHz -DAPP_ENABLE_L2_CACHE=false \
		-DAPP_ENCRYPTED=false -DAPP_VERSION_MAJOR=$(VERSION_MAJOR) \
		-major $(VERSION_MAJOR) -minor $(VERSION_MINOR) -micro $(VERSION_MICRO)
	@echo built ... $(TARGET).cia

#---------------------------------------------------------------------------------
# Reclawed: PC unit tests (see tests/Makefile; pass HOSTCC=... to pick the compiler).
test:
	@$(MAKE) --no-print-directory -C tests

# Reclawed: full rebuild that also writes build/compile_commands.json for
# clangd and other editors.
compile_commands: clean
	@$(MAKE) --no-print-directory ADD_COMPILE_COMMAND=generate_compile_commands

#---------------------------------------------------------------------------------
clean:
	@echo clean ...
	@rm -fr $(BUILD) $(TARGET).3dsx $(OUTPUT).smdh $(TARGET).elf $(TARGET).cia $(GFXBUILD)

#---------------------------------------------------------------------------------
$(GFXBUILD)/%.t3x	$(BUILD)/%.h	:	%.t3s
#---------------------------------------------------------------------------------
	@echo $(notdir $<)
	@tex3ds -i $< -H $(BUILD)/$*.h -d $(DEPSDIR)/$*.d -o $(GFXBUILD)/$*.t3x

#---------------------------------------------------------------------------------
else

#---------------------------------------------------------------------------------
# main targets
#---------------------------------------------------------------------------------
$(OUTPUT).3dsx	:	$(OUTPUT).elf $(_3DSXDEPS)

$(OFILES_SOURCES) : $(HFILES)

$(OUTPUT).elf	:	$(OFILES)

#---------------------------------------------------------------------------------
# you need a rule like this for each extension you use as binary data
#---------------------------------------------------------------------------------
%.bin.o	%_bin.h :	%.bin
#---------------------------------------------------------------------------------
	@echo $(notdir $<)
	@$(bin2o)

#---------------------------------------------------------------------------------
.PRECIOUS	:	%.t3x %.shbin
#---------------------------------------------------------------------------------
%.t3x.o	%_t3x.h :	%.t3x
#---------------------------------------------------------------------------------
	$(SILENTMSG) $(notdir $<)
	$(bin2o)

#---------------------------------------------------------------------------------
%.shbin.o %_shbin.h : %.shbin
#---------------------------------------------------------------------------------
	$(SILENTMSG) $(notdir $<)
	$(bin2o)

-include $(DEPSDIR)/*.d

#---------------------------------------------------------------------------------------
endif
#---------------------------------------------------------------------------------------
