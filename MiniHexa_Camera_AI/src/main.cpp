
#include <cstdio>
#include <cstdint>
#include <cstring>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/schema/schema_generated.h"

#include "model_data.h"
#include "esp_heap_caps.h"

#include "esp_camera.h"
#include "esp_err.h"

#include "driver/i2c_slave.h"

// Starting allocation; adjust if AllocateTensors fails.
// constexpr size_t kTensorArenaSize = 512 * 1024;
// alignas(16) static uint8_t tensor_arena[kTensorArenaSize];
constexpr size_t kTensorArenaSize = 512 * 1024;


static i2c_slave_dev_handle_t i2c_handle = nullptr;

bool initialize_i2c()
{
    i2c_slave_config_t config = {};
    config.i2c_port = I2C_NUM_0;
    config.sda_io_num = GPIO_NUM_47;
    config.scl_io_num = GPIO_NUM_48;
    config.clk_source = I2C_CLK_SRC_DEFAULT;
    config.send_buf_depth = 256;
    config.slave_addr = 0x52;
    config.addr_bit_len = I2C_ADDR_BIT_LEN_7;

    esp_err_t err = i2c_new_slave_device(&config, &i2c_handle);

    if (err != ESP_OK) {
        printf("ERROR: I2C initialization failed: %s\n",
               esp_err_to_name(err));
        return false;
    }

    printf("I2C slave initialized at address 0x52\n");
    return true;
}

static volatile uint8_t detection_flag = 0;

