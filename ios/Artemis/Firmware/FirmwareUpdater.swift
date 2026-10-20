import CryptoKit
import Foundation
import os

/// An ESP32 app image (`firmware/build/artemis-fw.bin`).
struct FirmwareImage {
    let data: Data
    let version: String
    let sha256: String

    /// Reads the version from the image's app descriptor (esp_app_desc_t,
    /// right after the image and first segment headers).
    init?(data: Data) {
        let descOffset = 24 + 8
        guard data.count > descOffset + 48, data[0] == 0xE9 else { return nil }
        let magic = data.subdata(in: descOffset..<descOffset + 4).withUnsafeBytes { $0.load(as: UInt32.self) }
        guard magic == 0xABCD_5432 else { return nil }
        let versionBytes = data.subdata(in: descOffset + 16..<descOffset + 48)
        self.data = data
        version = String(decoding: versionBytes.prefix { $0 != 0 }, as: UTF8.self)
        sha256 = SHA256.hash(data: data).map { String(format: "%02x", $0) }.joined()
    }

    /// The image built into this app (Xcode build phase "Bundle watch firmware").
    static var bundled: FirmwareImage? {
        guard let url = Bundle.main.url(forResource: "artemis-fw", withExtension: "bin"),
            let data = try? Data(contentsOf: url)
        else { return nil }
        return FirmwareImage(data: data)
    }
}

/// Sends a firmware image to the watch (docs/BLE_PROTOCOL.md, Firmware update).
@MainActor
final class FirmwareUpdater: ObservableObject {
    enum Phase: Equatable {
        case idle
        case preparing  // watch erases its spare slot
        case sending(Double)
        case finishing  // watch verifies and restarts
        case done(String)  // new version
        case failed(String)
    }

    @Published private(set) var phase: Phase = .idle

    private let log = Logger(subsystem: "artemis", category: "ota")
    private let link: WatchLink
    private var image: FirmwareImage?
    private var offset = 0
    private var expectedVersion = ""

    init(link: WatchLink) {
        self.link = link
    }

    var isBusy: Bool {
        switch phase {
        case .preparing, .sending, .finishing: return true
        default: return false
        }
    }

    func start(_ image: FirmwareImage) {
        guard link.isConnected, !isBusy else { return }
        self.image = image
        offset = 0
        expectedVersion = image.version
        phase = .preparing
        link.onOtaMessage = { [weak self] in self?.handle($0) }
        link.onReadyToSend = { [weak self] in self?.pump() }
        link.otaControl(#"{"op":"begin","size":\#(image.data.count),"sha256":"\#(image.sha256)"}"#)
    }

    func cancel() {
        guard isBusy else { return }
        link.otaControl(#"{"op":"abort"}"#)
        finish(.idle)
    }

    /// Info arrives after the watch restarts: did the new version come up?
    func watchReported(version: String) {
        if case .finishing = phase, version == expectedVersion { finish(.done(version)) }
    }

    private func handle(_ msg: OtaMessage) {
        if msg.ok == false {
            // After "end" the watch restarts and the link drops: that is success.
            if case .finishing = phase, msg.op == "link" { return }
            finish(.failed(msg.err ?? "error"))
            return
        }
        switch msg.op {
        case "begin":
            phase = .sending(0)
            pump()
        case "end":
            phase = .finishing
        default:
            break
        }
    }

    /// Sends packets while CoreBluetooth has room; resumes from onReadyToSend.
    private func pump() {
        guard case .sending = phase, let image else { return }
        let packet = max(20, link.maxPacket)
        while offset < image.data.count {
            let end = min(offset + packet, image.data.count)
            guard link.sendOtaPacket(image.data.subdata(in: offset..<end)) else { break }
            offset = end
        }
        phase = .sending(Double(offset) / Double(image.data.count))
        if offset >= image.data.count {
            phase = .finishing
            link.otaControl(#"{"op":"end"}"#)
        }
    }

    private func finish(_ next: Phase) {
        phase = next
        image = nil
        link.onReadyToSend = nil
        if case .failed(let why) = next { log.error("update failed: \(why)") }
    }
}
