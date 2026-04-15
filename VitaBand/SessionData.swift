//
//  SessionData.swift
//  VitaBand
//
//  Created by Vidhi Challani on 1/10/26.
//
import Foundation
import Combine

enum UserState: String, CaseIterable {
    case ok = "OK"
    case warning = "WARNING"
    case critical = "CRITICAL"
    case emergency = "EMERGENCY"
}

struct EmergencyContact: Identifiable, Hashable {
    let id = UUID()
    var name: String
    var phoneNumber: String
}

final class SessionData: ObservableObject {
    @Published var contacts: [EmergencyContact] = []

    @Published var baselineHeartRate: Double = 0
    @Published var baselineBodyTemp: Double = 0

    @Published var currentHeartRate: Double = 0
    @Published var currentBodyTemp: Double = 0
    @Published var ambientTemp: Double = 0

    @Published var currentState: UserState = .ok
}

