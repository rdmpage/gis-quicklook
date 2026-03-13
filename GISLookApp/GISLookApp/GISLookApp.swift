import SwiftUI

@main
struct GISLookApp: App {
    var body: some Scene {
        WindowGroup {
            ContentView()
        }
        .windowResizability(.contentSize)
        .defaultSize(width: 460, height: 420)
    }
}
