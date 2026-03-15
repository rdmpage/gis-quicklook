# gis-quicklook

A macOS QuickLook plugin for GIS raster and vector files. Press **Space** in Finder to preview shapefiles, grids, DEMs, and more.

This is a modernized fork of [GISLook/GISMeta](https://github.com/berniejenny/GISLook-GISMeta) by Bernhard Jenny (Oregon State University), updated to build and run on macOS 13 Ventura and later with Apple Silicon support.

## Supported formats

| Format | Extensions |
|--------|-----------|
| ESRI Shapefile | `.shp` `.dbf` `.shx` |
| ESRI ASCII Grid | `.asc` |
| ESRI Binary Grid | `.flt` |
| BIL / BIP / BSQ | `.bil` `.bip` `.bsq` |
| ArcInfo Coverage | `.adf` |
| ArcInfo E00 | `.e00` |
| GeoJSON | `.geojson` |
| USGS DEM | `.dem` |
| SRTM | `.hgt` |
| Surfer Grid | `.grd` |
| Portable Graymap | `.pgm` |

## Requirements

- macOS 13 Ventura or later (Apple Silicon or Intel)

## Installation

1. Download **GISLook-0.2.0.zip** from the [latest release](https://github.com/rdmpage/gis-quicklook/releases/latest) and unzip it
2. Remove the quarantine flag (required because the app is not notarised):
   ```
   xattr -cr GISLookApp.app
   ```
3. Drag **GISLookApp.app** to your `/Applications` folder
4. Open **GISLookApp** once to register the file types with Launch Services
5. Press **Space** on any supported GIS file in Finder

> **Why the `xattr` step?**
> The app is signed with an Apple Development certificate but has not been notarised through the Mac App Store review process. macOS Gatekeeper will block it until you remove the quarantine flag with `xattr -cr`. This is a one-time step.

## Building the app

The plugin consists of two parts built separately:

- **C rendering code** (`GISLook/`, `GISSource/`) — built with `make`
- **Swift host app** (`GISLookApp/`) — built with Xcode; embeds the plugin and registers the UTIs with macOS

### 1. Build the C plugin

```sh
make                      # build with default settings (THUMBNAIL_STYLE=0)
make THUMBNAIL_STYLE=1    # dark background variant
make clean                # remove build artefacts
```

Output: `build/GISLook.qlgenerator`

### 2. Build the host app

Open `GISLookApp/GISLookApp.xcodeproj` in Xcode and build (⌘B), or from the command line:

```sh
/Applications/Xcode.app/Contents/Developer/usr/bin/xcodebuild \
  -project GISLookApp/GISLookApp.xcodeproj \
  -scheme GISLookApp -configuration Release build
```

### 3. Copy the plugin into the app bundle

The Xcode project does not yet have a build phase for the C code, so the plugin binary must be copied in manually after each `make` run:

```sh
BUNDLE=~/Library/Developer/Xcode/DerivedData/GISLookApp-*/Build/Products/Release/GISLookApp.app/Contents/Library/QuickLook/GISLook.qlgenerator

cp build/GISLook.qlgenerator/Contents/MacOS/GISLook  $BUNDLE/Contents/MacOS/GISLook
cp build/GISLook.qlgenerator/Contents/Info.plist      $BUNDLE/Contents/Info.plist
codesign --force --sign "Apple Development: <your identity>" $BUNDLE
```

### 4. Test

```sh
qlmanage -t -s 256 -o /tmp examples/22-West_Tropical_Africa.geojson
```

### 5. Package for distribution

```sh
cd ~/Library/Developer/Xcode/DerivedData/GISLookApp-*/Build/Products/Release
zip -r GISLook-<version>.zip GISLookApp.app
```

## Repository layout

```
GISLook/        Plugin entry points (C): factory, preview, thumbnail callbacks
GISSource/      GIS format readers: shapelib, raster grids, E00, GeoJSON, etc.
GISLookApp/     SwiftUI host app (embeds the plugin, registers UTIs)
examples/       Sample GIS files for testing
docs/           Development notes
```

## References

- Jenny, Bernhard. (2008). Previewing GIS data with GISlook on mac OS X. The bulletin of the Society of University Cartographers. Society of University Cartographers. 42. 49-50.

## License

GNU General Public License v3. See [LICENSE](LICENSE).

Original work copyright © Bernhard Jenny, Oregon State University.
Modernization copyright © contributors to this repository.
