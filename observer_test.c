#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "stdbool.h"

#include <sys/socket.h>
#include <netdb.h>
#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_partition.h"

#include "nvs.h"
#include "nvs_flash.h"

#include "driver/gpio.h"
#include "driver/uart.h"
#include "driver/i2s.h"

#include "sdkconfig.h"
#include "rom/uart.h"

#include "controller.h"
#include "esp_bt.h"
#include "esp_bt_device.h"
#include "esp_gap_ble_api.h"
#include "esp_gattc_api.h"
#include "esp_gatt_defs.h"
#include "esp_bt_main.h"
#include "esp_ibeacon_api.h"
#include "esp_bt_defs.h"

#define UARTB_TXD (18)
#define UARTB_RXD (19)
#define UARTA_TXD (17)
#define UARTA_RXD (16)
#define DEFAULT_BAUD  921600
#define UART_BUFFSIZE 128
char* test_message = "TEST";
/// Bluetooth address length
#define ESP_BD_ADDR_LEN     6
//***********************************************************************************************
static const char *TAG1 = "TEST: ";
#define delay(ms) (vTaskDelay(ms/portTICK_RATE_MS));

#define LED1   25 	// Yesil led
#define LED2   26 	// Mavi led

#define OUT_A  5
#define OUT_B  12

#define IN_A  13
#define IN_B  14
//***********************************************************************************************
static const char* DEMO_TAG = "IBEACON_DEMO";
extern esp_ble_ibeacon_vendor_t vendor_config;

///Declare static functions
static void esp_gap_cb(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param);

#if (IBEACON_MODE == IBEACON_RECEIVER)
static esp_ble_scan_params_t ble_scan_params = {
    .scan_type              = BLE_SCAN_TYPE_ACTIVE,
    .own_addr_type          = BLE_ADDR_TYPE_PUBLIC,
    .scan_filter_policy     = BLE_SCAN_FILTER_ALLOW_ALL,
    .scan_interval          = 0x50,
    .scan_window            = 0x30
};

#elif (IBEACON_MODE == IBEACON_SENDER)
static esp_ble_adv_params_t ble_adv_params = {
    .adv_int_min        = 0x20,
    .adv_int_max        = 0x40,
    .adv_type           = ADV_TYPE_NONCONN_IND,
    .own_addr_type      = BLE_ADDR_TYPE_PUBLIC,
    .channel_map        = ADV_CHNL_ALL,
    .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};
#endif

static void esp_gap_cb(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    switch (event) {
    case ESP_GAP_BLE_ADV_DATA_RAW_SET_COMPLETE_EVT:{
#if (IBEACON_MODE == IBEACON_SENDER)
        esp_ble_gap_start_advertising(&ble_adv_params);
#endif
        break;
    }
    case ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT: {
#if (IBEACON_MODE == IBEACON_RECEIVER)
        //the unit of the duration is second, 0 means scan permanently
        uint32_t duration = 0;
        esp_ble_gap_start_scanning(duration);
#endif
        break;
    }
    case ESP_GAP_BLE_SCAN_START_COMPLETE_EVT:
        //scan start complete event to indicate scan start successfully or failed
        if (param->scan_start_cmpl.status != ESP_BT_STATUS_SUCCESS) {
            ESP_LOGE(DEMO_TAG, "Scan start failed");
        }
        break;
    case ESP_GAP_BLE_ADV_START_COMPLETE_EVT:
        //adv start complete event to indicate adv start successfully or failed
        if (param->adv_start_cmpl.status != ESP_BT_STATUS_SUCCESS) {
            ESP_LOGE(DEMO_TAG, "Adv start failed");
        }
        break;
    case ESP_GAP_BLE_SCAN_RESULT_EVT: {
        esp_ble_gap_cb_param_t *scan_result = (esp_ble_gap_cb_param_t *)param;
        switch (scan_result->scan_rst.search_evt) {
        case ESP_GAP_SEARCH_INQ_RES_EVT:
            /* Search for BLE iBeacon Packet */
            if (esp_ble_is_ibeacon_packet(scan_result->scan_rst.ble_adv, scan_result->scan_rst.adv_data_len)){
                esp_ble_ibeacon_t *ibeacon_data = (esp_ble_ibeacon_t*)(scan_result->scan_rst.ble_adv);
                ESP_LOGE(DEMO_TAG, "----------iBeacon Found----------");
                esp_log_buffer_hex("IBEACON_DEMO: Device address:", scan_result->scan_rst.bda, BD_ADDR_LEN );
                esp_log_buffer_hex("IBEACON_DEMO: Proximity UUID:", ibeacon_data->ibeacon_vendor.proximity_uuid, ESP_UUID_LEN_128);

                uint16_t major = ENDIAN_CHANGE_U16(ibeacon_data->ibeacon_vendor.major);
                uint16_t minor = ENDIAN_CHANGE_U16(ibeacon_data->ibeacon_vendor.minor);
                ESP_LOGE(DEMO_TAG, "Major: 0x%04x (%d)", major, major);
                ESP_LOGE(DEMO_TAG, "Minor: 0x%04x (%d)", minor, minor);
                ESP_LOGE(DEMO_TAG, "Measured power (RSSI at a 1m distance):%d dbm", ibeacon_data->ibeacon_vendor.measured_power);
                ESP_LOGE(DEMO_TAG, "RSSI of packet:%d dbm", scan_result->scan_rst.rssi);
                delay(1000);

            	uint8_t *MAC_address_find = NULL;
            	MAC_address_find = scan_result->scan_rst.bda;

           					printf("MAC_address_find: ");
            		            for (int j = 0; j < BD_ADDR_LEN; j++) {
            		            	printf("%02X ", MAC_address_find[j]);
            		            }
            		            printf("\n");

            }
            break;
        default:
            break;
        }
        break;
    }

    case ESP_GAP_BLE_SCAN_STOP_COMPLETE_EVT:
        if (param->scan_stop_cmpl.status != ESP_BT_STATUS_SUCCESS){
            ESP_LOGE(DEMO_TAG, "Scan stop failed");
        }
        else {
            ESP_LOGI(DEMO_TAG, "Stop scan successfully");
        }
        break;

    case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
        if (param->adv_stop_cmpl.status != ESP_BT_STATUS_SUCCESS){
            ESP_LOGE(DEMO_TAG, "Adv stop failed");
        }
        else {
            ESP_LOGI(DEMO_TAG, "Stop adv successfully");
        }
        break;

    default:
        break;
    }
}

