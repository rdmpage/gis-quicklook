# GISLook QuickLook Generator + GISTypes UTI Registration
# Builds GISLook.qlgenerator for macOS 13+ (universal binary: arm64 + x86_64)
# and GISTypes.app (stub app that registers GIS UTIs with Launch Services).
#
# Usage:
#   make          - build both the plugin and the UTI stub app
#   make install  - install to ~/Library/QuickLook/ and ~/Applications/,
#                   register UTIs, and reload QuickLook
#   make test     - test the plugin with qlmanage (set TEST_FILE=path/to/file.shp)
#   make uninstall - remove both installed items
#   make clean    - remove build artifacts

# ── Toolchain ────────────────────────────────────────────────────────────────
CC      := xcrun clang
SDK     := $(shell xcrun --show-sdk-path --sdk macosx)
ARCHS   := -arch arm64 -arch x86_64
LSREG   := /System/Library/Frameworks/CoreServices.framework/Versions/A/Frameworks/LaunchServices.framework/Versions/A/Support/lsregister

# Code-signing identity.  Use your Apple Development certificate so AMFI
# (Apple Mobile File Integrity) accepts the generator when quicklookd loads it.
# Ad-hoc signing (-) is rejected by AMFI for code loaded by system daemons.
CODESIGN_ID := Apple Development: r.page@bio.gla.ac.uk (X9YL37JCBJ)

# ── Paths ─────────────────────────────────────────────────────────────────────
SRCDIR   := GISSource
PLGDIR   := GISLook
TYPDIR   := GISTypes
BUILDDIR := build

# QuickLook generator bundle
BUNDLE  := $(BUILDDIR)/GISLook.qlgenerator
BINARY  := $(BUNDLE)/Contents/MacOS/GISLook

# UTI registration stub app
TYPES_APP := $(BUILDDIR)/GISTypes.app
TYPES_BIN := $(TYPES_APP)/Contents/MacOS/GISTypes

# Install destinations
QL_INSTALL_DIR   := $(HOME)/Library/QuickLook
APPS_INSTALL_DIR := $(HOME)/Applications

# ── Compiler flags ────────────────────────────────────────────────────────────
CFLAGS := \
	$(ARCHS) \
	-isysroot $(SDK) \
	-mmacosx-version-min=13.0 \
	-O2 \
	-Wall \
	-Wno-deprecated-declarations \
	-I$(SRCDIR) \
	-I$(SRCDIR)/avce00-2.0.0 \
	-I$(SRCDIR)/e00compr-1.0.0 \
	-I$(SRCDIR)/shape

LDFLAGS := \
	$(ARCHS) \
	-isysroot $(SDK) \
	-mmacosx-version-min=13.0 \
	-dynamiclib \
	-framework QuickLook \
	-framework CoreFoundation \
	-framework CoreServices \
	-framework ApplicationServices

# ── Source files ──────────────────────────────────────────────────────────────
# Plugin entry points
PLUGIN_SRCS := \
	$(PLGDIR)/main.c \
	$(PLGDIR)/GeneratePreviewForURL.c \
	$(PLGDIR)/GenerateThumbnailForURL.c

# GIS format readers (top-level)
GISSOURCE_SRCS := \
	$(SRCDIR)/BILToImage.c \
	$(SRCDIR)/Color.c \
	$(SRCDIR)/E00.c \
	$(SRCDIR)/E00GridToImage.c \
	$(SRCDIR)/ESRIASCIIGridToImage.c \
	$(SRCDIR)/ESRIBinaryGridToImage.c \
	$(SRCDIR)/ESRIShape.c \
	$(SRCDIR)/File.c \
	$(SRCDIR)/HdrFile.c \
	$(SRCDIR)/PGMToImage.c \
	$(SRCDIR)/ReadRaster.c \
	$(SRCDIR)/ReadVector.c \
	$(SRCDIR)/SRTMToImage.c \
	$(SRCDIR)/SurferGridToImage.c \
	$(SRCDIR)/USGSDEMToImage.c \
	$(SRCDIR)/strlwr.c

