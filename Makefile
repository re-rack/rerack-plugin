# IMajor2: an iPlug2 VST3 audio-effect plugin around libgmajor2. Run from the project root.
#
#   make                 the VST3 bundle, build/out/IMajor2.vst3 (the default)
#   make test            build and run the tests
#   make install         copy the bundle into VST3_DIR (~/.vst3), replacing an older copy
#   make clean           remove build/
#
# Dependencies are the git submodules under lib/; make checks out the ones the build needs.

CC ?= cc
CXX ?= c++
GIT ?= git
PKG_CONFIG ?= pkg-config
BUILD ?= build
IPLUG2 ?= lib/iplug2
VST3_SDK ?= lib/vst3sdk
GM2_DIR ?= lib/libgmajor2
VST3_DIR ?= $(HOME)/.vst3

ARCH := $(shell uname -m)
OPT := -O3 -DNDEBUG
STRICT_WARNINGS := -Wall -Wextra -Wpedantic -Wshadow -Wconversion
DEPFLAGS := -MD -MP

# --- submodules -------------------------------------------------------------

# A fresh clone leaves the submodule directories empty. Each tree is initialised on its own so a
# submodule the build does not use stays untouched.
# shortcut: a tree that is already checked out is not moved to the recorded revision
IPLUG2_STAMP := $(IPLUG2)/iPlug2.cmake
VST3_STAMP := $(VST3_SDK)/pluginterfaces/base/funknown.h

$(IPLUG2_STAMP):
	$(GIT) submodule update --init $(IPLUG2)

$(VST3_STAMP):
	$(GIT) submodule update --init $(VST3_SDK)/base $(VST3_SDK)/pluginterfaces $(VST3_SDK)/public.sdk

# A source inside a tree make has not seen yet would otherwise be a fatal
# "no rule to make target"; the stamp runs the checkout before the compile starts.
$(IPLUG2)/%: | $(IPLUG2_STAMP)
	@:

$(VST3_SDK)/%: | $(VST3_STAMP)
	@:

# --- libgmajor2 -------------------------------------------------------------

