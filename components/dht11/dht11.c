#include "dht11.h"
#include "driver/gpio.h"
#include "esp_rom_sys.h" // sử dụng hàm delay từ thư viện esp_rom_sys
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// khai báo biến toàn cục để lưu trữ số chân GPIO được sử dụng cho cảm biến DHT11
static int dht_gpio; 

// hàm khởi tạo chân GPIO cho cảm biến DHT11
esp_err_t dht11_init(int gpio_num){
    dht_gpio = gpio_num; // lưu trữ số chân GPIO vào biến toàn cục
    gpio_reset_pin(dht_gpio); // đặt lại cấu hình của chân GPIO về mặc định
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_DISABLE, // tắt ngắt = gpio_install_isr_service(0) sẽ không sử dụng ngắt
        .mode = GPIO_MODE_INPUT_OUTPUT, // thiết lập chân GPIO là input/output = gpio_set_direction(dht_gpio, GPIO_MODE_INPUT_OUTPUT) 
        .pin_bit_mask = (1ULL << dht_gpio), // chọn chân GPIO được sử dụng = gpio_set_pin(dht_gpio)
        .pull_down_en = GPIO_PULLDOWN_DISABLE, // tắt chế độ pull-down = gpio_set_pull_mode(dht_gpio, GPIO_PULLDOWN_DISABLE)
        .pull_up_en = GPIO_PULLUP_ENABLE // bật chế độ pull-up = gpio_set_pull_mode(dht_gpio, GPIO_PULLUP_ENABLE)
    };
    esp_err_t ret = gpio_config(&io_conf); // cấu hình chân GPIO
    if(ret == ESP_OK){
        // sau khi cấu hình thành công, đặt chân GPIO ở mức cao để chuẩn bị cho việc giao tiếp với cảm biến DHT11
        gpio_set_level(dht_gpio, 1); 
    }
    return ret; // trả về kết quả của việc cấu hình chân GPIO
}

static int wait_for_level(int level, int timeout_us){
    int us = 0; // biến đếm thời gian chờ
    while (gpio_get_level(dht_gpio) != level){ // trong khi chân GPIO ở mức chưa thay đổi
        if(us >= timeout_us) return -1; // nếu thời gian chờ vượt quá giới hạn, trả về lỗi
        esp_rom_delay_us(1); // đợi 1 micro giây trước khi kiểm tra lại
        us++; // tăng biến đếm thời gian chờ
    }
    return 0; // trả về thành công nếu chân GPIO đã thay đổi mức
}

esp_err_t dht11_read(dht11_reading_t *reading){
    uint8_t data[5] = {0}; // mảng để lưu trữ dữ liệu đọc được từ cảm biến DHT11

    // Send start signal to DHT11
    gpio_set_direction(dht_gpio, GPIO_MODE_OUTPUT); // thiết lập chân GPIO là output để gửi tín hiệu khởi động
    gpio_set_level(dht_gpio, 0); // gửi tín hiệu khởi động
    vTaskDelay(pdMS_TO_TICKS(20)); // MCU gửi tín hiệu khởi động trong 20ms (>18ms theo datasheet) để đảm bảo DHT11 nhận được tín hiệu này

    gpio_set_level(dht_gpio, 1); //
    esp_rom_delay_us(30); // sau khi gửi tín hiệu khởi động, MCU cần giữ chân GPIO ở mức cao trong khoảng 20-40us để DHT11 có thời gian phản hồi
    // Sau khi gửi tín hiệu khởi động, MCU cần chuyển chân GPIO sang chế độ input để nhận dữ liệu từ DHT11
    gpio_set_direction(dht_gpio, GPIO_MODE_INPUT); // thiết lập chân GPIO là input để nhận dữ liệu từ DHT11

    // DHT11 sẽ phản hồi bằng cách kéo chân GPIO xuống mức thấp trong khoảng 80us, 
    if(wait_for_level(0, 100) == -1) return ESP_ERR_TIMEOUT; // nếu DHT11 không kéo chân GPIO xuống mức thấp trong khoảng thời gian này, trả về lỗi timeout
    // sau đó kéo lên mức cao trong khoảng 80us nữa trước khi bắt đầu gửi dữ liệu
    if(wait_for_level(1, 100) == -1) return ESP_ERR_TIMEOUT; // nếu DHT11 không kéo chân GPIO lên mức cao trong khoảng thời gian này, trả về lỗi timeout
    // 
    if(wait_for_level(0, 100) == -1) return ESP_ERR_TIMEOUT; // nếu DHT11 không kéo chân GPIO xuống mức thấp trong khoảng thời gian này, trả về lỗi timeout

    // đọc 40 bits 
    portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED; // khởi tạo mutex để đảm bảo timing chính xác khi đọc dữ liệu từ DHT11
    portENTER_CRITICAL(&mux); // vào vùng critical để đảm bảo timing chính xác
    for(int i=0; i< 40; i++){
        if(wait_for_level(1, 100) == -1){ 
            portEXIT_CRITICAL(&mux); 
            return ESP_FAIL; // nếu DHT11 không kéo chân GPIO lên mức cao trong khoảng thời gian này, trả về lỗi
        }
        esp_rom_delay_us(40); // sau khi DHT11 kéo chân GPIO lên mức cao, MCU cần đợi khoảng 40us để xác định xem bit dữ liệu là 0 hay 1
        if(gpio_get_level(dht_gpio)){
            data[i/8] |= (1 << (7 - (i%8))); // nếu chân GPIO vẫn ở mức cao sau khoảng thời gian này, bit dữ liệu là 1, ngược lại là 0
            wait_for_level(0,100); // đợi DHT11 kéo chân GPIO xuống mức thấp để chuẩn bị cho bit dữ liệu tiếp theo
        }
    }
    portEXIT_CRITICAL(&mux); // thoát vùng critical sau khi đọc xong 40 bits dữ liệu
    // kiểm tra checksum
    if(data[4] == ((data[0] + data[1] + data[2] + data[3])&0xFF)){
        //& 0xFF: Đây là phép toán bitwise AND. Nó đảm bảo rằng nếu tổng 4 Byte đó vượt quá 255 (vượt quá kích thước của 1 Byte), 
        //thì nó chỉ lấy 8 bit thấp nhất để so sánh với Byte Checksum.
        reading->humidity = data[0]; // nếu checksum hợp lệ, lưu trữ giá trị độ ẩm và nhiệt độ vào cấu trúc dht11_reading_t
        reading-> temperature = data[2]; //neu checksum hợp lệ, lưu trữ giá trị độ ẩm và nhiệt độ vào cấu trúc dht11_reading_t
        return ESP_OK; // trả về thành công nếu đọc dữ liệu thành công và checksum hợp lệ
    }else{
        return ESP_ERR_INVALID_CRC; // trả về lỗi nếu checksum không hợp lệ
    }

}