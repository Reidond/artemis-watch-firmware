import SwiftUI

@main
struct ArtemisApp: App {
    @StateObject private var model = AppModel.shared
    @Environment(\.scenePhase) private var scenePhase

    init() {
        AppModel.registerBackgroundTasks { AppModel.shared }
    }

    var body: some Scene {
        WindowGroup {
            ContentView()
                .environmentObject(model)
                .tint(Palette.red)
                .preferredColorScheme(.dark)
        }
        .onChange(of: scenePhase) { _, phase in
            if phase == .background { model.scheduleWeatherRefresh() }
        }
    }
}

extension AppModel {
    /// One model for the app and its background tasks.
    static let shared = AppModel()
}
