import CoreBluetooth
import Foundation
import os

/// The BLE connection to the watch. The watch is already bonded with the
/// iPhone (it uses Apple's notification and media services), so the app joins
/// that link: it finds the watch among connected peripherals, or by scanning,
/// and keeps a pending connection open so iOS reconnects it in the background.
@MainActor
final class WatchLink: NSObject, ObservableObject {
    enum State: Equatable {
        case bluetoothOff, unauthorized, searching, connecting, connected
    }

    @Published private(set) var state: State = .searching
    @Published private(set) var info: WatchInfo?
    @Published private(set) var settings = WatchSettings()
    @Published private(set) var lastSeen: Date?

    /// Called with each info notification (also in the background).
    var onInfo: ((WatchInfo) -> Void)?
    /// OTA control notifications, for the firmware updater.
    var onOtaMessage: ((OtaMessage) -> Void)?
    /// The link is ready for more write-without-response packets.
    var onReadyToSend: (() -> Void)?

    private let log = Logger(subsystem: "artemis", category: "link")
    private var central: CBCentralManager!
    private var peripheral: CBPeripheral?
    private var characteristics: [CBUUID: CBCharacteristic] = [:]
    private var pendingWrites: [CBUUID: Data] = [:]

    private static let restoreID = "artemis.central"
    private static let savedPeripheralKey = "watchPeripheralID"

    override init() {
        super.init()
        central = CBCentralManager(
            delegate: self, queue: .main,
            options: [CBCentralManagerOptionRestoreIdentifierKey: Self.restoreID])
    }

    var isConnected: Bool { state == .connected }

    // MARK: Requests

    /// Sends changed settings. The UI shows them right away; the watch
    /// confirms by notifying its full settings back.
    func update(_ change: WatchSettings) {
        guard let data = WatchJSON.encode(change) else { return }
        settings.merge(change)
        write(data, to: ArtemisUUID.settings)
    }

    func send(weather: WeatherPayload) {
        guard let data = WatchJSON.encode(weather) else { return }
        write(data, to: ArtemisUUID.weather)
    }

