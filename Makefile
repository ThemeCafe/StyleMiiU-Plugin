#-------------------------------------------------------------------------------
.SUFFIXES:
#-------------------------------------------------------------------------------

ifeq ($(strip $(DEVKITPRO)),)
$(error "Please set DEVKITPRO in your environment. export DEVKITPRO=<path to>/devkitpro")
endif

TOPDIR ?= $(CURDIR)

LIBCONTENTREDIRECTION_DIR := $(TOPDIR)/external/libcontentredirection
LIBNOTIFICATIONS_DIR := $(TOPDIR)/external/libnotifications
WUPS_DIR := $(TOPDIR)/external/WiiUPluginSystem
WUPS_ROOT := $(WUPS_DIR)
CAFE_GLYPHS_DIR := $(TOPDIR)/external/cafe_glyphs

include $(WUPS_DIR)/share/wups_rules

WUT_ROOT := $(DEVKITPRO)/wut
WUMS_ROOT := $(DEVKITPRO)/wums
#-------------------------------------------------------------------------------
# TARGET is the name of the output
# BUILD is the directory where object files & intermediate files will be placed
# SOURCES is a list of directories containing source code
# DATA is a list of directories containing data files
# INCLUDES is a list of directories containing header files
#-------------------------------------------------------------------------------
TARGET		:=	stylemiiu
BUILD		:=	build
SOURCES		:=	src \
			src/utils \
			src/wupsext
DATA		:=	data
INCLUDES	:=	src

#-------------------------------------------------------------------------------
# options for code generation
#-------------------------------------------------------------------------------

CXX += -std=c++23

CPPFLAGS = 	$(INCLUDE) \
		-D__WIIU__ \
		-D__WUT__ \
		-D__WUPS__

ifneq ($(DEBUG),)
CPPFLAGS += -DDEBUG=1
ifeq ($(DEBUG),VERBOSE)
CPPFLAGS += -DVERBOSE_DEBUG=1
endif
endif

CFLAGS	:=	-g \
		-Wall -Wextra \
		-Os \
		-ffunction-sections \
		-fdata-sections \
		$(MACHDEP)

CXXFLAGS	:= $(CFLAGS)

ASFLAGS	:=	-g $(ARCH)
LDFLAGS	=	-g $(ARCH) $(RPXSPECS) -Wl,-Map,$(notdir $*.map) $(WUPSSPECS) 

LIBS	:= -lwups -lwut -lcontentredirection -lnotifications

#-------------------------------------------------------------------------------
# list of directories containing libraries, this must be the top level
# containing include and lib
#-------------------------------------------------------------------------------
LIBDIRS	:= \
	$(PORTLIBS) \
	$(WUPS_ROOT) \
	$(LIBCONTENTREDIRECTION_DIR) \
	$(LIBNOTIFICATIONS_DIR) \
	$(CAFE_GLYPHS_DIR) \
	$(WUT_ROOT)

#-------------------------------------------------------------------------------
# no real need to edit anything past this point unless you need to add additional
# rules for different file extensions
#-------------------------------------------------------------------------------
ifneq ($(BUILD),$(notdir $(CURDIR)))
#-------------------------------------------------------------------------------

export OUTPUT	:=	$(CURDIR)/$(TARGET)
export TOPDIR	:=	$(CURDIR)

export VPATH	:=	$(foreach dir,$(SOURCES),$(CURDIR)/$(dir)) \
			$(foreach dir,$(DATA),$(CURDIR)/$(dir))

export DEPSDIR	:=	$(CURDIR)/$(BUILD)

CFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
CPPFILES	:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp)))
SFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.s)))
BINFILES	:=	$(foreach dir,$(DATA),$(notdir $(wildcard $(dir)/*.*)))

#-------------------------------------------------------------------------------
# use CXX for linking C++ projects, CC for standard C
#-------------------------------------------------------------------------------
ifeq ($(strip $(CPPFILES)),)
#-------------------------------------------------------------------------------
	export LD	:=	$(CC)
#-------------------------------------------------------------------------------
else
#-------------------------------------------------------------------------------
	export LD	:=	$(CXX)
#-------------------------------------------------------------------------------
endif
#-------------------------------------------------------------------------------

export OFILES_BIN	:=	$(addsuffix .o,$(BINFILES))
export OFILES_SRC	:=	$(CPPFILES:.cpp=.o) $(CFILES:.c=.o) $(SFILES:.s=.o)
export OFILES 	:=	$(OFILES_BIN) $(OFILES_SRC)
export HFILES_BIN	:=	$(addsuffix .h,$(subst .,_,$(BINFILES)))

export INCLUDE	:=	$(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) \
			$(foreach dir,$(LIBDIRS),-I$(dir)/include) \
			-I$(CURDIR)/$(BUILD)

export LIBPATHS	:=	$(foreach dir,$(LIBDIRS),-L$(dir)/lib)

.PHONY: $(BUILD) clean all

#-------------------------------------------------------------------------------
all: $(BUILD)

$(BUILD):
	@$(MAKE) --no-print-directory -C $(WUPS_DIR) lib/libwups.a TOPDIR=$(WUPS_DIR)
	@$(MAKE) --no-print-directory -C $(LIBCONTENTREDIRECTION_DIR) TOPDIR=$(LIBCONTENTREDIRECTION_DIR)
	@$(MAKE) --no-print-directory -C $(LIBNOTIFICATIONS_DIR) TOPDIR=$(LIBNOTIFICATIONS_DIR)
	@$(shell [ ! -d $(BUILD) ] && mkdir -p $(BUILD))
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

#-------------------------------------------------------------------------------
clean:
	@echo clean ...
	@rm -fr $(BUILD) $(TARGET).wps $(TARGET).elf
	@$(MAKE) --no-print-directory -C $(LIBCONTENTREDIRECTION_DIR) clean TOPDIR=$(LIBCONTENTREDIRECTION_DIR)
	@$(MAKE) --no-print-directory -C $(LIBNOTIFICATIONS_DIR) clean TOPDIR=$(LIBNOTIFICATIONS_DIR)
	@$(MAKE) --no-print-directory -C $(WUPS_DIR) clean TOPDIR=$(WUPS_DIR)

#-------------------------------------------------------------------------------
else
.PHONY:	all

DEPENDS	:=	$(OFILES:.o=.d)

#-------------------------------------------------------------------------------
# main targets
#-------------------------------------------------------------------------------
all	:	$(OUTPUT).wps

$(OUTPUT).wps	:	$(OUTPUT).elf
$(OUTPUT).elf	:	$(OFILES)

$(OFILES_SRC)	: $(HFILES_BIN)

#-------------------------------------------------------------------------------
# you need a rule like this for each extension you use as binary data
#-------------------------------------------------------------------------------
%.bin.o	%_bin.h :	%.bin
#-------------------------------------------------------------------------------
	@echo $(notdir $<)
	@$(bin2o)

-include $(DEPENDS)

#-------------------------------------------------------------------------------
endif
#-------------------------------------------------------------------------------