void ble_ibeacon_appRegister(void)
{
    esp_err_t status;

    ESP_LOGI(DEMO_TAG, "register callback");

    //register the scan callback function to the gap module
    if ((status = esp_ble_gap_register_callback(esp_gap_cb)) != ESP_OK) {
        ESP_LOGE(DEMO_TAG, "gap register error, error code = %x", status);
        return;
    }
}

void ble_ibeacon_init(void)
{
    esp_bluedroid_init();
    esp_bluedroid_enable();
    ble_ibeacon_appRegister();
//***********************************************************************************************
//	MAC address read

	uint8_t WifiMAC[6];
	uint8_t *MAC=esp_bt_dev_get_address();

    memcpy(WifiMAC, MAC, sizeof(uint8_t) * ESP_BD_ADDR_LEN);
    ESP_LOGE(TAG1, "ESP_MAC: %02X%02X%02X%02X%02X%02X",WifiMAC[0],WifiMAC[1],WifiMAC[2],WifiMAC[3],WifiMAC[4],WifiMAC[5]);
 //***********************************************************************************************
}

void app_main()
{
		gpio_pad_select_gpio(LED1);
		gpio_pad_select_gpio(LED2);

		gpio_pad_select_gpio(IN_A);
		gpio_pad_select_gpio(IN_B);

		gpio_pad_select_gpio(OUT_A);
		gpio_pad_select_gpio(OUT_B);

		gpio_set_direction(LED1, GPIO_MODE_OUTPUT);
		gpio_set_direction(LED2, GPIO_MODE_OUTPUT);

		gpio_set_direction(IN_A, GPIO_MODE_INPUT);
		gpio_set_direction(IN_B, GPIO_MODE_INPUT);

		gpio_set_pull_mode(IN_A, GPIO_PULLUP_ONLY);
		gpio_set_pull_mode(IN_B, GPIO_PULLUP_ONLY);

		gpio_set_direction(OUT_A, GPIO_MODE_OUTPUT);
		gpio_set_direction(OUT_B, GPIO_MODE_OUTPUT);

		gpio_set_pull_mode(OUT_A, GPIO_PULLUP_ONLY);
		gpio_set_pull_mode(OUT_B, GPIO_PULLUP_ONLY);

		gpio_set_level(LED1, 0);
		gpio_set_level(LED2, 0);
//***********************************************************************************************
		uint8_t receive_buffer[UART_BUFFSIZE];
		memset (receive_buffer, 0, UART_BUFFSIZE);
		int UART_count=0;
		int input_output_test=0;
//		int beacon_test=0;

		//UART1 configuration
		const int uart_num1 = UART_NUM_1;
		uart_config_t uart_config1 = {
			.baud_rate = DEFAULT_BAUD,
			.data_bits = UART_DATA_8_BITS,
			.parity    = UART_PARITY_DISABLE,
			.stop_bits = UART_STOP_BITS_1,
			.flow_ctrl = UART_HW_FLOWCTRL_DISABLE,    //UART_HW_FLOWCTRL_CTS_RTS,
			.rx_flow_ctrl_thresh = 122,
		};
//----------------------------------------------------------------------------------------
		//Configure UART1 parameters
		uart_param_config(uart_num1, &uart_config1);
		uart_set_pin(uart_num1, UARTB_TXD, UARTA_RXD, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE);
		uart_driver_install(uart_num1, UART_BUFFSIZE*2, 0, 0, NULL, 0);
//***********************************************************************************************
// UART Transmit, Receive Test
while ( UART_count <= 2 ) {
		uart_write_bytes(uart_num1, test_message, strlen(test_message));
		ESP_LOGE(TAG1, "Transmit Message: %s", test_message);
		ESP_LOGE(TAG1, "UART Transmit OK \r");
		//Read data from UARTA
		int len1 = uart_read_bytes(uart_num1, receive_buffer, UART_BUFFSIZE*2, 0);
		if(len1 > 0) {
				int sonuc;
				sonuc = strcmp(test_message, (char*)receive_buffer);
				if(sonuc==0){

		ESP_LOGE(TAG1, "Receive Message: %s", receive_buffer);
		ESP_LOGE(TAG1, "UART Receive OK \r\n");
		memset (receive_buffer, 0, UART_BUFFSIZE);
				}
		}
UART_count++;
}
//***********************************************************************************************
//		- INPUT_A - OK.
//    	- INPUT_B - OK.
//		- OUTA - OK.
//    	- OUTB - OK.
while ( input_output_test <= 2 ) {

		gpio_set_level(OUT_A, 0);
		delay(200);
		if (gpio_get_level(IN_B) == 0)	{
			ESP_LOGE(TAG1, "INPUT_B - OK.");
			ESP_LOGE(TAG1, "OUTA - OK.");
		}

		gpio_set_level(OUT_B, 0);
		delay(200);
		if (gpio_get_level(IN_A) == 0)	{
			ESP_LOGE(TAG1, "INPUT_A - OK.");
			ESP_LOGE(TAG1, "OUTB - OK.\r\n");
		}

input_output_test++;
}
//***********************************************************************************************
// BEACON Test
//while ( beacon_test <= 2 ) {
//***********************************************************************************************
		ESP_ERROR_CHECK(nvs_flash_init());
		ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));
		esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
		esp_bt_controller_init(&bt_cfg);
		esp_bt_controller_enable(ESP_BT_MODE_BLE);

		ble_ibeacon_init();
		/* set scan parameters */

		#if (IBEACON_MODE == IBEACON_RECEIVER)
		esp_ble_gap_set_scan_params(&ble_scan_params);

		#elif (IBEACON_MODE == IBEACON_SENDER)
		esp_ble_ibeacon_t ibeacon_adv_data;
		esp_err_t status = esp_ble_config_ibeacon_data (&vendor_config, &ibeacon_adv_data);
		if (status == ESP_OK){
			esp_ble_gap_config_adv_data_raw((uint8_t*)&ibeacon_adv_data, sizeof(ibeacon_adv_data));
		}
		else {
			ESP_LOGE(DEMO_TAG, "Config iBeacon data failed, status =0x%x\n", status);
		}
		#endif
//***********************************************************************************************
int count=0;
while(1) {
//***********************************************************************************************
//LED Test
		gpio_set_level(LED1, 1);
		gpio_set_level(LED2, 0);
//    	ESP_LOGE(TAG, "Yesil LEDi yak");
		delay(500);
		gpio_set_level(LED1, 0);
		gpio_set_level(LED2, 1);
//    	ESP_LOGE(TAG, "Mavi LEDi yak");
		delay(500);
		count++;
		printf("count: %d\r\n", count);

		if(count==2){
			if(esp_ble_gap_stop_scanning()==ESP_OK){
				ESP_LOGI(TAG1, "Stopped");
			}else{
			ESP_LOGI(TAG1, "Stopped Failed");
			}
	}
//***********************************************************************************************

delay(100);
}
}

