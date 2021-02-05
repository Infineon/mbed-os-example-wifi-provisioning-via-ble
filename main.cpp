/*******************************************************************************
* File Name: main.cpp
*
* Version: 1.0
*
* Description:
*     This application demonstrates WiFi provisioning via BLE.
*     The application initializes both the WiFi and BLE stack. The necessary WiFi
*     credentials such as SSID and Password are passed by the user via mobile
*     app. The mobile app can be any third party app which can scan BLE devices,
*     scan BLE services and characteristics, connect, read and write GATT attributes.
*     Following RTOS resources are used.
*
*   Thread    : A thread to join AP. It waits for a semaphore to be released and
*               then connects to AP using the information provided by the user.
*   Semaphore : By default no semaphore is available for the thread to connect
*               to AP. Once the user configures valid WiFi credentials such as
*               SSID and Password, the semaphore will be released after data
*               validation.
*   EventQueue: EventQueue is the dispatcher for any OS events. Here it is mainly
*               used to act on any data written on BLE GATT database by the user.
*
* Related Document: README.md
*
* Supported Kits (Target Names):
*   CY8CKIT-062-WiFi-BT PSoC 6 WiFi-BT Pioneer Kit (CY8CKIT_062_WIFI_BT)
*   CY8CPROTO-062-4343W PSoC 6 Wi-Fi BT Prototyping Kit (CY8CPROTO_062_4343W)
*
********************************************************************************
* Copyright 2017-2019 Cypress Semiconductor Corporation
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
********************************************************************************/
#include <events/mbed_events.h>
#include "mbed.h"
#include "ble/BLE.h"
#include "pretty_printer.h"
#include "BleService.h"
#include "NTPClient.h"

#ifdef MBED_APP_PRINT_INFO
#define MBED_APP_INFO(x) printf x
#else
#define MBED_APP_INFO(x) do {} while (0)
#endif

#ifdef MBED_APP_PRINT_ERR
#define MBED_APP_ERR(x) printf x
#else
#define MBED_APP_ERR(x) do {} while (0)
#endif

#define WIFI_SCAN_AP_COUNT 15
#define BLE_NUM_EVENTS     16

static events::EventQueue ble_event_queue(BLE_NUM_EVENTS * EVENTS_EVENT_SIZE);

const static char DEVICE_NAME[]                    = "CONNECT-TO-WIFI";
uint8_t           wifi_ssid_name[WIFI_SSID_LEN]    = {0};
uint8_t           wifi_pwd[WIFI_PWD_LEN]           = {0};
uint8_t           wifi_connect_status              = 0;
uint16_t          max_ssid_len                     = WIFI_SSID_LEN;
uint16_t          max_pwd_len                      = WIFI_PWD_LEN;

const UUID BleService::BLE_SERVICE_UUID("21c04d09-c884-4af1-96a9-52e4e4ba195b");
const UUID BleService::WIFI_SSID_NAME_UUID("1e500043-6b31-4a3d-b91e-025f92ca9763");
const UUID BleService::WIFI_CONNECT_PWD_UUID("1e500043-6b31-4a3d-b91e-025f92ca9764");
const UUID BleService::WIFI_CONNECT_STATUS_UUID("1e500043-6b31-4a3d-b91e-025f92ca9765");

WiFiInterface 	*wifi;
Thread		T1;
Semaphore 	wifi_connect_sem(0);

/*
 * Main class to setup BLE advertisement with custom service,
 * handle BLE events such as update on GATT server, disconnect,
 * API to validate WiFi credentials and connect to the
 * given WiFi AP.
 */
class WiFiProvisioner : ble::Gap::EventHandler
{
public:
    BLE &_ble;

    WiFiProvisioner(BLE &ble, events::EventQueue &event_queue) :
        _ble(ble),
        _event_queue(event_queue),
        _ble_uuid(BleService::BLE_SERVICE_UUID),
        _ble_service(NULL),
        _adv_data_builder(_adv_buffer) { }

