import BackgroundTasks
import Combine
import Foundation
import UIKit
import os

/// Owns the watch link and services and keeps the watch fed: weather when it
/// is stale, the iPhone's time zone, and firmware when asked.
@MainActor
final class AppModel: ObservableObject {
    let link = WatchLink()
    let weather = WeatherService()
    lazy var firmware = FirmwareUpdater(link: link)

    @Published var autoWeather: Bool {
        didSet { UserDefaults.standard.set(autoWeather, forKey: "autoWeather") }
    }
    @Published private(set) var weatherSentAt: Date?

    private let log = Logger(subsystem: "artemis", category: "app")
    private var refreshing = false
    private var bag = Set<AnyCancellable>()

    static let weatherTaskID = (Bundle.main.bundleIdentifier ?? "artemis") + ".weather"
    /// Weather older than this gets refreshed when the watch checks in.
    private static let weatherMaxAge: TimeInterval = 50 * 60

    init() {
        autoWeather = UserDefaults.standard.object(forKey: "autoWeather") as? Bool ?? true
        link.onInfo = { [weak self] info in self?.watchCheckedIn(info) }
        // Forward nested changes so views observing the model update.
        link.objectWillChange.sink { [weak self] in self?.objectWillChange.send() }.store(in: &bag)
        weather.objectWillChange.sink { [weak self] in self?.objectWillChange.send() }.store(in: &bag)
    }

    var watchLanguage: String { link.settings.lang ?? "uk" }

    // MARK: Watch check-ins (also in the background)

    private func watchCheckedIn(_ info: WatchInfo) {
        firmware.watchReported(version: info.fw)
        syncTimeZone()
        let age = Date().timeIntervalSince1970 - TimeInterval(info.wx)
        if autoWeather, age > Self.weatherMaxAge {
            Task { await sendWeather() }
        }
    }

    /// Keeps the watch's DST rules in step with the iPhone's time zone.
    private func syncTimeZone() {
        let tz = PosixTimeZone.string(for: .current)
        if link.settings.tz != nil, link.settings.tz != tz {
            log.info("time zone -> \(tz)")
            link.update(WatchSettings(tz: tz))
        }
    }

    @discardableResult
    func sendWeather() async -> Bool {
        guard !refreshing else { return false }
        refreshing = true
        defer { refreshing = false }
        // Background wake-ups get a few seconds; ask for a little more.
        let task = UIApplication.shared.beginBackgroundTask(withName: "weather")
        defer { UIApplication.shared.endBackgroundTask(task) }
        guard let payload = await weather.refresh(language: watchLanguage) else { return false }
        link.send(weather: payload)
        weatherSentAt = Date()
        return true
    }

    // MARK: Background refresh

    static func registerBackgroundTasks(model: @escaping @MainActor () -> AppModel) {
        BGTaskScheduler.shared.register(forTaskWithIdentifier: weatherTaskID, using: .main) { task in
            MainActor.assumeIsolated {
                let app = model()
                app.scheduleWeatherRefresh()
                let work = Task { @MainActor in
                    let ok = await app.sendWeather()
                    task.setTaskCompleted(success: ok)
                }
                task.expirationHandler = { work.cancel() }
            }
        }
    }

    func scheduleWeatherRefresh() {
        let request = BGAppRefreshTaskRequest(identifier: Self.weatherTaskID)
        request.earliestBeginDate = Date(timeIntervalSinceNow: 60 * 60)
        try? BGTaskScheduler.shared.submit(request)
    }
}
