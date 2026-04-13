#include <stdio.h>
#include "esp_log.h"
#include "dht11.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_err.h"
#define TAG "SMART_HOME"

void dht11_task(void *pvParameters){
    dht11_reading_t reading; // cấu trúc để lưu trữ dữ liệu đọc được từ DHT11
    while(1){
        esp_err_t ret = dht11_read(&reading); // đọc dữ liệu từ DHT11 và lưu vào cấu trúc reading
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "Temp: %d, Hum: %d", reading.temperature, reading.humidity);
        } else if (ret == ESP_ERR_TIMEOUT) {
            ESP_LOGE(TAG, "Lỗi: Không tìm thấy cảm biến (Timeout)!");
        } else if (ret == ESP_ERR_INVALID_CRC) {
            ESP_LOGE(TAG, "Lỗi: Dữ liệu bị nhiễu (CRC Error)!");
        } else {
            ESP_LOGE(TAG, "Lỗi hệ thống khác!");
        }
        vTaskDelay(pdMS_TO_TICKS(2000)); // đợi 2 giây trước khi đọc dữ liệu tiếp theo
    }
}

void app_main(void)
{
    dht11_init(GPIO_NUM_4); // khởi tạo cảm biến DHT11

    xTaskCreate(dht11_task, "dht11_task", 2048, NULL, 5, NULL); // tạo task để đọc dữ liệu từ DHT11 và in ra console
    ESP_LOGI(TAG, "Khởi động Smart Home Firmware...");
}