# Bundled third-party libraries
AVCE00_SRCS := \
	$(SRCDIR)/avce00-2.0.0/avc_bin.c \
	$(SRCDIR)/avce00-2.0.0/avc_binwr.c \
	$(SRCDIR)/avce00-2.0.0/avc_e00gen.c \
	$(SRCDIR)/avce00-2.0.0/avc_e00parse.c \
	$(SRCDIR)/avce00-2.0.0/avc_e00read.c \
	$(SRCDIR)/avce00-2.0.0/avc_e00write.c \
	$(SRCDIR)/avce00-2.0.0/avc_mbyte.c \
	$(SRCDIR)/avce00-2.0.0/avc_misc.c \
	$(SRCDIR)/avce00-2.0.0/avc_rawbin.c \
	$(SRCDIR)/avce00-2.0.0/cpl_conv.c \
	$(SRCDIR)/avce00-2.0.0/cpl_dir.c \
	$(SRCDIR)/avce00-2.0.0/cpl_error.c \
	$(SRCDIR)/avce00-2.0.0/cpl_string.c \
	$(SRCDIR)/avce00-2.0.0/cpl_vsisimple.c

E00COMPR_SRCS := \
	$(SRCDIR)/e00compr-1.0.0/e00conv.c \
	$(SRCDIR)/e00compr-1.0.0/e00error.c \
	$(SRCDIR)/e00compr-1.0.0/e00read.c \
	$(SRCDIR)/e00compr-1.0.0/e00write.c

SHAPE_SRCS := \
	$(SRCDIR)/shape/dbfopen.c \
	$(SRCDIR)/shape/shpopen.c \
	$(SRCDIR)/shape/shptree.c

ALL_SRCS := $(PLUGIN_SRCS) $(GISSOURCE_SRCS) $(AVCE00_SRCS) $(E00COMPR_SRCS) $(SHAPE_SRCS)

# Derive object file paths under build/obj/
OBJS := $(patsubst %.c,$(BUILDDIR)/obj/%.o,$(ALL_SRCS))

# ── Default target ─────────────────────────────────────────────────────────────
.PHONY: all
all: $(BUNDLE) $(TYPES_APP)

# ── QuickLook generator bundle assembly ────────────────────────────────────────
$(BUNDLE): $(BINARY) $(PLGDIR)/Info.plist
	@cp $(PLGDIR)/Info.plist $(BUNDLE)/Contents/Info.plist
	@codesign --force --sign "$(CODESIGN_ID)" $(BUNDLE)
	@echo "✓ Built and signed: $(BUNDLE)"

# ── Link plugin ────────────────────────────────────────────────────────────────
$(BINARY): $(OBJS)
	@mkdir -p $(BUNDLE)/Contents/MacOS
	@mkdir -p $(BUNDLE)/Contents/Resources
	$(CC) $(LDFLAGS) -o $@ $^

# ── Compile (generic rule) ────────────────────────────────────────────────────
$(BUILDDIR)/obj/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

# ── UTI stub app ──────────────────────────────────────────────────────────────
$(TYPES_APP): $(TYPES_BIN) $(TYPDIR)/Info.plist
	@cp $(TYPDIR)/Info.plist $(TYPES_APP)/Contents/Info.plist
	@codesign --force --sign "$(CODESIGN_ID)" $(TYPES_APP)
	@echo "✓ Built and signed: $(TYPES_APP)"

$(TYPES_BIN): $(TYPDIR)/main.c
	@mkdir -p $(TYPES_APP)/Contents/MacOS
	@mkdir -p $(TYPES_APP)/Contents/Resources
	$(CC) $(ARCHS) -isysroot $(SDK) -mmacosx-version-min=13.0 -o $@ $<

