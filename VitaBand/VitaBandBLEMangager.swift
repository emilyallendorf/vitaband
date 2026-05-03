//
//  VitaBandBLEManager.swift
//  VitaBand
//
//  Created by Vidhi Challani on 2/10/26.
//

import Foundation
import CoreBluetooth
import Combine

enum VitaBandState: UInt8 {
    case ok = 0
    case warning = 1
    case critical = 2
    case emergency = 3

    var displayName: String {
        switch self {
        case .ok: return "OK"
        case .warning: return "WARNING"
        case .critical: return "CRITICAL"
        case .emergency: return "EMERGENCY"
        }
    }
}

struct VitaBandVitals {
    let heartRate: UInt8
    let bodyTempC: Float
    let ambientTempC: Float
    let state: VitaBandState
    let uptimeMs: UInt32
}

enum VitaBandParseError: Error {
    case invalidLength(Int)
    case invalidState(UInt8)
}

final class VitaBandBLEManager: NSObject, ObservableObject {
    @Published var connectionStatus: String = "Bluetooth starting..."
    @Published var isConnected: Bool = false

    @Published var latestVitals: VitaBandVitals?

    @Published var heartRate: Int = 0
    @Published var bodyTempC: Float = 0.0
    @Published var ambientTempC: Float = 0.0
    @Published var stateText: String = "Unknown"
    @Published var uptimeMs: UInt32 = 0

    private var centralManager: CBCentralManager!
    private var vitaBandPeripheral: CBPeripheral?
    private var vitalsCharacteristic: CBCharacteristic?

    private let serviceUUID = CBUUID(string: "8E7C3F40-1A6B-4F92-9D3C-5A1B2C7D8E01")
    private let vitalsCharacteristicUUID = CBUUID(string: "8E7C3F40-1A6B-4F92-9D3C-5A1B2C7D8E02")

    override init() {
        super.init()
        centralManager = CBCentralManager(delegate: self, queue: nil)
    }

    func startScan() {
        guard centralManager.state == .poweredOn else {
            connectionStatus = "Bluetooth is not powered on"
            return
        }

        connectionStatus = "Scanning for VitaBand..."
        centralManager.scanForPeripherals(withServices: [serviceUUID], options: nil)
    }

    func disconnect() {
        guard let peripheral = vitaBandPeripheral else { return }
        centralManager.cancelPeripheralConnection(peripheral)
    }

    private func resetPublishedVitals() {
        latestVitals = nil
        heartRate = 0
        bodyTempC = 0.0
        ambientTempC = 0.0
        stateText = "Unknown"
        uptimeMs = 0
    }

    private func applyVitals(_ vitals: VitaBandVitals) {
        latestVitals = vitals
        heartRate = Int(vitals.heartRate)
        bodyTempC = vitals.bodyTempC
        ambientTempC = vitals.ambientTempC
        stateText = vitals.state.displayName
        uptimeMs = vitals.uptimeMs
    }

    private func parseVitalsPacket(_ data: Data) throws -> VitaBandVitals {
        guard data.count == 14 else {
            throw VitaBandParseError.invalidLength(data.count)
        }

        let heartRate = data[0]
        let bodyTempC = readFloat32LittleEndian(from: data, at: 1)
        let ambientTempC = readFloat32LittleEndian(from: data, at: 5)

        let rawState = data[9]
        guard let state = VitaBandState(rawValue: rawState) else {
            throw VitaBandParseError.invalidState(rawState)
        }

        let uptimeMs = readUInt32LittleEndian(from: data, at: 10)

        return VitaBandVitals(
            heartRate: heartRate,
            bodyTempC: bodyTempC,
            ambientTempC: ambientTempC,
            state: state,
            uptimeMs: uptimeMs
        )
    }

    private func readUInt32LittleEndian(from data: Data, at offset: Int) -> UInt32 {
        let b0 = UInt32(data[offset])
        let b1 = UInt32(data[offset + 1]) << 8
        let b2 = UInt32(data[offset + 2]) << 16
        let b3 = UInt32(data[offset + 3]) << 24
        return b0 | b1 | b2 | b3
    }

    private func readFloat32LittleEndian(from data: Data, at offset: Int) -> Float {
        let bits = readUInt32LittleEndian(from: data, at: offset)
        return Float(bitPattern: bits)
    }
}

// MARK: - CBCentralManagerDelegate
extension VitaBandBLEManager: CBCentralManagerDelegate {
    func centralManagerDidUpdateState(_ central: CBCentralManager) {
        switch central.state {
        case .unknown:
            connectionStatus = "Bluetooth state: unknown"
        case .resetting:
            connectionStatus = "Bluetooth resetting..."
        case .unsupported:
            connectionStatus = "Bluetooth not supported on this device"
        case .unauthorized:
            connectionStatus = "Bluetooth permission not granted"
        case .poweredOff:
            connectionStatus = "Bluetooth is off"
        case .poweredOn:
            connectionStatus = "Bluetooth is on"
            startScan()
        @unknown default:
            connectionStatus = "Bluetooth state: unknown future case"
        }
    }

