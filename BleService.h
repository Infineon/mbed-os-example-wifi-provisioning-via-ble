/********************************************************************************
* Copyright 2019 Cypress Semiconductor Corporation
* SPDX-License-Identifier: Apache-2.0
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*     http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*******************************************************************************/
#ifndef __BLE_SERVICE_H__
#define __BLE_SERVICE_H__

#define WIFI_SSID_LEN  33
#define WIFI_PWD_LEN   64

class BleService {
public:

    const static UUID BLE_SERVICE_UUID;
    const static UUID WIFI_SSID_NAME_UUID;
    const static UUID WIFI_CONNECT_PWD_UUID;
    const static UUID WIFI_CONNECT_STATUS_UUID;

    BleService(char *ssid, char *pwd, bool connectionStatus) :
               _wifi_ssid(WIFI_SSID_NAME_UUID, ssid),
               _wifi_pwd(WIFI_CONNECT_PWD_UUID, pwd),
               _wifi_state(WIFI_CONNECT_STATUS_UUID, &connectionStatus)
               {
                   GattCharacteristic *charTable[] = {&_wifi_ssid, &_wifi_pwd, &_wifi_state};
                   GattService         wifiProvisioningService(BLE_SERVICE_UUID, charTable, sizeof(charTable) / sizeof(GattCharacteristic *));
                   BLE::Instance().gattServer().addService(wifiProvisioningService);
               }

    /*
     * Get GATT characteristic handle for wifi ssid.
     */
    GattAttribute::Handle_t getSsidValueHandle() const
    {
        return _wifi_ssid.getValueHandle();
    }

    /*
     * Get GATT characteristic handle for wifi password.
     */
    GattAttribute::Handle_t getPwdValueHandle() const
    {
        return _wifi_pwd.getValueHandle();
    }

    /*
     * Get GATT characteristic handle for wifi connection status.
     */
    GattAttribute::Handle_t getConnectStatusValueHandle() const
    {
        return _wifi_state.getValueHandle();
    }

private:
    ReadWriteArrayGattCharacteristic<char, WIFI_SSID_LEN> _wifi_ssid;
    ReadWriteArrayGattCharacteristic<char, WIFI_PWD_LEN> _wifi_pwd;
    ReadWriteGattCharacteristic<bool> _wifi_state;
};

#endif /* #ifndef __BLE_SERVICE_H__ */