    ~WiFiProvisioner() {
        delete _ble_service;
    }

    /*
     * Initialize BLE, Set event handler and
     * setup BLE events dispatcher.
     * Starts advertising after BLE init complete.
     */
    void start() {
        _ble.gap().setEventHandler(this);
        _ble.init(this, &WiFiProvisioner::on_init_complete);
        _event_queue.dispatch_forever();
    }

    /*
     * Local function to update wifi connection status
     */
    void update_wifi_connection_status(uint8_t status)
    {
        uint8_t  gatt_db_conn_status;
        uint16_t len = sizeof(gatt_db_conn_status);

        _ble.gattServer().read(_ble_service->getConnectStatusValueHandle(),
                                               &gatt_db_conn_status, &len);
        if (gatt_db_conn_status != status)
        {
            _ble.gattServer().write(_ble_service->getConnectStatusValueHandle(),
                                                 (const uint8_t *)&status, len);
        }

        /* update local flag with the wifi connection status */
        wifi_connect_status = status;
    }

private:
    events::EventQueue &_event_queue;

    /* Adding BLE GATT service */
    UUID       _ble_uuid;
    BleService *_ble_service;

    uint8_t _adv_buffer[ble::LEGACY_ADVERTISING_MAX_SIZE];
    ble::AdvertisingDataBuilder _adv_data_builder;

    /*
     * Callback triggered when the ble initialization process has finished
     */
    void on_init_complete(BLE::InitializationCompleteCallbackContext *params)
    {
        if (params->error != BLE_ERROR_NONE) {
            MBED_APP_ERR(("Ble initialization failed."));
            MBED_ASSERT(false);
        }

	/* Initialize the BLE service  */
        _ble_service = new BleService((char *)&wifi_ssid_name[0], (char *)&wifi_pwd[0], false);
        _ble.gattServer().onDataWritten(this, &WiFiProvisioner::on_data_written);

        print_mac_address();
        start_advertising();
    }

    /*
     * Prepare BLE advertisement packet and start
     * advertising.
     */
    void start_advertising()
    {
        /* Create advertising parameters */
        ble::AdvertisingParameters adv_parameters(
            ble::advertising_type_t::CONNECTABLE_UNDIRECTED,
            ble::adv_interval_t(ble::millisecond_t(100))
        );

        _adv_data_builder.setFlags();
        _adv_data_builder.setName(DEVICE_NAME);

        /* Setup advertising */
        ble_error_t error = _ble.gap().setAdvertisingParameters(
                                 ble::LEGACY_ADVERTISING_HANDLE,
                                 adv_parameters);

        if (error)
        {
            print_error(error, "Setup advertising parameters failed");
            MBED_ASSERT(false);
        }

        error = _ble.gap().setAdvertisingPayload(ble::LEGACY_ADVERTISING_HANDLE,
                                        _adv_data_builder.getAdvertisingData());

        if (error)
        {
            print_error(error, "Setup advertising payload failed");
            MBED_ASSERT(false);
        }

        /* Start advertising */
        error = _ble.gap().startAdvertising(ble::LEGACY_ADVERTISING_HANDLE);

        if (error)
        {
            print_error(error, "Start advertising failed");
            MBED_ASSERT(false);
        }
    }

    /*
     * Function to validate WiFi credentials and try to
     * connect to the given WiFi AP SSID.
     */
    void check_and_let_wifi_connect()
    {
        if (wifi->get_connection_status() == NSAPI_STATUS_GLOBAL_UP)
        {
            MBED_APP_ERR(("Already connected to AP. You may disconnect and connect again.\n"));
            return;
        }

        /* connect to AP only when we have valid wifi credentials */
        if (wifi_ssid_name[0] != '\0')
        {
            /* release semaphore for wifi thread to try connect to AP */
            wifi_connect_sem.release();
        } else {
            MBED_APP_INFO(("Invalid WiFi credentials. Please configure SSID and Password\n"));
            _ble.gattServer().write(_ble_service->getConnectStatusValueHandle(), 0, sizeof(uint8_t));
            wifi_connect_status = 0;
        }
    }

