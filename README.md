# gis-quicklook

A macOS QuickLook plugin for GIS raster and vector files. Press Space in Finder to preview shapefiles, grids, DEMs, and more.

This is a modernized fork of [GISLook/GISMeta](https://github.com/berniejenny/GISLook-GISMeta) by Bernhard Jenny (Oregon State University), updated to build and run on macOS 13 Ventura and later with Apple Silicon support.

## Supported formats

| Format | Extension |
|---|---|
| ESRI Shapefile | `.shp` |
| ESRI ASCII Grid | `.asc` |
| ESRI Binary Grid | `.flt` |
| BIL / BIP / BSQ | `.bil` `.bip` `.bsq` |
| ArcInfo Coverage | `.adf` |
| ArcInfo E00 | `.e00` |
| USGS DEM | `.dem` |
| SRTM | `.hgt` |
| Surfer Grid | `.grd` |
| Portable Graymap | `.pgm` |

## Requirements

- macOS 13 Ventura or later
- Xcode (for building)

## Install

1. Clone the repo and open `GISLookApp/GISLookApp.xcodeproj` in Xcode
2. Build (⌘B)
3. Copy `GISLookApp.app` from the Xcode build output to `/Applications/`

That's it. The QuickLook plugin is embedded in the app bundle and registered automatically by macOS. The app does not need to be running for previews to work.

## Building the plugin for development

If you want to iterate on the C rendering code without going through Xcode each time:

```sh
make          # build only (output: build/GISLook.qlgenerator)
make install  # build + install directly to ~/Library/QuickLook/
make test TEST_FILE=/path/to/file.shp
make clean
```

`make install` is useful for rapid testing. For distribution, use the Xcode app.

## Repository layout

```
GISLook/        Plugin entry points (C): factory, preview, thumbnail callbacks
GISSource/      GIS format readers: shapelib, raster grids, E00, etc.
GISLookApp/     SwiftUI host app (embeds the plugin, registers UTIs)
examples/       Sample shapefile for testing
docs/           Development notes
```

## References

- Jenny, Bernhard. (2008). Previewing GIS data with GISlook on mac OS X. The bulletin of the Society of University Cartographers. Society of University Cartographers. 42. 49-50. 

## License

GNU General Public License v3. See [LICENSE](LICENSE).

Original work copyright © Bernhard Jenny, Oregon State University.
Modernization copyright © contributors to this repository.
