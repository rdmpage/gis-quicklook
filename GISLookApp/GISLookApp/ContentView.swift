import SwiftUI

struct ContentView: View {

    let formats: [(String, String)] = [
        ("ESRI Shapefile",    ".shp"),
        ("ESRI ASCII Grid",   ".asc"),
        ("ESRI Binary Grid",  ".flt"),
        ("BIL / BIP / BSQ",   ".bil  .bip  .bsq"),
        ("ArcInfo Coverage",  ".adf"),
        ("ArcInfo E00",       ".e00"),
        ("USGS DEM",          ".dem"),
        ("SRTM",              ".hgt"),
        ("Surfer Grid",       ".grd"),
        ("Portable Graymap",  ".pgm"),
    ]

    var body: some View {
        VStack(alignment: .leading, spacing: 20) {

            // Status header
            HStack(spacing: 10) {
                Image(systemName: "checkmark.seal.fill")
                    .foregroundStyle(.green)
                    .font(.title)
                Text("GISLook is active")
                    .font(.title2.bold())
            }

            Text("Press Space on any supported file in Finder to preview it.")
                .foregroundStyle(.secondary)

            Divider()

            // Format table
            Text("Supported formats")
                .font(.headline)

            Grid(alignment: .leading, horizontalSpacing: 20, verticalSpacing: 5) {
                ForEach(formats, id: \.0) { name, ext in
                    GridRow {
                        Text(name)
                            .gridColumnAlignment(.leading)
                        Text(ext)
                            .foregroundStyle(.secondary)
                            .font(.system(.body, design: .monospaced))
                            .gridColumnAlignment(.leading)
                    }
                }
            }

            Divider()

            // Footer note
            Text("GISLook works whenever this app is installed in /Applications. It does not need to be running.")
                .font(.footnote)
                .foregroundStyle(.secondary)
        }
        .padding(24)
        .frame(minWidth: 420)
    }
}
