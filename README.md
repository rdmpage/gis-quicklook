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
- Xcode Command Line Tools (`xcode-select --install`)

## Build and install

```sh
make install
```

This builds a universal binary (arm64 + x86_64), installs it to `~/Library/QuickLook/`, and resets the QuickLook cache.

You may need to log out and back in for Finder icon thumbnails to refresh.

## Other make targets

```sh
make              # build only (output: build/GISLook.qlgenerator)
make install      # build + install to ~/Library/QuickLook/
make uninstall    # remove from ~/Library/QuickLook/
make test TEST_FILE=/path/to/file.shp   # preview a specific file
make clean        # remove build artifacts
```

## Testing without installing

```sh
make test TEST_FILE=/path/to/your/data.shp
```

This runs `qlmanage` directly against the built plugin so you can test without installing.

## License

GNU General Public License v3. See [LICENSE](LICENSE).

Original work copyright © Bernhard Jenny, Oregon State University.
Modernization copyright © contributors to this repository.
