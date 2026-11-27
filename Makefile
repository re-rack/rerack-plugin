# IMajor2: an iPlug2 VST3 audio-effect plugin around libgmajor2. Run from the project root.
#
#   make                 the VST3 bundle, build/out/IMajor2.vst3 (the default)
#   make test            build and run the tests
#   make install         copy the bundle into VST3_DIR (~/.vst3), replacing an older copy
#   make deps            fetch iPlug2 and the VST3 SDK, and build Skia where a library is missing
#   make clean           remove build/, Skia's gn output and its fetched externals; deps/skia/lib
#                        and the Skia sources stay, so no Skia rebuild follows
#
# Fetched dependencies and the build land under deps/ and build/, both ignored by git.

CC ?= cc
CXX ?= c++
GIT ?= git
CURL ?= curl
SHA256SUM ?= sha256sum
PKG_CONFIG ?= pkg-config
JOBS ?= $(shell nproc)
BUILD ?= build
DEPS ?= deps
GM2_DIR ?= lib/libgmajor2
VST3_DIR ?= $(HOME)/.vst3

ARCH := $(shell uname -m)
OPT := -O3 -DNDEBUG
STRICT_WARNINGS := -Wall -Wextra -Wpedantic -Wshadow -Wconversion
DEPFLAGS := -MD -MP

# --- fetched dependencies ---------------------------------------------------

IPLUG2_REV := a6186c7a75cc961b12038d484bbc37124043b08b
IPLUG2_URL := https://github.com/iPlug2/iPlug2/archive/$(IPLUG2_REV).tar.gz
IPLUG2_SHA256 := c88a679d61176d66b4cbde6ef6e8ec1bb33b114c28f3c0390666bd43a47d463f
IPLUG2 := $(DEPS)/iplug2
IPLUG2_STAMP := $(IPLUG2)/iPlug2.cmake

VST3_TAG := v3.8.1_build_84
VST3_URL := https://github.com/steinbergmedia/vst3sdk.git
VST3_SDK := $(IPLUG2)/Dependencies/IPlug/VST3_SDK
VST3_STAMP := $(VST3_SDK)/pluginterfaces/base/funknown.h

SKIA := $(DEPS)/skia
SKIA_NAMES := svg skparagraph skshaper skunicode_icu skunicode_core skia
SKIA_ARCHIVES := $(addprefix $(SKIA)/lib/lib,$(addsuffix .a,$(SKIA_NAMES)))

deps: $(IPLUG2_STAMP) $(VST3_STAMP) $(SKIA_ARCHIVES)

$(IPLUG2_STAMP):
	@echo "Fetching iPlug2 $(IPLUG2_REV)"
	@mkdir -p $(DEPS)
	$(CURL) -fsSL -o $(DEPS)/iplug2.tar.gz $(IPLUG2_URL)
	echo '$(IPLUG2_SHA256)  $(DEPS)/iplug2.tar.gz' | $(SHA256SUM) -c -
	rm -rf $(DEPS)/iplug2-unpack
	mkdir -p $(DEPS)/iplug2-unpack
	tar -xzf $(DEPS)/iplug2.tar.gz -C $(DEPS)/iplug2-unpack --strip-components=1
	rm -rf $(IPLUG2)
	mv $(DEPS)/iplug2-unpack $(IPLUG2)
	rm -f $(DEPS)/iplug2.tar.gz

# iPlug2 only looks for the VST3 SDK inside its own tree.
$(VST3_STAMP): | $(IPLUG2_STAMP)
	@echo "Fetching the VST3 SDK $(VST3_TAG)"
	rm -rf $(VST3_SDK)
	$(GIT) clone -q --depth 1 --branch $(VST3_TAG) $(VST3_URL) $(VST3_SDK)
	$(GIT) -C $(VST3_SDK) submodule --quiet update --init --depth 1 \
	  base pluginterfaces public.sdk

# Grouped: a missing archive runs the script once, which installs all six.
$(SKIA_ARCHIVES) &: | $(IPLUG2_STAMP)
	@echo "Building Skia into $(SKIA), this takes a long time"
	IPLUG_SKIA_LINUX_DIR=$(abspath $(SKIA)) SKIA_JOBS=$(JOBS) \
	  bash $(IPLUG2)/Dependencies/IGraphics/build-skia-linux.sh