    func centralManager(_ central: CBCentralManager,
                        didDiscover peripheral: CBPeripheral,
                        advertisementData: [String : Any],
                        rssi RSSI: NSNumber) {
        connectionStatus = "Found VitaBand: \(peripheral.name ?? "Unnamed Device")"

        vitaBandPeripheral = peripheral
        vitaBandPeripheral?.delegate = self

        centralManager.stopScan()
        connectionStatus = "Connecting..."
        centralManager.connect(peripheral, options: nil)
    }

    func centralManager(_ central: CBCentralManager,
                        didConnect peripheral: CBPeripheral) {
        isConnected = true
        connectionStatus = "Connected. Discovering services..."
        peripheral.discoverServices([serviceUUID])
    }

    func centralManager(_ central: CBCentralManager,
                        didFailToConnect peripheral: CBPeripheral,
                        error: Error?) {
        isConnected = false
        connectionStatus = "Failed to connect: \(error?.localizedDescription ?? "Unknown error")"
    }

    func centralManager(_ central: CBCentralManager,
                        didDisconnectPeripheral peripheral: CBPeripheral,
                        error: Error?) {
        isConnected = false
        vitalsCharacteristic = nil
        vitaBandPeripheral = nil
        resetPublishedVitals()

        if let error = error {
            connectionStatus = "Disconnected: \(error.localizedDescription)"
        } else {
            connectionStatus = "Disconnected"
        }
    }
}

// MARK: - CBPeripheralDelegate
extension VitaBandBLEManager: CBPeripheralDelegate {
    func peripheral(_ peripheral: CBPeripheral,
                    didDiscoverServices error: Error?) {
        if let error = error {
            connectionStatus = "Service discovery failed: \(error.localizedDescription)"
            return
        }

        guard let services = peripheral.services else {
            connectionStatus = "No services found"
            return
        }

        for service in services {
            if service.uuid == serviceUUID {
                connectionStatus = "Service found. Discovering characteristic..."
                peripheral.discoverCharacteristics([vitalsCharacteristicUUID], for: service)
                return
            }
        }

        connectionStatus = "VitaBand service not found"
    }

    func peripheral(_ peripheral: CBPeripheral,
                    didDiscoverCharacteristicsFor service: CBService,
                    error: Error?) {
        if let error = error {
            connectionStatus = "Characteristic discovery failed: \(error.localizedDescription)"
            return
        }

        guard let characteristics = service.characteristics else {
            connectionStatus = "No characteristics found"
            return
        }

        for characteristic in characteristics {
            if characteristic.uuid == vitalsCharacteristicUUID {
                vitalsCharacteristic = characteristic
                connectionStatus = "Connected to VitaBand characteristic"

                peripheral.setNotifyValue(true, for: characteristic)
                peripheral.readValue(for: characteristic)
                return
            }
        }

        connectionStatus = "Vitals characteristic not found"
    }

    func peripheral(_ peripheral: CBPeripheral,
                    didUpdateNotificationStateFor characteristic: CBCharacteristic,
                    error: Error?) {
        if let error = error {
            connectionStatus = "Failed to enable notifications: \(error.localizedDescription)"
            return
        }

        if characteristic.uuid == vitalsCharacteristicUUID {
            if characteristic.isNotifying {
                connectionStatus = "Notifications enabled"
            } else {
                connectionStatus = "Notifications disabled"
            }
        }
    }

    func peripheral(_ peripheral: CBPeripheral,
                    didUpdateValueFor characteristic: CBCharacteristic,
                    error: Error?) {
        if let error = error {
            connectionStatus = "Value update failed: \(error.localizedDescription)"
            return
        }

        guard characteristic.uuid == vitalsCharacteristicUUID,
              let data = characteristic.value else {
            return
        }

        do {
            let vitals = try parseVitalsPacket(data)
            applyVitals(vitals)
            connectionStatus = "Received \(data.count) bytes"
            print("Parsed VitaBand vitals:")
            print("  HR: \(vitals.heartRate) bpm")
            print("  Body temp: \(vitals.bodyTempC) C")
            print("  Ambient temp: \(vitals.ambientTempC) C")
            print("  State: \(vitals.state.displayName)")
            print("  Uptime: \(vitals.uptimeMs) ms")
        } catch VitaBandParseError.invalidLength(let count) {
            connectionStatus = "Invalid packet length: \(count) bytes"
            print("Expected 14 bytes, got \(count)")
        } catch VitaBandParseError.invalidState(let rawValue) {
            connectionStatus = "Invalid state value: \(rawValue)"
            print("Invalid state raw value: \(rawValue)")
        } catch {
            connectionStatus = "Failed to parse vitals packet"
            print("Unexpected parse error: \(error)")
        }
    }
}