    /*
     * Callback function when anything written to GATT database
     */
    void on_data_written(const GattWriteCallbackParams *params)
    {
        if (params->handle == _ble_service->getSsidValueHandle())
        {
            memset(wifi_ssid_name, 0, sizeof(wifi_ssid_name));
            memcpy(wifi_ssid_name, params->data, params->len);
            MBED_APP_INFO(("WiFi SSID: %s\n", wifi_ssid_name));
        }
        else if (params->handle == _ble_service->getPwdValueHandle())
        {
            memset(wifi_pwd, 0, sizeof(wifi_pwd));
            memcpy(wifi_pwd, params->data, params->len);
            MBED_APP_INFO(("WiFi Password: %c%cxxx\n", wifi_pwd[0], wifi_pwd[1]));
            if (wifi_connect_status)
            {
                MBED_APP_INFO(("Connecting to AP...\n"));
                check_and_let_wifi_connect();
            }
        }
        else if (params->handle == _ble_service->getConnectStatusValueHandle())
        {
            wifi_connect_status = params->data[0];
            if (wifi_connect_status)
            {
                MBED_APP_INFO(("Connecting to AP...\n"));
                check_and_let_wifi_connect();
            } else {
                MBED_APP_INFO(("Received Wifi disconnect request.\n"));
                wifi->disconnect();
            }
        }
    }

    /*
     * On disconnect complete event handler
     */
    void onDisconnectionComplete(const ble::DisconnectionCompleteEvent &event) {
        _ble.gap().startAdvertising(ble::LEGACY_ADVERTISING_HANDLE);
        MBED_APP_INFO(("---BLE link disconnected---\n"));
    }
};

/* Initialize global objects */
BLE               &app_ble = BLE::Instance();
WiFiProvisioner   wifi_provision_app(app_ble, ble_event_queue);

/*
 * Schedule processing of events from the BLE middleware in the event queue.
 */
void schedule_ble_events(BLE::OnEventsToProcessCallbackContext *context)
{
    ble_event_queue.call(Callback<void()>(&context->ble, &BLE::processEvents));
}

const char *sec2str(nsapi_security_t sec)
{
    switch (sec) {
        case NSAPI_SECURITY_NONE:
            return "None";
        case NSAPI_SECURITY_WEP:
            return "WEP";
        case NSAPI_SECURITY_WPA:
            return "WPA";
        case NSAPI_SECURITY_WPA2:
            return "WPA2";
        case NSAPI_SECURITY_WPA_WPA2:
            return "WPA/WPA2";
        case NSAPI_SECURITY_UNKNOWN:
        default:
            return "Unknown";
    }
}

/*
 * Scan for WiFi Access Points around.
 * The max scan count has been set to 15, so it will
 * scan and report upto 15 APs available around.
 */
int wifi_scan(WiFiInterface *wifi)
{
    WiFiAccessPoint *ap;
    int count = 0;

    MBED_APP_INFO(("Scan:\n"));

    ap = new WiFiAccessPoint[WIFI_SCAN_AP_COUNT];
    count = wifi->scan(ap, WIFI_SCAN_AP_COUNT);

    if (count <= 0) {
        MBED_APP_ERR(("scan() failed with return value: %d\n", count));
        return count;
    }

    for (int i = 0; i < count; i++) {
        MBED_APP_INFO(("Network: %s secured: %s BSSID: %hhX:%hhX:%hhX:%hhx:%hhx:%hhx RSSI: %hhd Ch: %hhd\n",
                       ap[i].get_ssid(), sec2str(ap[i].get_security()), ap[i].get_bssid()[0],
                       ap[i].get_bssid()[1], ap[i].get_bssid()[2], ap[i].get_bssid()[3],
                       ap[i].get_bssid()[4], ap[i].get_bssid()[5], ap[i].get_rssi(), ap[i].get_channel()));
    }

    MBED_APP_INFO(("%d Networks available.\n", count));

    delete[] ap;

    return count;
}