# Listed as plain prerequisites rather than a FORCE: the recursive make decides what is stale,
# and a FORCE would make every make run touch the archive.
GM2_LIB := $(BUILD)/gm2/libgm2.a
GM2_SOURCES := $(wildcard $(GM2_DIR)/src/*.c) $(wildcard $(GM2_DIR)/src/*.h)

$(GM2_DIR)/Makefile:
	$(GIT) submodule update --init $(GM2_DIR)

$(GM2_LIB): $(GM2_SOURCES) $(GM2_DIR)/Makefile
	@echo "Building libgm2"
	@$(MAKE) --no-print-directory -C $(GM2_DIR) build CC='$(CC)' CFLAGS='$(OPT)' \
	  BUILD=$(abspath $(dir $(GM2_LIB)))

# --- compile flags ----------------------------------------------------------

IGRAPHICS_DIR := $(IPLUG2)/Dependencies/IGraphics

PLUGIN_LIBS := $(shell $(PKG_CONFIG) --libs gl glx x11-xcb xcb x11 xext sm ice)

IPLUG_INCLUDE_DIRS := \
  $(IPLUG2)/IPlug/VST3 \
  $(VST3_SDK) \
  $(VST3_SDK)/pluginterfaces \
  $(VST3_SDK)/public.sdk \
  $(VST3_SDK)/public.sdk/source \
  $(VST3_SDK)/base \
  $(IPLUG2)/IPlug \
  $(IPLUG2)/WDL \
  $(IPLUG2)/WDL/libpng \
  $(IPLUG2)/WDL/zlib \
  $(IPLUG2)/IPlug/Extras \
  $(IGRAPHICS_DIR)/glad_GL3/include \
  $(IGRAPHICS_DIR)/glad_GL3/src \
  $(IGRAPHICS_DIR)/NanoVG/src \
  $(IPLUG2)/IGraphics \
  $(IPLUG2)/IGraphics/Controls \
  $(IPLUG2)/IGraphics/Drawing \
  $(IPLUG2)/IGraphics/Platforms \
  $(IPLUG2)/IGraphics/Extras \
  $(IGRAPHICS_DIR)/STB \
  $(IGRAPHICS_DIR)/NanoSVG/src

PLUGIN_FLAGS := \
  -DIGRAPHICS_GL3 -DIGRAPHICS_NANOVG -DIMajor2_vst3_EXPORTS -DIPLUG_DSP=1 -DIPLUG_EDITOR=1 \
  -DNOMINMAX -DOS_LINUX -DRELEASE -DSAMPLE_TYPE_FLOAT -DVST3_API \
  -I$(CURDIR)/Source -I$(CURDIR) -I$(GM2_DIR)/src \
  $(addprefix -isystem ,$(IPLUG_INCLUDE_DIRS)) \
  $(OPT) -std=c++17 -fPIC -Wno-deprecated-declarations -fsigned-char \
  -include $(IPLUG2)/IPlug/IPlugLinuxHeaders.h \
  -Wno-multichar -Wno-stringop-overflow -Wno-stringop-overread
PLUGIN_C_FLAGS := $(filter-out -std=c++17,$(PLUGIN_FLAGS))
TEST_FLAGS := -I$(CURDIR)/Source -I$(GM2_DIR)/src $(OPT) -std=c++17
RESAMPLER_FLAGS := -I$(CURDIR)/Source $(OPT) -std=c++17 -fPIC

# --- sources ----------------------------------------------------------------

OUR_SOURCES := Source/IMajor2.cpp Source/IMajor2Editor.cpp Source/EffectCatalog.cpp

IPLUG2_SOURCES := $(addprefix $(IPLUG2)/, \
  IPlug/VST3/IPlugVST3.cpp \
  IPlug/VST3/IPlugVST3_ProcessorBase.cpp \
  IPlug/IPlugAPIBase.cpp \
  IPlug/IPlugParameter.cpp \
  IPlug/IPlugPaths.cpp \
  IPlug/IPlugPluginBase.cpp \
  IPlug/IPlugProcessor.cpp \
  IPlug/IPlugTimer.cpp \
  IGraphics/IControl.cpp \
  IGraphics/IGraphics.cpp \
  IGraphics/IGraphicsEditorDelegate.cpp \
  IGraphics/Controls/IControls.cpp \
  IGraphics/Controls/IPopupMenuControl.cpp \
  IGraphics/Controls/ITextEntryControl.cpp \
  IGraphics/Platforms/IGraphicsLinux.cpp)

VST3_SOURCES := $(addprefix $(VST3_SDK)/, \
  base/source/baseiids.cpp \
  base/source/fdebug.cpp \
  base/source/fobject.cpp \
  base/source/fstring.cpp \
  base/source/updatehandler.cpp \
  base/source/timer.cpp \
  base/thread/source/flock.cpp \
  pluginterfaces/base/funknown.cpp \
  pluginterfaces/base/ustring.cpp \
  pluginterfaces/base/coreiids.cpp \
  public.sdk/source/common/pluginview.cpp \
  public.sdk/source/common/commoniids.cpp \
  public.sdk/source/vst/vstbus.cpp \
  public.sdk/source/vst/vstcomponent.cpp \
  public.sdk/source/vst/vstcomponentbase.cpp \
  public.sdk/source/vst/vstinitiids.cpp \
  public.sdk/source/vst/vstparameters.cpp \
  public.sdk/source/vst/vstsinglecomponenteffect.cpp \
  public.sdk/source/main/pluginfactory.cpp \
  public.sdk/source/main/linuxmain.cpp)

PLUGIN_C_SOURCES := \
  $(IGRAPHICS_DIR)/glad_GL3/src/glad.c \
  $(IGRAPHICS_DIR)/NanoVG/src/nanovg.c

RESAMPLER_SOURCE := Source/Resampler.cpp
TEST_SOURCES := tests/resampler_test.cpp tests/depop_test.cpp tests/knob_format_test.cpp \
                Source/EffectCatalog.cpp

PLUGIN_OBJS := $(addprefix $(BUILD)/obj/plugin/,$(addsuffix .o,$(basename $(OUR_SOURCES) $(IPLUG2_SOURCES) $(VST3_SOURCES))))
STRICT_PLUGIN_OBJS := $(addprefix $(BUILD)/obj/plugin/,$(addsuffix .o,$(basename $(OUR_SOURCES))))
PLUGIN_C_OBJS := $(addprefix $(BUILD)/obj/plugin/,$(addsuffix .o,$(basename $(PLUGIN_C_SOURCES))))
RESAMPLER_OBJ := $(BUILD)/obj/lib/$(basename $(RESAMPLER_SOURCE)).o
TEST_OBJS := $(addprefix $(BUILD)/obj/test/,$(addsuffix .o,$(basename $(TEST_SOURCES))))

PLUGIN_ORDER := $(IPLUG2_STAMP) $(VST3_STAMP) $(GM2_DIR)/Makefile

# --- targets ----------------------------------------------------------------

BUNDLE := $(BUILD)/out/IMajor2.vst3
PLUGIN_SO := $(BUNDLE)/Contents/$(ARCH)-linux/IMajor2.so
PLUGIN_TTF := $(BUNDLE)/Contents/Resources/ttf/Roboto-Regular.ttf
FONT_SOURCE := $(IPLUG2)/Examples/IPlugEffect/resources/fonts/Roboto-Regular.ttf

TESTS := $(BUILD)/tests/resampler_test $(BUILD)/tests/depop_test $(BUILD)/tests/knob_format_test

.DEFAULT_GOAL := all
.DELETE_ON_ERROR:

all: $(PLUGIN_SO) $(PLUGIN_TTF)

test: $(TESTS)
	@set -e; for test in $(TESTS); do echo "run $$test"; $$test; done

install: all
	mkdir -p $(VST3_DIR)
	rm -rf $(VST3_DIR)/IMajor2.vst3
	cp -r $(BUNDLE) $(VST3_DIR)/

clean:
	rm -rf $(BUILD)

$(PLUGIN_SO): $(PLUGIN_OBJS) $(PLUGIN_C_OBJS) $(RESAMPLER_OBJ) $(GM2_LIB)
	@mkdir -p $(@D)
	$(CXX) -fPIC -O3 -shared -static-libstdc++ -static-libgcc -o $@ \
	  $(PLUGIN_OBJS) $(PLUGIN_C_OBJS) $(RESAMPLER_OBJ) $(GM2_LIB) $(PLUGIN_LIBS) -lpthread -ldl

$(PLUGIN_TTF): | $(IPLUG2_STAMP)
	@mkdir -p $(@D)
	cp -f $(FONT_SOURCE) $@

$(BUILD)/tests/resampler_test: $(BUILD)/obj/test/tests/resampler_test.o $(RESAMPLER_OBJ)
	@mkdir -p $(@D)
	$(CXX) $(OPT) -o $@ $^

$(BUILD)/tests/depop_test: $(BUILD)/obj/test/tests/depop_test.o \
                           $(BUILD)/obj/test/Source/EffectCatalog.o $(GM2_LIB)
	@mkdir -p $(@D)
	$(CXX) $(OPT) -o $@ $^

$(BUILD)/tests/knob_format_test: $(BUILD)/obj/test/tests/knob_format_test.o \
                                 $(BUILD)/obj/test/Source/EffectCatalog.o $(GM2_LIB)
	@mkdir -p $(@D)
	$(CXX) $(OPT) -o $@ $^

$(PLUGIN_OBJS): $(BUILD)/obj/plugin/%.o: %.cpp | $(PLUGIN_ORDER)
	@mkdir -p $(@D)
	$(CXX) $(PLUGIN_FLAGS) $(PLUGIN_WARNINGS) $(DEPFLAGS) -c $< -o $@

$(STRICT_PLUGIN_OBJS): PLUGIN_WARNINGS := $(STRICT_WARNINGS)

$(PLUGIN_C_OBJS): $(BUILD)/obj/plugin/%.o: %.c | $(PLUGIN_ORDER)
	@mkdir -p $(@D)
	$(CC) $(PLUGIN_C_FLAGS) $(DEPFLAGS) -c $< -o $@

$(RESAMPLER_OBJ): $(BUILD)/obj/lib/%.o: %.cpp | $(GM2_DIR)/Makefile
	@mkdir -p $(@D)
	$(CXX) $(RESAMPLER_FLAGS) $(STRICT_WARNINGS) $(DEPFLAGS) -c $< -o $@

$(TEST_OBJS): $(BUILD)/obj/test/%.o: %.cpp | $(GM2_DIR)/Makefile
	@mkdir -p $(@D)
	$(CXX) $(TEST_FLAGS) $(STRICT_WARNINGS) $(DEPFLAGS) -c $< -o $@

-include $(PLUGIN_OBJS:.o=.d) $(PLUGIN_C_OBJS:.o=.d) $(RESAMPLER_OBJ:.o=.d) $(TEST_OBJS:.o=.d)

.PHONY: all test install clean
