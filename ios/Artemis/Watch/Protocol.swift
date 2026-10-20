import CoreBluetooth
import Foundation

/// "Artemis Control" GATT service, see docs/BLE_PROTOCOL.md.
enum ArtemisUUID {
    private static func uuid(_ n: UInt8) -> CBUUID {
        CBUUID(string: String(format: "9B2A00%02X-6E1D-4F3C-B8A5-2C7D1E0F4A93", n))
    }

    static let service = uuid(0x00)
    static let info = uuid(0x01)
    static let settings = uuid(0x02)
    static let weather = uuid(0x03)
    static let command = uuid(0x04)
    static let otaControl = uuid(0x05)
    static let otaData = uuid(0x06)

    static let all = [info, settings, weather, command, otaControl, otaData]
}

/// Info characteristic: what the watch reports about itself.
struct WatchInfo: Codable, Equatable {
    var fw: String
    var hw: Int
    var bat: Int
    var mv: Int
    var chg: Int
    var t: Int64
    var wx: Int64
    var day: Int

    enum Charge { case unplugged, charging, full }
    var charge: Charge { chg == 1 ? .charging : chg == 2 ? .full : .unplugged }
}

/// Settings characteristic. Every key is optional so the app can send a subset.
struct WatchSettings: Codable, Equatable {
    var lang: String?
    var bright: Int?
    var timeout: Int?
    var h24: Bool?
    var flip: Bool?
    var sound: Bool?
    var tz: String?
    var epoch: Int64?

    mutating func merge(_ o: WatchSettings) {
        lang = o.lang ?? lang
        bright = o.bright ?? bright
        timeout = o.timeout ?? timeout
        h24 = o.h24 ?? h24
        flip = o.flip ?? flip
        sound = o.sound ?? sound
        tz = o.tz ?? tz
        epoch = o.epoch ?? epoch
    }

    static let brightnessSteps = [20, 40, 60, 80, 100]
    static let timeouts = [5, 10, 15, 30, 60, 120]
    /// Artemis II liftoff, 2026-04-01 22:35 UTC: the default T+ counter start.
    static let artemisIILaunch: Int64 = 1_775_082_900
}

/// Weather characteristic payload.
struct WeatherPayload: Encodable, Equatable {
    var ts: Int64
    var place: String
    var temp: Double
    var code: Int
    var hi: Double
    var lo: Double
    /// [local hour, °C, WMO code]
    var hours: [[Double]]
}

/// OTA control notifications from the watch.
struct OtaMessage: Decodable {
    var ok: Bool?
    var op: String?
    var err: String?
    var recv: Int?
}

enum WatchJSON {
    static let encoder: JSONEncoder = {
        let e = JSONEncoder()
        e.outputFormatting = [.withoutEscapingSlashes]
        return e
    }()

    static func encode<T: Encodable>(_ value: T) -> Data? { try? encoder.encode(value) }
    static func decode<T: Decodable>(_ type: T.Type, _ data: Data) -> T? { try? JSONDecoder().decode(type, from: data) }
}
