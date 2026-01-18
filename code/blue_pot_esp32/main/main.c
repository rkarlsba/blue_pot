/*
 * Blue POT for ESP32 - Main Application
 * Bluetooth to POTS Telephone Gateway
 * 
 * This is an ESP-IDF native port of the original Teensy Arduino code
 * 
 * Features:
 *   - BM64 Bluetooth module interface (HFP profile)
 *   - AG1171 SLIC telephone line interface
 *   - DTMF tone detection
 *   - Dial tone, busy tone, and phone ringing generation
 *   - Serial command interface for configuration
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "driver/uart.h"

#include "blue_pot.h"
#include "bt_module.h"
#include "pots_module.h"
#include "cmd_processor.h"

static const char *TAG = "BLUE_POT";

// =================================================================
// Task Handles
// =================================================================

static TaskHandle_t bluetooth_task_handle = NULL;
static TaskHandle_t pots_task_handle = NULL;
static TaskHandle_t command_task_handle = NULL;

// =================================================================
// Task Functions
// =================================================================

/*
 * Bluetooth Module Evaluation Task
 * Runs at ~50Hz (every 20ms)
 */
static void bluetooth_eval_task(void *pvParameters)
{
    const TickType_t delay = pdMS_TO_TICKS(20);

    ESP_LOGI(TAG, "Bluetooth evaluation task started");

    while (1) {
        bt_module_eval();
        vTaskDelay(delay);
    }

    vTaskDelete(NULL);
}

/*
 * POTS Module Evaluation Task
 * Runs at 100Hz (every 10ms)
 */
static void pots_eval_task(void *pvParameters)
{
    const TickType_t delay = pdMS_TO_TICKS(10);

    ESP_LOGI(TAG, "POTS evaluation task started");

    while (1) {
        pots_module_eval();
        vTaskDelay(delay);
    }

    vTaskDelete(NULL);
}

/*
 * Command Processor Task
 * Runs at lower priority to handle serial commands
 */
static void command_eval_task(void *pvParameters)
{
    ESP_LOGI(TAG, "Command processor task started");

    // Display startup info
    printf("\n");
    printf("===============================================\n");
    printf("Blue POT for ESP32 - Version %s\n", VERSION);
    printf("Bluetooth to POTS Telephone Gateway\n");
    printf("===============================================\n");
    printf("Type 'H' for help\n\n");

    while (1) {
        cmd_processor_eval();
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    vTaskDelete(NULL);
}

/*
 * Main application initialization and task creation
 */
void app_main(void)
{
    esp_err_t ret;

    // Get current pair ID from NVS for initialization
    nvs_handle_t nvs_handle;
    int pair_id = 0;

    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ret = nvs_open("blue_pot", NVS_READONLY, &nvs_handle);
    if (ret == ESP_OK) {
        nvs_get_i32(nvs_handle, PAIR_ID_KEY, &pair_id);
        nvs_close(nvs_handle);
    } else {
        pair_id = 0;  // Default to device 0
    }

    ESP_LOGI(TAG, "Blue POT ESP32 starting up");
    ESP_LOGI(TAG, "Using pairing device ID: %d", pair_id);

    // Initialize all subsystems
    ESP_LOGI(TAG, "Initializing modules...");
    cmd_processor_init();
    bt_module_init(pair_id);
    pots_module_init();

    ESP_LOGI(TAG, "All modules initialized successfully");

    // Create evaluation tasks with appropriate priorities
    // POTS task runs most frequently (every 10ms) - higher priority
    xTaskCreatePinnedToCore(
        pots_eval_task,              // Task function
        "pots_eval",                 // Task name
        2048,                        // Stack size
        NULL,                        // Parameters
        uxTaskPriorityGet(NULL) + 2, // Higher priority
        &pots_task_handle,           // Task handle
        0                            // Core 0
    );

    // Bluetooth task runs at 50Hz (every 20ms) - medium priority
    xTaskCreatePinnedToCore(
        bluetooth_eval_task,         // Task function
        "bt_eval",                   // Task name
        2048,                        // Stack size
        NULL,                        // Parameters
        uxTaskPriorityGet(NULL) + 1, // Medium priority
        &bluetooth_task_handle,      // Task handle
        1                            // Core 1 (if available)
    );

    // Command processor task - lowest priority
    xTaskCreatePinnedToCore(
        command_eval_task,           // Task function
        "cmd_eval",                  // Task name
        2048,                        // Stack size
        NULL,                        // Parameters
        uxTaskPriorityGet(NULL),     // Default priority
        &command_task_handle,        // Task handle
        0                            // Core 0
    );

    ESP_LOGI(TAG, "All tasks created successfully");
    ESP_LOGI(TAG, "Blue POT ESP32 ready");

    // This task is no longer needed, delete it
    vTaskDelete(NULL);
}
