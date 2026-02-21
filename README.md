# ⌚ Vitaband: Wearable Heat-Stroke Detection

**Vitaband** is an open-source, high-precision wearable designed to warn and prevent against heat stroke, in intense environmental working conditions. Built on the **nRF52840** (ARM Cortex-M4) and **Zephyr RTOS**, it provides a robust platform for real-time biometrics.

---

## 🏗 System Architecture

The project is divided into three main pillars: Hardware (PCB/Mechanical), Firmware (Zephyr/C), and Data Visualization (Mobile/Web).



### 1. The Sensor Suite
Vitaband utilizes a specialized trio of sensors to ensure data accuracy:
* **Optical (MAX86141)**: Dual-channel PPG for Heart Rate and SpO2.
* **Skin Temperature (TMP117)**: High-accuracy medical-grade thermometry with $±0.1°C$ precision.
* **Ambient Environment (SHT3x-DIS)**: Measures humidity and air temperature to calibrate skin readings against environmental factors.

---

## 💎 Hardware Design

The hardware is optimized for a small form factor, prioritizing signal integrity for the sensitive analog fronts of the optical sensors.

### Schematics & PCB
The heart of the device is the nRF52840 SoC. We utilized a 4-layer PCB design to separate noisy digital lines from the sensitive I2C bus and the high-current LED traces.



### Mechanical & Industrial Design
The enclosure is 3D-printed using skin-safe TPU and Resin. A light-tight seal is maintained for the MAX86141 sensor, which is critical for preventing ambient light interference during PPG readings.



---

## 🔋 Power Management & Proximity

One of Vitaband's core innovations is its **Dynamic Power Scaling**. Using the MAX86141's optical proximity function, the device stays in a "Low Power" state (~8sps) until it detects contact with the skin.



* **PROX Mode**: High-efficiency sensing to detect an object.
* **Normal Mode**: Full 100Hz+ sampling for heart rate and SpO2 calculation.

---

## 📱 Software & Connectivity

The firmware broadcasts data over **Bluetooth 5.2 Low Energy** using standard and custom GATT services.

* **Heart Rate Profile (HRS)**: Compatible with standard fitness applications.
* **Environmental Service**: Custom service transmitting skin temp, ambient temp, and humidity.
* **Battery Service (BAS)**: Reports real-time voltage and battery percentage.



---

## 🛠 Project Structure

```text
.
├── docs/               # Datasheets, Schematics, and 3D Models (STLs)
├── firmware/           # Zephyr-based C source code (main.c, app.overlay)
│   ├── drivers/        # Custom I2C drivers for TMP117, SHT3x, MAX86141
│   └── inc/            # Header files and API definitions
├── hardware/           # KiCad project files and Gerber outputs
├── assets/             # Images, diagrams, and branding used in this README
└── README.md           # Project overview
