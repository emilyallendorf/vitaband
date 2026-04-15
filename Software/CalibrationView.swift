//
//  CalibrationView.swift
//  VitaBand
//
//  Created by Vidhi Challani on 2/10/26.
//
import SwiftUI

struct CalibrationView: View {
    @EnvironmentObject var sessionData: SessionData
    @State private var calibrated = false

    var body: some View {
        VStack(spacing: 24) {
            Text("Calibration")
                .font(.largeTitle)
                .bold()

            Text("Press calibrate to capture the resting body temperature and heart rate.")
                .multilineTextAlignment(.center)

            Button("Calibrate") {
                VitaBandManager.shared.calibrateBaseline()
                calibrated = true
            }
            .buttonStyle(.borderedProminent)
            if calibrated {
                VStack(spacing: 8) {
                    Text("Beginning Heart Rate: \(sessionData.baselineHeartRate, specifier: "%.1f") bpm")
                    Text("Beginning Body Temperature: \(sessionData.baselineBodyTemp, specifier: "%.1f") °F")
                }
                .font(.title3)
            }
            NavigationLink("Finish") {
                StatusView()
            }
            .buttonStyle(.borderedProminent)
            .disabled(!calibrated)

            Spacer()
        }
        .padding()
    }
}

#Preview {
    CalibrationView()
        .environmentObject(SessionData())
}

