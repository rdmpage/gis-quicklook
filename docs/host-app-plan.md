# Host App Plan

A minimal macOS app (`GISLook.app`) that bundles the QuickLook plugin, registers UTIs,
and gives users a clean drag-to-Applications install experience.

---

## Why a host app?

- `.qlgenerator` bundles cannot be double-clicked and have no install UX on their own
- macOS auto-registers any `.qlgenerator` found at `App.app/Contents/Library/QuickLook/`
  when the app lives in `/Applications` — no `qlmanage -r` needed
- The separate `GISTypes.app` UTI stub can be eliminated; UTI declarations move into
  the host app's `Info.plist`
- Produces a single `.app` that users download, drag to Applications, and launch once

---

## Repository layout (new files only)

```
GISLookApp/
  GISLookApp.xcodeproj/       ← Xcode project (Swift/SwiftUI target)
  GISLookApp/
    GISLookApp.swift           ← @main entry point
    ContentView.swift          ← "GISLook is active" window
    Info.plist                 ← app Info.plist + UTI declarations (replaces GISTypes)
    Assets.xcassets/           ← app icon (can reuse GISTypes icon later)
```

Everything under `GISLook/`, `GISSource/`, `GISTypes/`, and the `Makefile` stays
untouched. The Xcode project calls `make` as a build phase to produce the plugin,
then copies it into the app bundle.

---

## Step-by-step

### 1. Create the Xcode project (~20 min)

- New project → macOS → App → Swift, SwiftUI, no Core Data, no tests
- Name: `GISLookApp`, bundle ID: `com.yourname.GISLook` (or reverse-domain of your choice)
- Save into `GISLookApp/` inside the repo

### 2. Minimal UI (~15 min)

Replace the default `ContentView.swift`:

```swift
import SwiftUI

struct ContentView: View {
    let formats = [
        ("ESRI Shapefile",        ".shp"),
        ("ESRI ASCII Grid",       ".asc"),
        ("ESRI Binary Grid",      ".flt"),
        ("BIL / BIP / BSQ",       ".bil .bip .bsq"),
        ("ArcInfo Coverage",      ".adf"),
        ("ArcInfo E00",           ".e00"),
        ("USGS DEM",              ".dem"),
        ("SRTM",                  ".hgt"),
        ("Surfer Grid",           ".grd"),
        ("Portable Graymap",      ".pgm"),
    ]

    var body: some View {
        VStack(alignment: .leading, spacing: 16) {
            Label("GISLook is active", systemImage: "checkmark.seal.fill")
                .font(.title2.bold())
                .foregroundStyle(.green)

            Text("Press Space on any supported file in Finder to preview it.")
                .foregroundStyle(.secondary)

            Divider()

            Text("Supported formats").font(.headline)

            Grid(alignment: .leading, columnSpacing: 24, rowSpacing: 4) {
                ForEach(formats, id: \.0) { name, ext in
                    GridRow {
                        Text(name)
                        Text(ext).foregroundStyle(.secondary).font(.system(.body, design: .monospaced))
                    }
                }
            }
        }
        .padding(24)
        .frame(minWidth: 420)
    }
}
```

Set the window to non-resizable and hide the title bar if desired — SwiftUI `.windowStyle(.hiddenTitleBar)` in the `@main` App struct, or leave as default.

### 3. Move UTI declarations into the app's Info.plist (~20 min)

Copy the `UTImportedTypeDeclarations` and `CFBundleDocumentTypes` arrays from
`GISTypes/Info.plist` into `GISLookApp/GISLookApp/Info.plist`.

Change `UTImportedTypeDeclarations` → `UTExportedTypeDeclarations` for any types
that don't already have a system owner (`.shp`, `.adf`, `.hgt`, etc.).
`UTExportedTypeDeclarations` gives your app ownership of those types, which is
more correct for types the system doesn't know about.

### 4. Embed the plugin via a Run Script build phase (~20 min)

In Xcode → target → Build Phases → `+` → New Run Script Phase:

```sh
# Build the plugin (universal arm64+x86_64)
make -C "$SRCROOT/.." build

# Copy into the app bundle
PLUGIN_DST="$BUILT_PRODUCTS_DIR/$CONTENTS_FOLDER_PATH/Library/QuickLook"
mkdir -p "$PLUGIN_DST"
cp -R "$SRCROOT/../build/GISLook.qlgenerator" "$PLUGIN_DST/"
```

Set "Input Files" to `$(SRCROOT)/../build/GISLook.qlgenerator` so Xcode only
re-runs this phase when the plugin has changed.

The embedded `.qlgenerator` is signed automatically by Xcode as part of the
app's overall signing step — no separate codesign call needed.

### 5. Signing (~10 min)

- For local development: "Sign to Run Locally" or your Apple Development cert
- For distribution: switch to Developer ID Application cert
- The `--options runtime` flag (Hardened Runtime) must be set in Xcode:
  Signing & Capabilities → Hardened Runtime → enable

### 6. Update the repo Makefile (optional, ~15 min)

Add a convenience target that opens the Xcode project so the Makefile remains
the single entry point for contributors who want to build without opening Xcode:

```makefile
app:
	open GISLookApp/GISLookApp.xcodeproj

app-build:
	xcodebuild -project GISLookApp/GISLookApp.xcodeproj \
	           -scheme GISLookApp \
	           -configuration Release \
	           -archivePath build/GISLookApp.xcarchive \
	           archive
```

### 7. Remove GISTypes (cleanup, ~5 min)

Once the host app is working:
- Remove `GISTypes/` from the repo (UTIs are now in the host app)
- Remove the GISTypes build/install steps from the `Makefile`

---

## Distribution via GitHub Releases

Once notarization is set up, the release flow is:

1. Tag a release: `git tag v1.0.0 && git push --tags`
2. GitHub Actions workflow (`.github/workflows/release.yml`) triggers on tag push:
   - `xcodebuild archive`
   - `xcodebuild -exportArchive` with Developer ID export options
   - `xcrun notarytool submit` (using a stored App Store Connect API key secret)
   - `xcrun stapler staple`
   - Zip the `.app` and upload as a GitHub Release asset
3. Users download `GISLook-v1.0.0.zip`, unzip, drag `GISLook.app` to `/Applications`

A basic Actions workflow skeleton for this is ~50 lines of YAML and can be added
when ready to cut the first release.

---

## What stays the same

- All C source code in `GISLook/` and `GISSource/` — untouched
- The `make install` developer workflow for local testing — still works
- The `.qlgenerator` CFPlugIn architecture — no rewrite needed

---

## Estimated time

| Task | Time |
|------|------|
| Xcode project + minimal SwiftUI UI | 30–45 min |
| UTI declarations migration | 20 min |
| Run Script build phase + plugin embedding | 20 min |
| Signing + Hardened Runtime | 10 min |
| Makefile cleanup (remove GISTypes) | 15 min |
| **Total** | **~2 hours** |

GitHub Actions release pipeline is an additional ~1 hour when ready.