# The fetched trees are created by the rules above; a source make has not seen yet would
# otherwise be a fatal "no rule to make target".
$(IPLUG2)/%: | $(IPLUG2_STAMP) $(VST3_STAMP)
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

FREETYPE_INCLUDES := $(patsubst -I%,-isystem %,$(shell $(PKG_CONFIG) --cflags freetype2))
PLUGIN_LIBS := $(shell $(PKG_CONFIG) --libs glx opengl sm ice x11 xext x11-xcb xcb fontconfig expat)

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
  $(IPLUG2)/Dependencies/IGraphics/glad_GL3/include \
  $(SKIA)/src/skia \
  $(SKIA)/src/skia/include \
  $(SKIA)/src/skia/include/core \
  $(SKIA)/src/skia/include/effects \
  $(SKIA)/src/skia/include/config \
  $(SKIA)/src/skia/include/utils \
  $(SKIA)/src/skia/include/utils/mac \
  $(SKIA)/src/skia/include/gpu \
  $(SKIA)/src/skia/include/private \
  $(SKIA)/src/skia/modules/svg/include \
  $(SKIA)/src/skia/modules/skcms \
  $(IPLUG2)/IGraphics \
  $(IPLUG2)/IGraphics/Controls \
  $(IPLUG2)/IGraphics/Drawing \
  $(IPLUG2)/IGraphics/Platforms \
  $(IPLUG2)/IGraphics/Extras \
  $(IPLUG2)/Dependencies/IGraphics/STB \
  $(IPLUG2)/Dependencies/IGraphics/NanoSVG/src

PLUGIN_FLAGS := \
  -DIGRAPHICS_GL3 -DIGRAPHICS_SKIA -DIMajor2_vst3_EXPORTS -DIPLUG_DSP=1 -DIPLUG_EDITOR=1 \
  -DNOMINMAX -DOS_LINUX -DRELEASE -DSAMPLE_TYPE_FLOAT -DSK_RELEASE -DVST3_API \
  -I$(CURDIR)/Source -I$(CURDIR) -I$(GM2_DIR)/src \
  $(addprefix -isystem ,$(IPLUG_INCLUDE_DIRS)) $(FREETYPE_INCLUDES) \
  $(OPT) -std=c++17 -fPIC -Wno-deprecated-declarations -fsigned-char \
  -include $(IPLUG2)/IPlug/IPlugLinuxHeaders.h \
  -Wno-multichar -Wno-stringop-overflow -Wno-stringop-overread
GLAD_FLAGS := $(filter-out -std=c++17,$(PLUGIN_FLAGS))
TEST_FLAGS := -I$(CURDIR)/Source -I$(GM2_DIR)/src $(OPT) -std=c++17
RESAMPLER_FLAGS := -I$(CURDIR)/Source $(OPT) -std=c++17 -fPIC

# --- sources ----------------------------------------------------------------

OUR_SOURCES := Source/IMajor2.cpp Source/IMajor2Editor.cpp Source/EffectCatalog.cpp

