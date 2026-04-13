#ifndef DHT11_H
#define DHT11_H

#include "esp_err.h"

// cấu trúc dữ liệu để lưu trữ giá trị nhiệt độ và độ ẩm
typedef struct{
    int temperature;
    int humidity;
} dht11_reading_t;

//  khởi tạo chân gpio cho cảm biến dht11
//  truyền vào số chân gpio được kết nối với cảm biến
esp_err_t dht11_init(int gpio_num);

// đọc dữ liệu từ cảm biến dht11
// trả về một cấu trúc dht11_reading_t chứa giá trị nhiệt độ và độ ẩm
// nếu có lỗi xảy ra, trả về mã lỗi 
esp_err_t dht11_read(dht11_reading_t *reading); 

#endif

