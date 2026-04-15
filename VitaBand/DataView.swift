//
//  DataView.swift
//  VitaBand
//
//  Created by Vidhi Challani on 2/10/26.
//
import SwiftUI

struct DataView: View {
    @EnvironmentObject var sessionData: SessionData

    var body: some View {
        VStack(spacing: 16) {
            Text("Sensor Data")
                .font(.largeTitle)
                .bold()

            Group {
                Text("Current State: \(sessionData.currentState.rawValue)")
                Text("Current Heart Rate: \(sessionData.currentHeartRate, specifier: "%.1f") bpm")
                Text("Current Body Temperature: \(sessionData.currentBodyTemp, specifier: "%.1f") °F")
                Text("Beginning Heart Rate: \(sessionData.baselineHeartRate, specifier: "%.1f") bpm")
                Text("Beginning Body Temperature: \(sessionData.baselineBodyTemp, specifier: "%.1f") °F")
                Text("Ambient Temperature: \(sessionData.ambientTemp, specifier: "%.1f") °F")
            }
            .font(.title3)

            Spacer()
        }
        .padding()
    }
}
