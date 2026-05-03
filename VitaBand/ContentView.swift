//
//  ContentView.swift
//  VitaBand
//
//  Created by Vidhi Challani on 2/9/26.
//

import SwiftUI

struct ContentView: View {
    @StateObject private var bleManager = VitaBandBLEManager()

    var body: some View {
        NavigationStack {
            VStack(spacing: 0) {
                ContactListView()

                Divider()
                    .padding(.top, 8)

                VStack(spacing: 8) {
                    Text("BLE Status")
                        .font(.headline)

                    Text(bleManager.connectionStatus)
                        .font(.caption)
                        .multilineTextAlignment(.center)

                    Text(bleManager.isConnected ? "Connected" : "Not Connected")
                        .foregroundColor(bleManager.isConnected ? .green : .red)

                    HStack {
                        Button("Scan") {
                            bleManager.startScan()
                        }

                        Button("Disconnect") {
                            bleManager.disconnect()
                        }
                    }
                }
                .padding()
            }
            .navigationBarTitleDisplayMode(.inline)
        }
    }
}

#Preview {
    ContentView()
        .environmentObject(SessionData())
}
