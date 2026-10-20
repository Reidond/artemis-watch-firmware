import SwiftUI
import UniformTypeIdentifiers

struct ContentView: View {
    @EnvironmentObject private var model: AppModel

    var body: some View {
        NavigationStack {
            List {
                StatusSection()
                ActionsSection()
                SettingsSection()
                WeatherSection()
                FirmwareSection()
            }
            .scrollContentBackground(.hidden)
            .background(Palette.black)
            .fontDesign(.monospaced)
            .navigationTitle("Artemis")
        }
    }
}

// MARK: - Status

private struct StatusSection: View {
    @EnvironmentObject private var model: AppModel

    var body: some View {
        Section {
            HStack(alignment: .firstTextBaseline) {
                Circle().fill(dotColor).frame(width: 8, height: 8)
                Text(stateText).font(.headline)
                Spacer()
                if let info = model.link.info {
                    Text(verbatim: "T+\(String(format: "%03d", info.day))")
                        .font(.headline).foregroundStyle(Palette.red)
                }
            }
            if let info = model.link.info {
                LabeledContent("Battery", value: batteryText(info))
                LabeledContent("Firmware", value: info.fw)
                LabeledContent("Board", value: "rev \(info.hw)")
            }
            if model.link.state == .searching {
                Text("Pair the watch in Settings → Bluetooth first, then keep it close.")
                    .font(.footnote).foregroundStyle(Palette.grey)
            }
        }
        .panel()
    }

    private var dotColor: Color {
        model.link.state == .connected ? Palette.white : Palette.red
    }

    private var stateText: LocalizedStringKey {
        switch model.link.state {
        case .connected: return "Connected"
        case .connecting: return "Connecting…"
        case .searching: return "Looking for the watch…"
        case .bluetoothOff: return "Bluetooth is off"
        case .unauthorized: return "Bluetooth access needed"
        }
    }

    private func batteryText(_ info: WatchInfo) -> String {
        switch info.charge {
        case .charging: return String(localized: "Charging")
        case .full: return String(localized: "Charged")
        case .unplugged: return info.bat >= 0 ? "\(info.bat)%" : "—"
        }
    }
}

// MARK: - Actions

private struct ActionsSection: View {
    @EnvironmentObject private var model: AppModel

    var body: some View {
        Section {
            Button {
                model.link.findWatch()
            } label: {
                Label("Find watch", systemImage: "dot.radiowaves.left.and.right")
            }
            .disabled(!model.link.isConnected)
        }
        .panel()
    }
}

// MARK: - Watch settings

private struct SettingsSection: View {
    @EnvironmentObject private var model: AppModel

    var body: some View {
        let s = model.link.settings
        Section("Watch") {
            Picker("Language", selection: bind(s.lang ?? "uk") { WatchSettings(lang: $0) }) {
                Text(verbatim: "Українська").tag("uk")
                Text(verbatim: "English").tag("en")
            }
            Picker("Brightness", selection: bind(s.bright ?? 60) { WatchSettings(bright: $0) }) {
                ForEach(WatchSettings.brightnessSteps, id: \.self) { Text(verbatim: "\($0)%").tag($0) }
            }
            Picker("Screen timeout", selection: bind(s.timeout ?? 15) { WatchSettings(timeout: $0) }) {
                ForEach(WatchSettings.timeouts, id: \.self) { Text(timeoutText($0)).tag($0) }
            }
            Toggle("24-hour clock", isOn: bind(s.h24 ?? true) { WatchSettings(h24: $0) })
            Toggle("Flip screen", isOn: bind(s.flip ?? false) { WatchSettings(flip: $0) })
            Toggle("Sounds", isOn: bind(s.sound ?? true) { WatchSettings(sound: $0) })
            DatePicker(
                "T+ counts from",
                selection: bind(Date(timeIntervalSince1970: TimeInterval(s.epoch ?? WatchSettings.artemisIILaunch))) {
                    WatchSettings(epoch: Int64($0.timeIntervalSince1970))
                },
                displayedComponents: [.date, .hourAndMinute])
            if s.epoch != WatchSettings.artemisIILaunch {
                Button("Count from the Artemis II launch") {
                    model.link.update(WatchSettings(epoch: WatchSettings.artemisIILaunch))
                }
            }
        }
        .disabled(!model.link.isConnected)
        .panel()
    }

    /// Binding that sends a one-key settings change to the watch.
    private func bind<T>(_ value: T, _ change: @escaping (T) -> WatchSettings) -> Binding<T> {
        Binding(get: { value }, set: { model.link.update(change($0)) })
    }

    private func timeoutText(_ seconds: Int) -> String {
        seconds < 60
            ? String(localized: "\(seconds) s")
            : String(localized: "\(seconds / 60) min")
    }
}

// MARK: - Weather

private struct WeatherSection: View {
    @EnvironmentObject private var model: AppModel
    @State private var sending = false

