//
//  ContactListView.swift
//  VitaBand
//
//  Created by Vidhi Challani on 2/10/26.
//
import SwiftUI

struct ContactListView: View {
    @EnvironmentObject var sessionData: SessionData

    @State private var newName = ""
    @State private var newPhoneNumber = ""

    var body: some View {
        VStack(spacing: 20) {
            Text("Emergency Contact List")
                .font(.largeTitle)
                .bold()

            VStack(spacing: 12) {
                TextField("Contact name", text: $newName)
                    .textFieldStyle(.roundedBorder)

                TextField("Phone number", text: $newPhoneNumber)
                    .textFieldStyle(.roundedBorder)
                    .keyboardType(.phonePad)

                Button("Add Contact") {
                    addContact()
                }
                .buttonStyle(.borderedProminent)
            }

            if sessionData.contacts.isEmpty {
                Spacer()

                Text("No emergency contacts added yet.")
                    .foregroundColor(.gray)

                Spacer()
            } else {
                List {
                    ForEach(sessionData.contacts) { contact in
                        VStack(alignment: .leading, spacing: 4) {
                            Text(contact.name)
                                .font(.headline)

                            Text(contact.phoneNumber)
                                .foregroundColor(.secondary)
                        }
                    }
                    .onDelete(perform: deleteContact)
                }
            }

            NavigationLink("Next") {
                CalibrationView()
            }
            .buttonStyle(.borderedProminent)
            .disabled(sessionData.contacts.isEmpty)
        }
        .padding()
    }

    private func addContact() {
        let trimmedName = newName.trimmingCharacters(in: .whitespacesAndNewlines)
        let trimmedPhone = newPhoneNumber.trimmingCharacters(in: .whitespacesAndNewlines)

        guard !trimmedName.isEmpty, !trimmedPhone.isEmpty else { return }

        let contact = EmergencyContact(name: trimmedName, phoneNumber: trimmedPhone)
        sessionData.contacts.append(contact)

        newName = ""
        newPhoneNumber = ""
    }

    private func deleteContact(at offsets: IndexSet) {
        sessionData.contacts.remove(atOffsets: offsets)
    }
}