void i2c_test_task(void *arg)
{
    while (true) {
        uint8_t value = detection_flag;

        esp_err_t err = i2c_slave_transmit(
            i2c_handle,
            &value,
            1,
            1000
        );

        if (err != ESP_OK && err != ESP_ERR_TIMEOUT) {
            printf("I2C error: %s\n", esp_err_to_name(err));
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }
}



bool initialize_camera()
{
    camera_config_t config = {};

    config.pin_pwdn = -1;
    config.pin_reset = -1;

    config.pin_xclk = 15;
    config.pin_sccb_sda = 4;
    config.pin_sccb_scl = 5;

    config.pin_d0 = 11;
    config.pin_d1 = 9;
    config.pin_d2 = 8;
    config.pin_d3 = 10;
    config.pin_d4 = 12;
    config.pin_d5 = 18;
    config.pin_d6 = 17;
    config.pin_d7 = 16;

    config.pin_vsync = 6;
    config.pin_href = 7;
    config.pin_pclk = 13;

    config.xclk_freq_hz = 20000000;
    config.ledc_timer = LEDC_TIMER_0;
    config.ledc_channel = LEDC_CHANNEL_0;

    config.pixel_format = PIXFORMAT_RGB565;
    config.frame_size = FRAMESIZE_QQVGA;  // 160 x 120
    config.jpeg_quality = 12;
    config.fb_count = 1;
    config.fb_location = CAMERA_FB_IN_PSRAM;
    config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;

    esp_err_t err = esp_camera_init(&config);

    if (err != ESP_OK) {
        printf("ERROR: Camera initialization failed: 0x%x\n", err);
        return false;
    }

    printf("Camera initialized successfully!\n");
    return true;
}


bool camera_frame_to_input(
    const camera_fb_t* frame,
    TfLiteTensor* input)
{
    if (!frame || !input ||
        frame->format != PIXFORMAT_RGB565 ||
        frame->width == 0 || frame->height == 0 ||
        input->type != kTfLiteInt8 ||
        input->bytes != 96 * 96 * 3) {
        return false;
    }

    const int width = 96;
    const int height = 96;

    const float scale = input->params.scale;
    const int zero_point = input->params.zero_point;

    if (scale <= 0.0f) return false;

    auto quantize = [scale, zero_point](uint8_t value) -> int8_t {
        int q = static_cast<int>(
            value / scale + zero_point + 0.5f
        );

        if (q < -128) q = -128;
        if (q > 127) q = 127;

        return static_cast<int8_t>(q);
    };

    for (int y = 0; y < height; y++) {
        int src_y = y * frame->height / height;

        for (int x = 0; x < width; x++) {
            int src_x = x * frame->width / width;

            int src_index = (src_y * frame->width + src_x) * 2;

            uint16_t pixel =
                (static_cast<uint16_t>(frame->buf[src_index]) << 8) |
                frame->buf[src_index + 1];

            uint8_t r = ((pixel >> 11) & 0x1F) * 255 / 31;
            uint8_t g = ((pixel >> 5) & 0x3F) * 255 / 63;
            uint8_t b = (pixel & 0x1F) * 255 / 31;

            int dst_index = (y * width + x) * 3;

            input->data.int8[dst_index] = quantize(r);
            input->data.int8[dst_index + 1] = quantize(g);
            input->data.int8[dst_index + 2] = quantize(b);
        }
    }

    return true;
}


extern "C" void app_main(void)
{

    if (!initialize_camera()) {
        return;
    }


    if (!initialize_i2c()) {
        return;
    }
    xTaskCreate(
        i2c_test_task,
        "i2c_test",
        4096,
        nullptr,
        5,
        nullptr
    );

    camera_fb_t* frame = esp_camera_fb_get();

    if (frame == nullptr) {
        printf("ERROR: Camera capture failed\n");
        return;
    }

    printf("Captured frame: %u x %u\n",
        (unsigned)frame->width,
        (unsigned)frame->height);

    esp_camera_fb_return(frame);

    printf("MiniHexa Bag Detector - INT8 CNN\n");

    const tflite::Model* model = tflite::GetModel(model_data);

    if (model->version() != TFLITE_SCHEMA_VERSION) {
        printf("ERROR: Model schema mismatch\n");
        return;
    }

    tflite::MicroMutableOpResolver<5> resolver;

    resolver.AddConv2D();
    resolver.AddMaxPool2D();
    resolver.AddMean();
    resolver.AddFullyConnected();
    resolver.AddLogistic();

    uint8_t* tensor_arena = static_cast<uint8_t*>(
        heap_caps_aligned_alloc(
            16,
            kTensorArenaSize,
            MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT
        )
    );

    if (tensor_arena == nullptr) {
        printf("ERROR: PSRAM allocation failed\n");
        return;
    }

    printf("Tensor arena allocated in PSRAM\n");

    tflite::MicroInterpreter interpreter(
        model, resolver, tensor_arena, kTensorArenaSize
    );

    if (interpreter.AllocateTensors() != kTfLiteOk) {
        printf("ERROR: Tensor allocation failed\n");
        return;
    }

    TfLiteTensor* input = interpreter.input(0);

    if (input->type != kTfLiteInt8) {
        printf("ERROR: Expected INT8 input\n");
        return;
    }

    printf("Input dimensions: ");
    for (int i = 0; i < input->dims->size; i++) {
        printf("%d ", input->dims->data[i]);
    }
    printf("\n");

    // Temporary test image: uniform white RGB pixels.
    // This is NOT camera input yet.


    while (true) {
        camera_fb_t* frame = esp_camera_fb_get();

        if (frame == nullptr) {
            printf("ERROR: Camera capture failed\n");
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        bool converted = camera_frame_to_input(frame, input);
        esp_camera_fb_return(frame);

        if (!converted) {
            printf("ERROR: Frame conversion failed\n");
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        if (interpreter.Invoke() == kTfLiteOk) {
            TfLiteTensor* output = interpreter.output(0);

            float probability =
                (output->data.int8[0] - output->params.zero_point) *
                output->params.scale;
            
            detection_flag = (probability >= 0.70f) ? 1 : 0;

            bool bag_detected = detection_flag == 1;

            printf("Bag probability: %.3f | Confirmed: %s\n",
                probability,
                bag_detected ? "YES" : "NO");
        } else {
            printf("ERROR: Inference failed\n");
        }

        vTaskDelay(pdMS_TO_TICKS(100));
    }

}
