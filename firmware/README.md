# Vitaband Firmware

![License](https://img.shields.io/badge/License-Apache%202.0-blue.svg)
![Platform](https://img.shields.io/badge/Platform-nRF%20Connect%20SDK-orange.svg)
![Target](https://img.shields.io/badge/Target-nRF52840-green.svg)

**Vitaband** is an advanced health-tracking firmware built on the Zephyr RTOS. It integrates high-precision optical, skin temperature, and ambient sensors to provide real-time health monitoring via Bluetooth Low Energy (BLE).

## 🚀 Key Features

* **Heart Rate & SpO2**: Driver support for the MAX86141 with integrated FIFO management and low-power proximity detection.
* **Dual-Zone Thermometry**: 
    * **TMP117**: Medical-grade skin temperature monitoring.
    * **SHT3x-DIS**: Ambient temperature and humidity for environmental compensation.
* **Power Optimization**: Smart proximity sensing to reduce LED power consumption when not in contact with skin.
* **Modular Architecture**: Clean separation between hardware drivers (`/drivers`) and application logic (`/src`).

## 🛠 Project Structure

```text
├── drivers/
│   ├── inc/        # Driver header files (MAX86141, TMP117, SHT3x)
│   └── src/        # I2C driver implementations
├── inc/            # Application headers (BLE, Power, Sensors)
├── src/            # Main logic & State Machine
├── app_vitaband_base.overlay (full app: SPI0 MAX86140 + TMP117 + SHT3x on TWIM1)
├── app_bringup_min.overlay / app_ambient_pwm.overlay (+ optional *_i2c_dkswap)
├── app_sparkfun_tmp102_dk*.overlay / prj_sparkfun_tmp102_dk*.conf (TMP102)
├── app_tmp117_i2c*.overlay / prj_tmp117_i2c.conf (TMP117)
├── prj.conf, prj_bringup.conf, prj_ambient_pwm.conf, …
└── CMakeLists.txt
```

Bring-up and ambient images do **not** use a shared root `app.overlay`: pass the matching file with `-DDTC_OVERLAY_FILE=…` (see the header comments in each `prj_*.conf`) so overlays never merge unexpectedly.