IPLUG2_SOURCES := $(addprefix $(IPLUG2)/, \
  IPlug/VST3/IPlugVST3.cpp \
  IPlug/VST3/IPlugVST3_ProcessorBase.cpp \
  Dependencies/IPlug/VST3_SDK/base/source/baseiids.cpp \
  Dependencies/IPlug/VST3_SDK/base/source/fdebug.cpp \
  Dependencies/IPlug/VST3_SDK/base/source/fobject.cpp \
  Dependencies/IPlug/VST3_SDK/base/source/fstring.cpp \
  Dependencies/IPlug/VST3_SDK/base/source/updatehandler.cpp \
  Dependencies/IPlug/VST3_SDK/base/thread/source/flock.cpp \
  Dependencies/IPlug/VST3_SDK/pluginterfaces/base/funknown.cpp \
  Dependencies/IPlug/VST3_SDK/pluginterfaces/base/ustring.cpp \
  Dependencies/IPlug/VST3_SDK/pluginterfaces/base/coreiids.cpp \
  Dependencies/IPlug/VST3_SDK/public.sdk/source/common/pluginview.cpp \
  Dependencies/IPlug/VST3_SDK/public.sdk/source/common/commoniids.cpp \
  Dependencies/IPlug/VST3_SDK/public.sdk/source/vst/vstbus.cpp \
  Dependencies/IPlug/VST3_SDK/public.sdk/source/vst/vstcomponent.cpp \
  Dependencies/IPlug/VST3_SDK/public.sdk/source/vst/vstcomponentbase.cpp \
  Dependencies/IPlug/VST3_SDK/public.sdk/source/vst/vstinitiids.cpp \
  Dependencies/IPlug/VST3_SDK/public.sdk/source/vst/vstparameters.cpp \
  Dependencies/IPlug/VST3_SDK/public.sdk/source/vst/vstsinglecomponenteffect.cpp \
  Dependencies/IPlug/VST3_SDK/public.sdk/source/main/pluginfactory.cpp \
  Dependencies/IPlug/VST3_SDK/public.sdk/source/main/linuxmain.cpp \
  Dependencies/IPlug/VST3_SDK/base/source/timer.cpp \
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

GLAD_SOURCE := $(IPLUG2)/Dependencies/IGraphics/glad_GL3/src/glad.c
RESAMPLER_SOURCE := Source/Resampler.cpp
TEST_SOURCES := tests/resampler_test.cpp tests/depop_test.cpp tests/knob_format_test.cpp \
                Source/EffectCatalog.cpp

PLUGIN_OBJS := $(addprefix $(BUILD)/obj/plugin/,$(addsuffix .o,$(basename $(OUR_SOURCES) $(IPLUG2_SOURCES))))
STRICT_PLUGIN_OBJS := $(addprefix $(BUILD)/obj/plugin/,$(addsuffix .o,$(basename $(OUR_SOURCES))))
GLAD_OBJ := $(BUILD)/obj/plugin/$(basename $(GLAD_SOURCE)).o
RESAMPLER_OBJ := $(BUILD)/obj/lib/$(basename $(RESAMPLER_SOURCE)).o
TEST_OBJS := $(addprefix $(BUILD)/obj/test/,$(addsuffix .o,$(basename $(TEST_SOURCES))))

PLUGIN_ORDER := $(IPLUG2_STAMP) $(VST3_STAMP) $(SKIA_ARCHIVES) $(GM2_DIR)/Makefile

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
	rm -rf $(BUILD) $(SKIA)/out $(SKIA)/src/skia/third_party/externals

$(PLUGIN_SO): $(PLUGIN_OBJS) $(GLAD_OBJ) $(RESAMPLER_OBJ) $(GM2_LIB) $(SKIA_ARCHIVES)
	@mkdir -p $(@D)
	$(CXX) -fPIC -O3 -shared -static-libstdc++ -static-libgcc -o $@ \
	  $(PLUGIN_OBJS) $(GLAD_OBJ) $(RESAMPLER_OBJ) $(GM2_LIB) $(SKIA_ARCHIVES) \
	  $(PLUGIN_LIBS) -lpthread -ldl

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

$(GLAD_OBJ): $(BUILD)/obj/plugin/%.o: %.c | $(PLUGIN_ORDER)
	@mkdir -p $(@D)
	$(CC) $(GLAD_FLAGS) $(DEPFLAGS) -c $< -o $@

$(RESAMPLER_OBJ): $(BUILD)/obj/lib/%.o: %.cpp | $(GM2_DIR)/Makefile
	@mkdir -p $(@D)
	$(CXX) $(RESAMPLER_FLAGS) $(STRICT_WARNINGS) $(DEPFLAGS) -c $< -o $@

$(TEST_OBJS): $(BUILD)/obj/test/%.o: %.cpp | $(GM2_DIR)/Makefile
	@mkdir -p $(@D)
	$(CXX) $(TEST_FLAGS) $(STRICT_WARNINGS) $(DEPFLAGS) -c $< -o $@

-include $(PLUGIN_OBJS:.o=.d) $(GLAD_OBJ:.o=.d) $(RESAMPLER_OBJ:.o=.d) $(TEST_OBJS:.o=.d)

.PHONY: all test install deps clean
