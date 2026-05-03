//
//  VitaBandApp.swift
//  VitaBand
//
//  Created by Vidhi Challani on 2/9/26.
//

import SwiftUI

@main
struct VitaBandApp: App {
    @StateObject private var sessionData = SessionData()

    var body: some Scene {
        WindowGroup {
            ContentView()
                .environmentObject(sessionData)
                .onAppear {
                    VitaBandManager.shared.attachSession(sessionData)
                }
        }
    }
}