    var body: some View {
        Section("Weather") {
            if let w = model.weather.latest {
                HStack(spacing: 14) {
                    Image(systemName: WeatherCode.symbol(w.code)).font(.title).frame(width: 36)
                    VStack(alignment: .leading, spacing: 2) {
                        Text(verbatim: "\(Int(w.temp.rounded()))°  \(w.place)").font(.headline)
                        Text(WeatherCode.text(w.code)).foregroundStyle(Palette.grey)
                    }
                    Spacer()
                    Text(verbatim: "↑\(Int(w.hi.rounded()))° ↓\(Int(w.lo.rounded()))°").foregroundStyle(Palette.grey)
                }
            }
            if let error = model.weather.lastError {
                Text(error).font(.footnote).foregroundStyle(Palette.red)
            }
            if model.weather.authorization == .notDetermined {
                Button("Allow location for weather") { model.weather.requestPermission() }
            }
            Toggle("Update automatically", isOn: $model.autoWeather)
            Button {
                sending = true
                Task {
                    await model.sendWeather()
                    sending = false
                }
            } label: {
                HStack {
                    Label("Send weather now", systemImage: "arrow.up.circle")
                    Spacer()
                    if sending { ProgressView() }
                }
            }
            .disabled(!model.link.isConnected || sending)
            if let sent = model.weatherSentAt {
                Text("Sent \(sent.formatted(date: .omitted, time: .shortened))")
                    .font(.footnote).foregroundStyle(Palette.grey)
            }
            // Open-Meteo data is CC BY 4.0: credit it.
            Link("Weather data: Open-Meteo.com", destination: URL(string: "https://open-meteo.com")!)
                .font(.footnote).foregroundStyle(Palette.grey)
        }
        .panel()
    }
}

/// WMO weather codes (Open-Meteo), as the watch maps them.
enum WeatherCode {
    static func text(_ code: Int) -> LocalizedStringKey {
        switch code {
        case 0: return "Clear"
        case 1: return "Mainly clear"
        case 2: return "Partly cloudy"
        case 3: return "Overcast"
        case 45, 48: return "Fog"
        case 51...55: return "Drizzle"
        case 56, 57, 66, 67: return "Freezing rain"
        case 61...65: return "Rain"
        case 71...77, 85, 86: return "Snow"
        case 80...82: return "Showers"
        case 95...: return "Thunderstorm"
        default: return "Overcast"
        }
    }

    static func symbol(_ code: Int) -> String {
        switch code {
        case 0, 1: return "sun.max"
        case 2: return "cloud.sun"
        case 45, 48: return "cloud.fog"
        case 51...67, 80...82: return "cloud.rain"
        case 71...77, 85, 86: return "cloud.snow"
        case 95...: return "cloud.bolt"
        default: return "cloud"
        }
    }
}

// MARK: - Firmware

private struct FirmwareSection: View {
    @EnvironmentObject private var model: AppModel
    @State private var importing = false
    @State private var imported: FirmwareImage?
    @State private var importError = false

    private var candidate: FirmwareImage? { imported ?? FirmwareImage.bundled }

    var body: some View {
        Section("Firmware") {
            if let image = candidate {
                LabeledContent(imported == nil ? "In this app" : "Imported file", value: image.version)
                if let info = model.link.info {
                    LabeledContent("On the watch", value: info.fw)
                }
                progress
                Button {
                    model.firmware.start(image)
                } label: {
                    Label("Update watch to \(image.version)", systemImage: "arrow.down.circle")
                }
                .disabled(!model.link.isConnected || model.firmware.isBusy)
            } else {
                Text("No firmware in this build.").foregroundStyle(Palette.grey)
            }
            Button("Choose a firmware file…") { importing = true }
                .disabled(model.firmware.isBusy)
        }
        .panel()
        .fileImporter(isPresented: $importing, allowedContentTypes: [.data]) { result in
            guard case .success(let url) = result else { return }
            let access = url.startAccessingSecurityScopedResource()
            defer { if access { url.stopAccessingSecurityScopedResource() } }
            if let data = try? Data(contentsOf: url), let image = FirmwareImage(data: data) {
                imported = image
            } else {
                importError = true
            }
        }
        .alert("Not a watch firmware image", isPresented: $importError) {}
    }

    @ViewBuilder private var progress: some View {
        switch model.firmware.phase {
        case .idle:
            EmptyView()
        case .preparing:
            Text("The watch is preparing…").foregroundStyle(Palette.grey)
        case .sending(let p):
            VStack(alignment: .leading, spacing: 6) {
                DitherProgress(value: p)
                HStack {
                    Text(verbatim: "\(Int(p * 100))%")
                    Spacer()
                    Button("Cancel", role: .cancel) { model.firmware.cancel() }
                }
            }
        case .finishing:
            Text("Verifying and restarting the watch…").foregroundStyle(Palette.grey)
        case .done(let version):
            Text("Watch updated to \(version)").foregroundStyle(Palette.white)
        case .failed(let why):
            Text("Update failed: \(why)").foregroundStyle(Palette.red)
        }
    }
}
