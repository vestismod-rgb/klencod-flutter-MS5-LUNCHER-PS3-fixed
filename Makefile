#---------------------------------------------------------------------------------
# MS5 LUNCHER - PS3 homebrew (PSL1GHT)
#   make        -> MS5Luncher.self
#   make pkg    -> MS5Luncher.pkg
#---------------------------------------------------------------------------------
.SUFFIXES:
#---------------------------------------------------------------------------------
ifeq ($(strip $(PSL1GHT)),)
$(error "Please set PSL1GHT in your environment. export PSL1GHT=<path>")
endif

include $(PSL1GHT)/ppu_rules

#---------------------------------------------------------------------------------
TARGET		:=	MS5Luncher
BUILD		:=	build
SOURCES		:=	source
DATA		:=	data
INCLUDES	:=	include

TITLE		:=	MS5 LUNCHER
# keep APPID in sync with SELF_ID in source/scan.c
APPID		:=	MS5LNCHR1
CONTENTID	:=	UP0001-$(APPID)_00-0000000000000000
PKGFILES	:=	pkgfiles

CFLAGS		=	-O2 -Wall -Wno-format-truncation -std=gnu99 -mcpu=cell $(MACHDEP) $(INCLUDE)
CXXFLAGS	=	$(CFLAGS)
# link with the PPU compiler driver (the default LD is the host ld, which rejects -mhard-float)
LD		:=	$(CXX)

LDFLAGS		=	$(MACHDEP) -Wl,-Map,$(notdir $@).map

LIBS		:=	-lrsx -lgcm_sys -lio -lnet -lsysutil -lrt -llv2 -lm

LIBDIRS		:=	$(PORTLIBS)

#---------------------------------------------------------------------------------
ifneq ($(BUILD),$(notdir $(CURDIR)))
#---------------------------------------------------------------------------------

export OUTPUT	:=	$(CURDIR)/$(TARGET)

export VPATH	:=	$(foreach dir,$(SOURCES),$(CURDIR)/$(dir)) \
			$(foreach dir,$(DATA),$(CURDIR)/$(dir))

export BUILDDIR	:=	$(CURDIR)/$(BUILD)
export DEPSDIR	:=	$(BUILDDIR)

CFILES		:=	$(foreach dir,$(SOURCES), $(notdir $(wildcard $(dir)/*.c)))
CXXFILES	:=	$(foreach dir,$(SOURCES), $(notdir $(wildcard $(dir)/*.cpp)))
SFILES		:=	$(foreach dir,$(SOURCES), $(notdir $(wildcard $(dir)/*.S)))
BINFILES	:=	$(foreach dir,$(DATA), $(notdir $(wildcard $(dir)/*.*)))

export OFILES	:=	$(addsuffix .o,$(BINFILES)) \
			$(CFILES:.c=.o) $(CXXFILES:.cpp=.o) \
			$(SFILES:.S=.o)

export INCLUDE	:=	$(foreach dir,$(INCLUDES), -I$(CURDIR)/$(dir)) \
			$(foreach dir,$(LIBDIRS),-I$(dir)/include) \
			-I$(CURDIR)/$(BUILD) -I$(PSL1GHT)/ppu/include

export LIBPATHS	:=	$(foreach dir,$(LIBDIRS),-L$(dir)/lib) \
			-L$(PSL1GHT)/ppu/lib

.PHONY: $(BUILD) clean pkg

$(BUILD):
	@[ -d $@ ] || mkdir -p $@
	@$(MAKE) --no-print-directory -C $(BUILD) -f $(CURDIR)/Makefile

clean:
	@echo clean ...
	@rm -rf $(BUILD) $(OUTPUT).elf $(OUTPUT).self $(OUTPUT).a $(OUTPUT).pkg

pkg: $(BUILD) $(OUTPUT).pkg

#---------------------------------------------------------------------------------
else

DEPENDS	:=	$(OFILES:.o=.d)

$(OUTPUT).self: $(OUTPUT).elf
$(OUTPUT).elf:	$(OFILES)

%.bin.o	:	%.bin
	@echo $(notdir $<)
	@$(bin2o)

-include $(DEPENDS)

#---------------------------------------------------------------------------------
endif
#---------------------------------------------------------------------------------
