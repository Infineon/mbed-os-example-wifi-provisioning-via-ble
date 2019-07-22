# WiFi Provisioning via BLE

### Introduction
  This code example demonstrates how to connect a kit to WiFi by
  provisioning the WiFi credentials via BLE.

### Supported Kits
```bash
  PSoC 6 Wi-Fi BT Prototyping Kit (CY8CPROTO_062_4343W)
  PSoC 6 WiFi-BT Pioneer Kit (CY8CKIT_062_WiFi_BT)
```

### Code Example Opertaion

  The WiFi and BLE stack on the kit will be initialized on power up.
  The BLE is initialized with custom GATT settings and will advertise its
  custom service with three characteristics in it:

  - BLE Name                : <b>CONNECT-TO-WIFI</b>
  - BLE Custom Service UUID : <b>21c04d09-c884-4af1-96a9-52e4e4ba195b</b>

  - BLE Characteristic 1
    - WiFi SSID             : TEXT (<= 32 chars long)
    - UUID                  : <b>1e500043-6b31-4a3d-b91e-025f92ca9763</b>
  - BLE Characteristic 2
    - WiFi Password         : TEXT (8 - 63 chars long)
    - UUID                  : <b>1e500043-6b31-4a3d-b91e-025f92ca9764</b>
  - BLE Characteristic 3
    - Connect/Disconnect    : UINT8 (1: connect; 0: disconnect)
    - UUID                  : <b>1e500043-6b31-4a3d-b91e-025f92ca9765</b>

  End user with the mobile app need to:

  - Connect to the BLE device with the name "CONNECT-TO-WIFI"
  - Discover BLE Service, and GATT characteristics.
  - Configure SSID, Password, and send connect request. Note that, if the AP
    is in "Open" network, the password field should be left empty.
  - Once the kit is connected to WiFi successfully, it will check for Internet
    connectivity by trying to fetch current timestamp from NTP server.

### Instructions to run the code example
```bash
  1. Import the code example into your mbed directory using following mbed command.
     $ mbed import https://github.com/cypresssemiconductorco/mbed-os-example-wifi-provisioning-via-ble

  2. Change working directory to the code example folder
     $ cd mbed-os-example-wifi-provisioning-via-ble

  3. Compile and program based on the target board and compiler type (GCC_ARM/IAR/ARMC6).
     Sample commands (include the parameter "--sterm" if you want to automatically open the
     serial terminal immediately after programming):
     $ mbed compile --toolchain GCC_ARM --target CY8CKIT_062_WIFI_BT --flash --sterm
     $ mbed compile --toolchain ARMC6 --target CY8CPROTO_062_4343W --flash --sterm
     $ mbed compile --toolchain IAR --target CY8CPROTO_062_4343W --flash --sterm
```
### NTP Remote Server Address
```bash
  Currently, the NTP server address is set with public address "2.pool.ntp.org".
  Refer to the file NTPClient.h if custom NTP server IP address needs to be set.
  Please note that, after connecting to AP, the kit will try to fetch current
  timestamp from NTP server only when the NTP server address configured is valid
  and Internet access is available.
```
### Known Issue
  The BLE link disconnection with the mobile phone is observed when the mobile
  phone is also configured as AP Hotspot for the kit to connect. The BLE connection
  is stable when the mobile hotspot is ON. When the hotspot is turned off, the BLE link
  gets disconnected. This is seen on Android and iOS mobile phones. So the recommended
  method is to use external WiFi router as AP and mobile phone for BLE connection to the
  kit.

### Release Notes
| **Version**  |                      **Description**             |
| ------------ | ------------------------------------------------
| 1.0          | Initial release. Tested with mbed-os v5.13.1     |

