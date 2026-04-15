//
//  VitaBandManager.swift
//  VitaBand
//
//  Created by Vidhi Challani on 2/10/26.
//
import Foundation
import Combine

final class VitaBandManager: ObservableObject {
    static let shared = VitaBandManager()

    private var timer: Timer?
    weak var sessionData: SessionData?

    func attachSession(_ session: SessionData) {
        self.sessionData = session
    }
    func calibrateBaseline() {
        guard let sessionData else { return }

        //Mock baseline vals
        sessionData.baselineHeartRate = Double.random(in: 72...82)
        sessionData.baselineBodyTemp = Double.random(in: 98.2...98.9)

        sessionData.currentHeartRate = sessionData.baselineHeartRate
        sessionData.currentBodyTemp = sessionData.baselineBodyTemp
        sessionData.ambientTemp = Double.random(in: 72...80)
        sessionData.currentState = .ok
    }
    func startSimulation() {
        stopSimulation()

        timer = Timer.scheduledTimer(withTimeInterval: 5.0, repeats: true) { _ in
            DispatchQueue.main.async {
                self.generateSimulatedReading()
            }
        }
    }
    func stopSimulation() {
        timer?.invalidate()
        timer = nil
    }
    private func generateSimulatedReading() {
        guard let sessionData else { return }

        let baselineHR = sessionData.baselineHeartRate
        let baselineTemp = sessionData.baselineBodyTemp

        let roll = Double.random(in: 0...1)

        if roll < 0.70 {
            //70% chance: OK
            sessionData.currentHeartRate = baselineHR + Double.random(in: -3...10)
            sessionData.currentBodyTemp = baselineTemp + Double.random(in: -0.2...0.8)
            sessionData.ambientTemp = Double.random(in: 72...84)
            sessionData.currentState = .ok

        } else if roll < 0.95 {
            //25% chance: WARNING
            sessionData.currentHeartRate = baselineHR + Double.random(in: 18...32)
            sessionData.currentBodyTemp = baselineTemp + Double.random(in: 1.0...2.0)
            sessionData.ambientTemp = Double.random(in: 84...95)
            sessionData.currentState = .warning

        } else {
            //5% chance: CRITICAL
            sessionData.currentHeartRate = baselineHR + Double.random(in: 38...52)
            sessionData.currentBodyTemp = baselineTemp + Double.random(in: 2.3...3.2)
            sessionData.ambientTemp = Double.random(in: 92...101)
            sessionData.currentState = .critical
        }
    }
    func simulateSpecificState(_ state: UserState) {
        guard let sessionData else { return }

        let baselineHR = sessionData.baselineHeartRate
        let baselineTemp = sessionData.baselineBodyTemp

        switch state {
        case .ok:
            sessionData.currentState = .ok
            sessionData.currentHeartRate = baselineHR + 4
            sessionData.currentBodyTemp = baselineTemp + 0.3
            sessionData.ambientTemp = 76

        case .warning:
            sessionData.currentState = .warning
            sessionData.currentHeartRate = baselineHR + 24
            sessionData.currentBodyTemp = baselineTemp + 1.4
            sessionData.ambientTemp = 88

        case .critical:
            sessionData.currentState = .critical
            sessionData.currentHeartRate = baselineHR + 45
            sessionData.currentBodyTemp = baselineTemp + 2.7
            sessionData.ambientTemp = 96

        case .emergency:
            // Never use emergency in demo mode
            sessionData.currentState = .critical
            sessionData.currentHeartRate = baselineHR + 45
            sessionData.currentBodyTemp = baselineTemp + 2.7
            sessionData.ambientTemp = 96
        }
    }
}
