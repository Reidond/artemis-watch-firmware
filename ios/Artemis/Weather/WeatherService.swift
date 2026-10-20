import CoreLocation
import Foundation
import os

/// Current weather and the next hours from Open-Meteo (free, no account),
/// for the iPhone's location. The last location is remembered so refreshes
/// in the background, where the app may not read location, still work.
@MainActor
final class WeatherService: NSObject, ObservableObject {
    @Published private(set) var latest: WeatherPayload?
    @Published private(set) var lastError: String?
    @Published private(set) var authorization: CLAuthorizationStatus

    private let log = Logger(subsystem: "artemis", category: "weather")
    private let location = CLLocationManager()
    private var locationWaiters: [CheckedContinuation<CLLocation?, Never>] = []

    private static let latKey = "weatherLat", lonKey = "weatherLon"

    override init() {
        authorization = location.authorizationStatus
        super.init()
        location.delegate = self
        location.desiredAccuracy = kCLLocationAccuracyKilometer
    }

    func requestPermission() {
        location.requestWhenInUseAuthorization()
    }

    /// Fetches fresh weather; `language` is the watch's ("uk" or "en") for the place name.
    func refresh(language: String) async -> WeatherPayload? {
        guard let here = await currentLocation() else {
            lastError = String(localized: "Location unavailable")
            return nil
        }
        do {
            let payload = try await fetch(at: here, language: language)
            latest = payload
            lastError = nil
            return payload
        } catch {
            log.error("weather: \(error.localizedDescription)")
            lastError = error.localizedDescription
            return nil
        }
    }

    // MARK: Location

    private func currentLocation() async -> CLLocation? {
        if authorization == .authorizedWhenInUse || authorization == .authorizedAlways {
            let fresh: CLLocation? = await withCheckedContinuation { continuation in
                locationWaiters.append(continuation)
                if locationWaiters.count == 1 { location.requestLocation() }
            }
            if let fresh {
                UserDefaults.standard.set(fresh.coordinate.latitude, forKey: Self.latKey)
                UserDefaults.standard.set(fresh.coordinate.longitude, forKey: Self.lonKey)
                return fresh
            }
        }
        let d = UserDefaults.standard
        guard d.object(forKey: Self.latKey) != nil else { return nil }
        return CLLocation(latitude: d.double(forKey: Self.latKey), longitude: d.double(forKey: Self.lonKey))
    }

    private func finishLocation(_ value: CLLocation?) {
        let waiters = locationWaiters
        locationWaiters = []
        waiters.forEach { $0.resume(returning: value) }
    }

    // MARK: Open-Meteo

    // Open-Meteo leaves gaps as null, so every value is optional.
    private struct Forecast: Decodable {
        struct Current: Decodable {
            var temperature_2m: Double?
            var weather_code: Int?
        }
        struct Hourly: Decodable {
            var time: [String]
            var temperature_2m: [Double?]
            var weather_code: [Int?]
        }
        struct Daily: Decodable {
            var temperature_2m_max: [Double?]?
            var temperature_2m_min: [Double?]?
        }
        var current: Current?
        var hourly: Hourly?
        var daily: Daily?
    }

    private struct ApiError: Decodable, LocalizedError {
        var reason: String
        var errorDescription: String? { reason }
    }

    private func fetch(at here: CLLocation, language: String) async throws -> WeatherPayload {
        var url = URLComponents(string: "https://api.open-meteo.com/v1/forecast")!
        url.queryItems = [
            .init(name: "latitude", value: String(format: "%.3f", here.coordinate.latitude)),
            .init(name: "longitude", value: String(format: "%.3f", here.coordinate.longitude)),
            .init(name: "current", value: "temperature_2m,weather_code"),
            .init(name: "hourly", value: "temperature_2m,weather_code"),
            .init(name: "daily", value: "temperature_2m_max,temperature_2m_min"),
            .init(name: "timezone", value: "auto"),
            .init(name: "forecast_days", value: "2"),
        ]
        var request = URLRequest(url: url.url!)
        request.timeoutInterval = 15
        let (data, response) = try await URLSession.shared.data(for: request)
        if let http = response as? HTTPURLResponse, http.statusCode != 200 {
            throw (try? JSONDecoder().decode(ApiError.self, from: data)) ?? URLError(.badServerResponse)
        }
        let forecast: Forecast
        do {
            forecast = try JSONDecoder().decode(Forecast.self, from: data)
        } catch {
            log.error("forecast decode: \(String(describing: error)) body: \(String(decoding: data.prefix(400), as: UTF8.self))")
            throw error
        }

        // Hourly times are local ("2026-10-20T10:00"); take the next six from now.
        let hourNow = Calendar.current.component(.hour, from: Date())
        let today = Self.localDay.string(from: Date())
        var hours: [[Double]] = []
        var nowFromHourly: (Double, Int)?
        if let hourly = forecast.hourly {
            for (i, time) in hourly.time.enumerated() where hours.count < 6 {
                guard time.count >= 13, let hour = Int(time.dropFirst(11).prefix(2)),
                    i < hourly.temperature_2m.count, i < hourly.weather_code.count,
                    let t = hourly.temperature_2m[i], let c = hourly.weather_code[i]
                else { continue }
                let isToday = time.prefix(10) == Substring(today)
                if isToday && hour == hourNow { nowFromHourly = (t, c) }
                if !isToday || hour > hourNow { hours.append([Double(hour), t, Double(c)]) }
            }
        }

        guard let temp = forecast.current?.temperature_2m ?? nowFromHourly?.0,
            let code = forecast.current?.weather_code ?? nowFromHourly?.1
        else { throw URLError(.cannotParseResponse) }
        let hi = forecast.daily?.temperature_2m_max?.first.flatMap { $0 } ?? temp
        let lo = forecast.daily?.temperature_2m_min?.first.flatMap { $0 } ?? temp

        return WeatherPayload(
            ts: Int64(Date().timeIntervalSince1970),
            place: await placeName(here, language: language),
            temp: temp, code: code, hi: hi, lo: lo, hours: hours)
    }

    private static let localDay: DateFormatter = {
        let f = DateFormatter()
        f.calendar = Calendar(identifier: .gregorian)
        f.locale = Locale(identifier: "en_US_POSIX")
        f.dateFormat = "yyyy-MM-dd"
        return f
    }()

    private func placeName(_ here: CLLocation, language: String) async -> String {
        let locale = Locale(identifier: language == "uk" ? "uk_UA" : "en_US")
        let marks = try? await CLGeocoder().reverseGeocodeLocation(here, preferredLocale: locale)
        let mark = marks?.first
        return mark?.locality ?? mark?.subAdministrativeArea ?? mark?.administrativeArea ?? ""
    }
}

extension WeatherService: CLLocationManagerDelegate {
    nonisolated func locationManagerDidChangeAuthorization(_ manager: CLLocationManager) {
        MainActor.assumeIsolated { authorization = manager.authorizationStatus }
    }

    nonisolated func locationManager(_ manager: CLLocationManager, didUpdateLocations locations: [CLLocation]) {
        MainActor.assumeIsolated { finishLocation(locations.last) }
    }

    nonisolated func locationManager(_ manager: CLLocationManager, didFailWithError error: Error) {
        MainActor.assumeIsolated { finishLocation(nil) }
    }
}