# ── Install ───────────────────────────────────────────────────────────────────
.PHONY: install
install: all
	@mkdir -p $(QL_INSTALL_DIR)
	@rm -rf $(QL_INSTALL_DIR)/GISLook.qlgenerator
	@cp -r $(BUNDLE) $(QL_INSTALL_DIR)/
	@mkdir -p $(APPS_INSTALL_DIR)
	@rm -rf $(APPS_INSTALL_DIR)/GISTypes.app
	@cp -r $(TYPES_APP) $(APPS_INSTALL_DIR)/
	@$(LSREG) -f $(APPS_INSTALL_DIR)/GISTypes.app
	@qlmanage -r
	@qlmanage -r cache
	@echo "✓ Installed GISLook.qlgenerator to $(QL_INSTALL_DIR)"
	@echo "✓ Installed GISTypes.app to $(APPS_INSTALL_DIR) (UTIs registered)"
	@echo "  QuickLook cache reset. You may need to log out and back in"
	@echo "  for Finder icon thumbnails to update."

# ── Uninstall ─────────────────────────────────────────────────────────────────
.PHONY: uninstall
uninstall:
	@rm -rf $(QL_INSTALL_DIR)/GISLook.qlgenerator
	@rm -rf $(APPS_INSTALL_DIR)/GISTypes.app
	@qlmanage -r
	@echo "✓ Uninstalled GISLook.qlgenerator and GISTypes.app"

# ── Test ──────────────────────────────────────────────────────────────────────
# Usage: make test TEST_FILE=/path/to/file.shp
# Installs first so qlmanage can find the plugin.
# Detects file extension and passes -c <UTI> so the test works even before
# the UTI registration has fully propagated.
.PHONY: test
test: install
	@if [ -z "$(TEST_FILE)" ]; then \
		echo "Usage: make test TEST_FILE=/path/to/your/file.shp"; \
		exit 1; \
	fi
	@mdimport "$(abspath $(TEST_FILE))" 2>/dev/null || true
	@ext=$$(echo "$(abspath $(TEST_FILE))" | sed 's/.*\.//'); \
	uti=$$(case "$$ext" in \
		shp|shx|dbf|prj|sbn|sbx|shb) echo com.esri.shape ;; \
		e00) echo com.esri.e00 ;; \
		adf) echo com.esri.coverage ;; \
		asc) echo com.esri.asciigrid ;; \
		bil) echo com.esri.bil ;; \
		bip) echo com.esri.bip ;; \
		bsq) echo com.esri.bsq ;; \
		flt) echo com.esri.binarygrid ;; \
		grd) echo com.goldensoftware.surfer.grid ;; \
		hgt) echo gov.nasa.srtm ;; \
		dem) echo gov.usgs.dem ;; \
		pgm) echo net.sourceforge.netpbm.pgm ;; \
		*) echo "" ;; \
	esac); \
	if [ -n "$$uti" ]; then \
		echo "Previewing $(abspath $(TEST_FILE)) as $$uti"; \
		qlmanage -p -c "$$uti" "$(abspath $(TEST_FILE))" 2>&1; \
	else \
		echo "Previewing $(abspath $(TEST_FILE)) (unknown extension, using system UTI)"; \
		qlmanage -p "$(abspath $(TEST_FILE))" 2>&1; \
	fi

# ── Trust (Gatekeeper) ────────────────────────────────────────────────────────
# Adds the installed generator to macOS Gatekeeper's allow list so that
# quicklookd (the daemon used by Finder) can load it.  Ad-hoc signed bundles
# are rejected by Gatekeeper without this step.
#
# Usage: sudo make trust
# Must be re-run after each 'make install' because the code-directory hash
# changes with every build.
.PHONY: trust
trust: install
	sudo spctl --add $(QL_INSTALL_DIR)/GISLook.qlgenerator
	@qlmanage -r
	@qlmanage -r cache
	@echo "✓ GISLook.qlgenerator added to Gatekeeper allow list"
	@echo "  Finder previews and thumbnails should now work."
	@echo "  Remember to re-run 'sudo make trust' after each rebuild."

# ── Clean ─────────────────────────────────────────────────────────────────────
.PHONY: clean
clean:
	@rm -rf $(BUILDDIR)
	@echo "✓ Cleaned build artifacts"