/*
 * Print out the WiFi connection information.
 */
void dump_wifi_stats()
{
    SocketAddress ipAddress;
    SocketAddress netMask;
    SocketAddress gateway;

    wifi->get_ip_address(&ipAddress);
    wifi->get_netmask(&netMask);
    wifi->get_gateway(&gateway);

    MBED_APP_INFO(("MAC\t: %s\n", wifi->get_mac_address()));
    MBED_APP_INFO(("IP\t: %s\n", ipAddress.get_ip_address()));
    MBED_APP_INFO(("Netmask\t: %s\n", netMask.get_ip_address()));
    MBED_APP_INFO(("Gateway\t: %s\n", gateway.get_ip_address()));
    MBED_APP_INFO(("RSSI\t: %d\n\n", wifi->get_rssi()));
}

/*
 * Try to fetch current network time from NTP server
 * every 5 seconds.
 */
void get_ntp_timestamp(void)
{
    NTPClient ntp(wifi);

    while(1)
    {
        time_t timestamp = ntp.get_timestamp();
        if (timestamp < 0) {
            MBED_APP_ERR(("An error occurred when getting the time. NTP Error Code: %lld\r\n"
                          "Please check AP Internet settings\n", timestamp));
        } else {
            MBED_APP_INFO(("Current time is %s\r\n", ctime(&timestamp)));
        }

        MBED_APP_INFO(("Waiting 5 seconds before trying again.\r\n"));
        ThisThread::sleep_for(5s);

        if (wifi->get_connection_status() == NSAPI_STATUS_CONNECTING)
        {
            MBED_APP_INFO(("Trying to connect to AP...\n"));
        }

        if (wifi->get_connection_status() != NSAPI_STATUS_GLOBAL_UP)
        {
            MBED_APP_INFO(("---WiFi disconnected---\n"));
            break;
        }
    }
}

/*
 * Connect to WiFi AP once the semaphore gets
 * released. Semaphore can be acquired when
 * the WiFi credentails are valid and connect request
 * made by the user.
 */
void wifi_connect_thread(void)
{
    do
    {
        int ret = 0;

        /* Wait for SSID & passphrase to be configured */
        wifi_connect_sem.acquire();

        /* Connect to AP */
        if (wifi_pwd[0] == '\0') {
            ret = wifi->connect((char *)&wifi_ssid_name[0], (char *)&wifi_pwd[0], NSAPI_SECURITY_NONE);
        } else {
            ret = wifi->connect((char *)&wifi_ssid_name[0], (char *)&wifi_pwd[0], NSAPI_SECURITY_WPA_WPA2);
        }
        if (ret != 0) {
            MBED_APP_ERR(("Wifi Connection Error: %d\n", ret));
            wifi_provision_app.update_wifi_connection_status(false);
        } else {
            MBED_APP_INFO(("Wifi Connection Success. Connected to AP: %s\n", wifi_ssid_name));
            MBED_APP_INFO(("WiFi Connection Info:\n"));
            dump_wifi_stats();
            MBED_APP_INFO(("Getting NTP timestamp from remote server to check Internet connectivity..\n"));
            get_ntp_timestamp();
        }

    } while (1);
}

/*
 * Application main().
 * Initialize WiFi and BLE interface,
 * start app thread, and start BLE advertisements.
 */
int main()
{
    /* Initialize WiFi stack */
    wifi = WiFiInterface::get_default_instance();

    if (!wifi) {
	MBED_APP_ERR(("Error: No WiFiInterface found.\n"));
	return -1;
    }
    int numAps = wifi_scan(wifi);
    MBED_APP_INFO(("Number of APs: %d\n", numAps));

    /* Start thread to join AP */
    T1.start(wifi_connect_thread);

    app_ble.onEventsToProcess(schedule_ble_events);

    /* Start beacon with simple GATT server to configure wifi ssid and password */
    wifi_provision_app.start();

    ThisThread::sleep_for(osWaitForever);

    return 0;
}