    func findWatch() {
        write(Data(#"{"op":"find"}"#.utf8), to: ArtemisUUID.command)
    }

    func otaControl(_ json: String) {
        write(Data(json.utf8), to: ArtemisUUID.otaControl)
    }

    /// Largest OTA data packet the link takes.
    var maxPacket: Int { peripheral?.maximumWriteValueLength(for: .withoutResponse) ?? 20 }

    /// Sends one OTA data packet if the link has room; false means wait for onReadyToSend.
    func sendOtaPacket(_ data: Data) -> Bool {
        guard let p = peripheral, let chr = characteristics[ArtemisUUID.otaData], p.canSendWriteWithoutResponse
        else { return false }
        p.writeValue(data, for: chr, type: .withoutResponse)
        return true
    }

    func refreshInfo() {
        guard let p = peripheral, let chr = characteristics[ArtemisUUID.info] else { return }
        p.readValue(for: chr)
    }

    private func write(_ data: Data, to uuid: CBUUID) {
        guard let p = peripheral, let chr = characteristics[uuid] else {
            // Not ready yet: the latest value goes out once characteristics are found.
            pendingWrites[uuid] = data
            return
        }
        p.writeValue(data, for: chr, type: .withResponse)
    }

    // MARK: Finding the watch

    private func findWatch(centralReady: Bool) {
        guard centralReady, peripheral == nil else { return }
        if let id = UserDefaults.standard.string(forKey: Self.savedPeripheralKey).flatMap(UUID.init),
            let known = central.retrievePeripherals(withIdentifiers: [id]).first
        {
            connect(known)
            return
        }
        if let connected = central.retrieveConnectedPeripherals(withServices: [ArtemisUUID.service]).first {
            connect(connected)
            return
        }
        state = .searching
        central.scanForPeripherals(withServices: [ArtemisUUID.service])
    }

    private func connect(_ p: CBPeripheral) {
        central.stopScan()
        peripheral = p
        p.delegate = self
        UserDefaults.standard.set(p.identifier.uuidString, forKey: Self.savedPeripheralKey)
        state = .connecting
        // No timeout: iOS completes this whenever the watch is in range, even
        // with the app in the background.
        central.connect(p)
    }

    /// The saved watch is not the one: forget it and search again.
    func forgetWatch() {
        if let p = peripheral { central.cancelPeripheralConnection(p) }
        peripheral = nil
        characteristics = [:]
        info = nil
        UserDefaults.standard.removeObject(forKey: Self.savedPeripheralKey)
        findWatch(centralReady: central.state == .poweredOn)
    }
}

// MARK: - CBCentralManagerDelegate

extension WatchLink: CBCentralManagerDelegate {
    nonisolated func centralManagerDidUpdateState(_ central: CBCentralManager) {
        MainActor.assumeIsolated {
            switch central.state {
            case .poweredOn:
                if let p = peripheral {
                    // Restored by iOS: pick the link up where it is.
                    state = .connecting
                    if p.state == .connected { p.discoverServices([ArtemisUUID.service]) } else { central.connect(p) }
                } else {
                    findWatch(centralReady: true)
                }
            case .unauthorized: state = .unauthorized
            default: state = .bluetoothOff
            }
        }
    }

    nonisolated func centralManager(_ central: CBCentralManager, willRestoreState dict: [String: Any]) {
        MainActor.assumeIsolated {
            let restored = dict[CBCentralManagerRestoredStatePeripheralsKey] as? [CBPeripheral] ?? []
            // Bluetooth is not powered on yet here; centralManagerDidUpdateState
            // resumes the link.
            if let p = restored.first {
                peripheral = p
                p.delegate = self
                state = .connecting
            }
        }
    }

    nonisolated func centralManager(
        _ central: CBCentralManager, didDiscover p: CBPeripheral, advertisementData: [String: Any], rssi: NSNumber
    ) {
        MainActor.assumeIsolated { connect(p) }
    }

    nonisolated func centralManager(_ central: CBCentralManager, didConnect p: CBPeripheral) {
        MainActor.assumeIsolated {
            log.info("connected")
            p.discoverServices([ArtemisUUID.service])
        }
    }

    nonisolated func centralManager(_ central: CBCentralManager, didFailToConnect p: CBPeripheral, error: Error?) {
        MainActor.assumeIsolated {
            log.error("connect failed: \(error?.localizedDescription ?? "-")")
            central.connect(p)
        }
    }

    nonisolated func centralManager(
        _ central: CBCentralManager, didDisconnectPeripheral p: CBPeripheral, error: Error?
    ) {
        MainActor.assumeIsolated {
            log.info("disconnected")
            characteristics = [:]
            state = .connecting
            onOtaMessage?(OtaMessage(ok: false, op: "link", err: "disconnected", recv: nil))
            central.connect(p)
        }
    }
}

// MARK: - CBPeripheralDelegate

extension WatchLink: CBPeripheralDelegate {
    nonisolated func peripheral(_ p: CBPeripheral, didDiscoverServices error: Error?) {
        MainActor.assumeIsolated {
            guard let service = p.services?.first(where: { $0.uuid == ArtemisUUID.service }) else {
                log.error("Artemis service missing; watch firmware too old?")
                return
            }
            p.discoverCharacteristics(ArtemisUUID.all, for: service)
        }
    }

    nonisolated func peripheral(_ p: CBPeripheral, didDiscoverCharacteristicsFor service: CBService, error: Error?) {
        MainActor.assumeIsolated {
            for chr in service.characteristics ?? [] {
                characteristics[chr.uuid] = chr
                if [ArtemisUUID.info, ArtemisUUID.settings, ArtemisUUID.otaControl].contains(chr.uuid) {
                    p.setNotifyValue(true, for: chr)
                }
            }
            if let chr = characteristics[ArtemisUUID.settings] { p.readValue(for: chr) }
            if let chr = characteristics[ArtemisUUID.info] { p.readValue(for: chr) }
            state = .connected
            for (uuid, data) in pendingWrites { write(data, to: uuid) }
            pendingWrites = [:]
        }
    }

    nonisolated func peripheral(_ p: CBPeripheral, didUpdateValueFor chr: CBCharacteristic, error: Error?) {
        MainActor.assumeIsolated {
            guard let data = chr.value else { return }
            lastSeen = Date()
            switch chr.uuid {
            case ArtemisUUID.info:
                if let value = WatchJSON.decode(WatchInfo.self, data) {
                    info = value
                    onInfo?(value)
                }
            case ArtemisUUID.settings:
                if let value = WatchJSON.decode(WatchSettings.self, data) { settings = value }
            case ArtemisUUID.otaControl:
                if let value = WatchJSON.decode(OtaMessage.self, data) { onOtaMessage?(value) }
            default:
                break
            }
        }
    }

    nonisolated func peripheral(_ p: CBPeripheral, didWriteValueFor chr: CBCharacteristic, error: Error?) {
        MainActor.assumeIsolated {
            if let error { log.error("write \(chr.uuid): \(error.localizedDescription)") }
        }
    }

    nonisolated func peripheralIsReady(toSendWriteWithoutResponse p: CBPeripheral) {
        MainActor.assumeIsolated { onReadyToSend?() }
    }
}
