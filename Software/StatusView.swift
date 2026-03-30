//
//  StatusView.swift
//  VitaBand
//
//  Created by Vidhi Challani on 2/10/26.
//
import SwiftUI

struct StatusView: View {
    @EnvironmentObject var sessionData: SessionData
    @State private var isSimulating = true

    var body: some View {
        VStack(spacing: 24) {
            Text("Current Status")
                .font(.largeTitle)
                .bold()
                .foregroundColor(.black)

            Text(sessionData.currentState.rawValue)
                .font(.system(size: 36, weight: .bold))
                .foregroundColor(.black)

            VStack(spacing: 12) {
                dataRow(
                    title: "Current Heart Rate",
                    value: String(format: "%.1f bpm", sessionData.currentHeartRate)
                )

                dataRow(
                    title: "Current Body Temperature",
                    value: String(format: "%.1f °F", sessionData.currentBodyTemp)
                )

                dataRow(
                    title: "Ambient Temperature",
                    value: String(format: "%.1f °F", sessionData.ambientTemp)
                )
            }
            .padding()
            .frame(maxWidth: .infinity)
            .background(Color.white.opacity(0.35))
            .cornerRadius(12)

            VStack(spacing: 12) {
                Text("Demo Controls")
                    .font(.headline)
                    .foregroundColor(.black)

                HStack(spacing: 12) {
                    Button("OK") {
                        VitaBandManager.shared.simulateSpecificState(.ok)
                    }
                    .buttonStyle(.bordered)

                    Button("Warning") {
                        VitaBandManager.shared.simulateSpecificState(.warning)
                    }
                    .buttonStyle(.bordered)

                    Button("Critical") {
                        VitaBandManager.shared.simulateSpecificState(.critical)
                    }
                    .buttonStyle(.bordered)
                }

                Button(isSimulating ? "Pause Simulation" : "Resume Simulation") {
                    isSimulating.toggle()

                    if isSimulating {
                        VitaBandManager.shared.startSimulation()
                    } else {
                        VitaBandManager.shared.stopSimulation()
                    }
                }
                .buttonStyle(.borderedProminent)
            }

            VStack(spacing: 12) {
                NavigationLink("Edit Contact List") {
                    ContactListView()
                }
                .buttonStyle(.bordered)

                NavigationLink("Recalibrate") {
                    CalibrationView()
                }
                .buttonStyle(.bordered)

                NavigationLink("View Data") {
                    DataView()
                }
                .buttonStyle(.borderedProminent)
            }

            Spacer()
        }
        .padding()
        .frame(maxWidth: .infinity, maxHeight: .infinity)
        .background(backgroundColor.ignoresSafeArea())
        .navigationTitle("Status")
        .navigationBarTitleDisplayMode(.inline)
        .onAppear {
            isSimulating = true
            VitaBandManager.shared.startSimulation()
        }
        .onDisappear {
            VitaBandManager.shared.stopSimulation()
        }
    }

    private var backgroundColor: Color {
        switch sessionData.currentState {
        case .ok:
            return Color(red: 0.85, green: 0.95, blue: 0.85)//pale green
        case .warning:
            return Color(red: 1.0, green: 0.97, blue: 0.75)//light yellow
        case .critical:
            return Color(red: 1.0, green: 0.85, blue: 0.88)//pink
        case .emergency:
            return Color(red: 1.0, green: 0.85, blue: 0.88)
        }
    }

    @ViewBuilder
    private func dataRow(title: String, value: String) -> some View {
        HStack {
            Text(title)
                .fontWeight(.medium)
                .foregroundColor(.black)
            Spacer()
            Text(value)
                .foregroundColor(.black)
        }
    }
}

#Preview {
    let sessionData = SessionData()
    sessionData.currentState = .warning
    sessionData.currentHeartRate = 108
    sessionData.currentBodyTemp = 100.1
    sessionData.ambientTemp = 88.0

    return NavigationStack {
        StatusView()
            .environmentObject(sessionData)
    }
}
