#---------------------------------------------------------------------------------
.SUFFIXES:
#---------------------------------------------------------------------------------

ifeq ($(strip $(DEVKITARM)),)
$(error "Please set DEVKITARM in your environment. export DEVKITARM=<path to>devkitARM")
endif

TOPDIR ?= $(CURDIR)
include $(DEVKITARM)/3ds_rules

TARGET      := Chess3DS
BUILD       := build
SOURCES     := source source/chess source/engine source/platform source/ui vendor/stockfish/src
DATA        :=
INCLUDES    := include vendor/stockfish/src
GRAPHICS    :=
GFXBUILD    := $(BUILD)

APP_TITLE       := Chess3DS
APP_DESCRIPTION := Fast offline chess with Stockfish 11
APP_AUTHOR      := mexo4
ICON            := icon.png

ARCH        := -march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft
CFLAGS      := -g -Wall -Wextra -O2 -mword-relocations -ffunction-sections $(ARCH)
CFLAGS      += $(INCLUDE) -D__3DS__ -DCHESS3DS_EMBEDDED -DNO_PREFETCH
CXXFLAGS    := $(CFLAGS) -fno-rtti -fno-exceptions -std=gnu++17 -pthread
ASFLAGS     := -g $(ARCH)
LDFLAGS     := -specs=3dsx.specs -g $(ARCH) -pthread -Wl,-Map,$(notdir $*.map)
LIBS        := -lcitro2d -lcitro3d -lctru -lm
LIBDIRS     := $(CTRULIB)

ifneq ($(BUILD),$(notdir $(CURDIR)))

export OUTPUT := $(CURDIR)/$(TARGET)
export TOPDIR := $(CURDIR)
export VPATH := $(foreach dir,$(SOURCES),$(CURDIR)/$(dir))
export DEPSDIR := $(CURDIR)/$(BUILD)

APP_DIRS := $(filter-out vendor/stockfish/src,$(SOURCES))
APP_CPPFILES := $(foreach dir,$(APP_DIRS),$(notdir $(wildcard $(dir)/*.cpp)))
SF_CPPFILES := $(filter-out main.cpp,$(notdir $(wildcard vendor/stockfish/src/*.cpp)))
CPPFILES := $(APP_CPPFILES) $(SF_CPPFILES)
CFILES := $(foreach dir,$(APP_DIRS),$(notdir $(wildcard $(dir)/*.c)))
SFILES := $(foreach dir,$(APP_DIRS),$(notdir $(wildcard $(dir)/*.s)))

export LD := $(CXX)
export OFILES := $(CPPFILES:.cpp=.o) $(CFILES:.c=.o) $(SFILES:.s=.o)
export INCLUDE := $(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) \
                  $(foreach dir,$(LIBDIRS),-I$(dir)/include) \
                  -I$(CURDIR)/$(BUILD)
export LIBPATHS := $(foreach dir,$(LIBDIRS),-L$(dir)/lib)
export _3DSXDEPS := $(OUTPUT).smdh
export APP_ICON := $(TOPDIR)/$(ICON)
export _3DSXFLAGS += --smdh=$(OUTPUT).smdh

.PHONY: all clean cia

all: $(BUILD)
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

$(BUILD):
	@mkdir -p $@

cia: all
	@command -v cxitool >/dev/null || { echo "cxitool is required for CIA packaging"; exit 1; }
	@command -v makerom >/dev/null || { echo "makerom is required for CIA packaging"; exit 1; }
	cxitool $(TARGET).3dsx $(TARGET).cxi
	makerom -v -f cia -o $(TARGET).cia -target t -i $(TARGET).cxi:0:0 -ignoresign -icon $(TARGET).smdh

clean:
	@echo clean ...
	@rm -rf $(BUILD) $(TARGET).3dsx $(TARGET).smdh $(TARGET).elf $(TARGET).cxi $(TARGET).cia

else

$(OUTPUT).3dsx: $(OUTPUT).elf $(_3DSXDEPS)
$(OUTPUT).elf: $(OFILES)

-include $(DEPSDIR)/*.d

endif
