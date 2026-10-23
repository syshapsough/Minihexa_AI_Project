
/*
 * ============================================================
 * MiniHexa - Camera-Based Punching Bag Detection
 * ============================================================
 *
 * Hardware: ESP32-S3 with OV2640 camera
 * Framework: ESP-IDF
 * AI Framework: TensorFlow Lite Micro
 *
 * Description:
 * Captures images using the camera, preprocesses them,
 * and runs a quantized CNN to detect a punching bag.
 *
 * The detection result is transmitted to the MiniHexa
 * controller over I2C:
 *
 *   1 = Punching bag detected
 *   0 = Punching bag not detected
 *
 * ============================================================
 */


/* ============================================================
 * SECTION 1: LIBRARIES AND GLOBAL CONFIGURATION
 * ============================================================ */

// Standard C/C++ libraries
#include <cstdio>
#include <cstdint>
#include <cstring>

// FreeRTOS task management
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// TensorFlow Lite Micro inference engine
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/schema/schema_generated.h"

// Trained neural network stored as a C array
#include "model_data.h"

// ESP32 memory allocation
#include "esp_heap_caps.h"

// ESP32 camera driver
#include "esp_camera.h"
#include "esp_err.h"

// ESP-IDF I2C slave driver
#include "driver/i2c_slave.h"

// Memory reserved for neural network tensors.
// The arena is allocated dynamically in external PSRAM.
constexpr size_t kTensorArenaSize = 512 * 1024;

// Handle representing the I2C slave device
static i2c_slave_dev_handle_t i2c_handle = nullptr;

// Shared detection result.
// Updated by the inference loop and read by the I2C task.
static volatile uint8_t detection_flag = 0;


/* ============================================================
 * SECTION 2: I2C COMMUNICATION INITIALIZATION
 * ============================================================ */

/*
 * initialize_i2c()
 *
 * Configures the ESP32-S3 as an I2C slave device.
 *
 * The MiniHexa controller acts as the I2C master and
 * requests the latest detection result.
 *
 * Configuration:
 *   I2C port      : I2C_NUM_0
 *   SDA           : GPIO 47
 *   SCL           : GPIO 48
 *   Slave address : 0x52
 *
 * Returns:
 *   true  - Initialization successful
 *   false - Initialization failed
 */
bool initialize_i2c()
{
    i2c_slave_config_t config = {};

    config.i2c_port = I2C_NUM_0;
    config.sda_io_num = GPIO_NUM_47;
    config.scl_io_num = GPIO_NUM_48;
    config.clk_source = I2C_CLK_SRC_DEFAULT;

    // Internal transmission buffer size
    config.send_buf_depth = 256;

    // 7-bit I2C slave address
    config.slave_addr = 0x52;
    config.addr_bit_len = I2C_ADDR_BIT_LEN_7;

    // Register the I2C slave device
    esp_err_t err = i2c_new_slave_device(
        &config,
        &i2c_handle
    );

    if (err != ESP_OK) {
        printf("ERROR: I2C initialization failed: %s\n",
               esp_err_to_name(err));
        return false;
    }

    printf("I2C slave initialized at address 0x52\n");

    return true;
}


/* ============================================================
 * SECTION 3: I2C DETECTION RESULT TRANSMISSION
 * ============================================================ */

/*
 * i2c_test_task()
 *
 * FreeRTOS task responsible for transmitting the latest
 * detection result to the MiniHexa controller.
 *
 * The transmitted message contains a single byte:
 *
 *   0x00 -> No punching bag detected
 *   0x01 -> Punching bag detected
 *
 * The task runs independently of the camera inference loop.
 *
 * Note:
 * i2c_slave_transmit() queues data for an I2C master read.
 * It does not initiate communication with the controller.
 */
void i2c_test_task(void *arg)
{
    while (true) {

        // Copy the latest detection result
        uint8_t value = detection_flag;

        // Queue one byte for transmission to the I2C master
        esp_err_t err = i2c_slave_transmit(
            i2c_handle,
            &value,
            1,
            1000
        );

        // Report errors other than a timeout
        if (err != ESP_OK && err != ESP_ERR_TIMEOUT) {
            printf("I2C error: %s\n",
                   esp_err_to_name(err));
        }

        // Delay before preparing the next transmission
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}


/* ============================================================
 * SECTION 4: CAMERA INITIALIZATION
 * ============================================================ */

/*
 * initialize_camera()
 *
 * Configures and initializes the OV2640 camera.
 *
 * Camera settings:
 *   Pixel format : RGB565
 *   Resolution   : QQVGA (160 x 120)
 *   Frame buffers: 1
 *   Buffer memory: PSRAM
 *
 * RGB565 stores each pixel using 16 bits:
 *   Red   : 5 bits
 *   Green : 6 bits
 *   Blue  : 5 bits
 *
 * Returns:
 *   true  - Camera initialized successfully
 *   false - Camera initialization failed
 */
bool initialize_camera()
{
    camera_config_t config = {};

    // Power-down and reset pins are not connected
    config.pin_pwdn = -1;
    config.pin_reset = -1;

    // Camera clock and configuration interface
    config.pin_xclk = 15;
    config.pin_sccb_sda = 4;
    config.pin_sccb_scl = 5;

    // Camera parallel data pins
    config.pin_d0 = 11;
    config.pin_d1 = 9;
    config.pin_d2 = 8;
    config.pin_d3 = 10;
    config.pin_d4 = 12;
    config.pin_d5 = 18;
    config.pin_d6 = 17;
    config.pin_d7 = 16;

    // Camera synchronization signals
    config.pin_vsync = 6;
    config.pin_href = 7;
    config.pin_pclk = 13;

    // External camera clock: 20 MHz
    config.xclk_freq_hz = 20000000;

    // LEDC peripheral generates the camera clock
    config.ledc_timer = LEDC_TIMER_0;
    config.ledc_channel = LEDC_CHANNEL_0;

    // Capture raw RGB565 images
    config.pixel_format = PIXFORMAT_RGB565;

    // QQVGA resolution: 160 x 120 pixels
    config.frame_size = FRAMESIZE_QQVGA;

    // JPEG quality is unused when capturing RGB565
    config.jpeg_quality = 12;

    // Use one frame buffer stored in PSRAM
    config.fb_count = 1;
    config.fb_location = CAMERA_FB_IN_PSRAM;

    // Capture frames when the buffer is available
    config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;

    // Initialize the camera driver
    esp_err_t err = esp_camera_init(&config);

    if (err != ESP_OK) {
        printf("ERROR: Camera initialization failed: 0x%x\n",
               err);
        return false;
    }

    printf("Camera initialized successfully!\n");

    return true;
}


/* ============================================================
 * SECTION 5: IMAGE PREPROCESSING AND QUANTIZATION
 * ============================================================ */

/*
 * camera_frame_to_input()
 *
 * Converts a captured RGB565 camera frame into the input
 * format expected by the neural network.
 *
 * Processing steps:
 *
 * 1. Validate the camera frame and model input tensor.
 * 2. Resize the image from 160x120 to 96x96.
 * 3. Convert each RGB565 pixel to RGB888.
 * 4. Quantize RGB values to signed INT8.
 * 5. Store the result directly in the model input tensor.
 *
 * The resize operation uses nearest-neighbor sampling.
 *
 * Model input:
 *   Resolution : 96 x 96
 *   Channels   : 3 (RGB)
 *   Data type  : INT8
 *
 * Returns:
 *   true  - Frame converted successfully
 *   false - Invalid frame or incompatible model input
 */
bool camera_frame_to_input(
    const camera_fb_t* frame,
    TfLiteTensor* input)
{
    // Validate frame properties and model input format
    if (!frame || !input ||
        frame->format != PIXFORMAT_RGB565 ||
        frame->width == 0 || frame->height == 0 ||
        input->type != kTfLiteInt8 ||
        input->bytes != 96 * 96 * 3) {
        return false;
    }

    // Neural network input resolution
    const int width = 96;
    const int height = 96;

    // Quantization parameters stored in the TFLite model
    const float scale = input->params.scale;
    const int zero_point = input->params.zero_point;

    if (scale <= 0.0f) {
        return false;
    }

    /*
     * Convert an unsigned RGB channel value into INT8.
     *
     * Quantization formula:
     *
     *   q = round(value / scale) + zero_point
     *
     * The result is restricted to the INT8 range:
     *   -128 to 127
     *
     * This implementation uses +0.5 before truncation,
     * so negative values are not rounded symmetrically.
     */
    auto quantize = [scale, zero_point](uint8_t value) -> int8_t {

        int q = static_cast<int>(
            value / scale + zero_point + 0.5f
        );

        // Clamp the quantized value to the INT8 range
        if (q < -128) q = -128;
        if (q > 127) q = 127;

        return static_cast<int8_t>(q);
    };

    // Iterate over each pixel of the 96x96 model input
    for (int y = 0; y < height; y++) {

        // Map destination row to original camera frame
        int src_y = y * frame->height / height;

        for (int x = 0; x < width; x++) {

            // Map destination column to original frame
            int src_x = x * frame->width / width;

            // Each RGB565 pixel occupies two bytes
            int src_index =
                (src_y * frame->width + src_x) * 2;

            // Reconstruct the 16-bit RGB565 pixel
            uint16_t pixel =
                (static_cast<uint16_t>(
                    frame->buf[src_index]) << 8) |
                frame->buf[src_index + 1];

            // Extract and expand RGB565 color channels
            // into the 0-255 RGB888 range
            uint8_t r =
                ((pixel >> 11) & 0x1F) * 255 / 31;

            uint8_t g =
                ((pixel >> 5) & 0x3F) * 255 / 63;

            uint8_t b =
                (pixel & 0x1F) * 255 / 31;

            // Position of this pixel in the input tensor
            int dst_index = (y * width + x) * 3;

            // Quantize and store the RGB channels
            input->data.int8[dst_index] = quantize(r);
            input->data.int8[dst_index + 1] = quantize(g);
            input->data.int8[dst_index + 2] = quantize(b);
        }
    }

    return true;
}


/* ============================================================
 * SECTION 6: SYSTEM INITIALIZATION
 * ============================================================ */

/*
 * app_main()
 *
 * Main entry point of the ESP-IDF application.
 *
 * Initialization sequence:
 *
 * 1. Initialize the camera.
 * 2. Initialize I2C communication.
 * 3. Start the I2C transmission task.
 * 4. Test camera capture.
 * 5. Load and validate the neural network.
 * 6. Allocate memory for inference.
 * 7. Start continuous detection.
 */
extern "C" void app_main(void)
{
    // Initialize the camera before I2C communication
    if (!initialize_camera()) {
        return;
    }

    // Initialize the I2C slave interface
    if (!initialize_i2c()) {
        return;
    }

    // Create a FreeRTOS task for I2C transmission
    xTaskCreate(
        i2c_test_task,    // Task function
        "i2c_test",      // Task name
        4096,            // Stack size in bytes
        nullptr,         // No task parameters
        5,               // Task priority
        nullptr          // No task handle required
    );

    // Capture a test frame to verify camera operation
    camera_fb_t* frame = esp_camera_fb_get();

    if (frame == nullptr) {
        printf("ERROR: Camera capture failed\n");
        return;
    }

    printf("Captured frame: %u x %u\n",
           (unsigned)frame->width,
           (unsigned)frame->height);

    // Release the test frame buffer
    esp_camera_fb_return(frame);

    printf("MiniHexa Bag Detector - INT8 CNN\n");


    /* ========================================================
     * SECTION 7: TENSORFLOW LITE MICRO SETUP
     * ======================================================== */

    /*
     * Load the trained neural network.
     *
     * model_data is a byte array defined in model_data.h.
     * It contains the converted TFLite model.
     */
    const tflite::Model* model =
        tflite::GetModel(model_data);

    // Verify compatibility with the TFLite schema
    if (model->version() != TFLITE_SCHEMA_VERSION) {
        printf("ERROR: Model schema mismatch\n");
        return;
    }

    /*
     * Register the neural network operations.
     *
     * Only the operations required by this model are
     * included to reduce memory usage.
     *
     * The current CNN requires five operations:
     *   Conv2D
     *   MaxPool2D
     *   Mean
     *   FullyConnected
     *   Logistic
     *
     * Different model architectures may require
     * additional or different operations.
     */
    tflite::MicroMutableOpResolver<5> resolver;

    resolver.AddConv2D();
    resolver.AddMaxPool2D();
    resolver.AddMean();
    resolver.AddFullyConnected();
    resolver.AddLogistic();

    /*
     * Allocate the tensor arena in external PSRAM.
     *
     * The arena stores intermediate tensors and temporary
     * memory used during neural network inference.
     *
     * Size      : 512 KB
     * Alignment : 16 bytes
     */
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

    // Create the TensorFlow Lite Micro interpreter
    tflite::MicroInterpreter interpreter(
        model,
        resolver,
        tensor_arena,
        kTensorArenaSize
    );

    // Allocate memory for model tensors
    if (interpreter.AllocateTensors() != kTfLiteOk) {
        printf("ERROR: Tensor allocation failed\n");
        return;
    }

    // Get the model's first input tensor
    TfLiteTensor* input = interpreter.input(0);

    // Verify that the model uses INT8 input
    if (input->type != kTfLiteInt8) {
        printf("ERROR: Expected INT8 input\n");
        return;
    }

    // Display the model's input dimensions
    printf("Input dimensions: ");

    for (int i = 0; i < input->dims->size; i++) {
        printf("%d ", input->dims->data[i]);
    }

    printf("\n");


    /* ========================================================
     * SECTION 8: CONTINUOUS CAMERA INFERENCE
     * ======================================================== */

    /*
     * Main detection loop:
     *
     * 1. Capture a camera frame.
     * 2. Convert the frame into INT8 model input.
     * 3. Execute the neural network.
     * 4. Read the output probability.
     * 5. Update the detection flag.
     * 6. Repeat.
     *
     * The I2C task independently transmits the latest
     * detection flag to the MiniHexa controller.
     */
    while (true) {

        // Capture a new camera frame
        camera_fb_t* frame = esp_camera_fb_get();

        if (frame == nullptr) {
            printf("ERROR: Camera capture failed\n");

            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        // Resize, convert, and quantize the image
        bool converted =
            camera_frame_to_input(frame, input);

        // Return the frame buffer after preprocessing
        esp_camera_fb_return(frame);

        if (!converted) {
            printf("ERROR: Frame conversion failed\n");

            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        // Run inference using the trained CNN
        if (interpreter.Invoke() == kTfLiteOk) {

            // Retrieve the first output tensor
            TfLiteTensor* output = interpreter.output(0);

            /*
             * Convert the INT8 model output into a
             * floating-point probability.
             *
             * Dequantization formula:
             *
             * probability =
             *   (quantized_value - zero_point) * scale
             */
            float probability =
                (output->data.int8[0] -
                 output->params.zero_point) *
                output->params.scale;

            /*
             * Apply the detection threshold.
             *
             * If probability >= 0.70:
             *   detection_flag = 1
             *
             * Otherwise:
             *   detection_flag = 0
             *
             * This assumes output[0] represents the
             * probability of the punching bag class.
             */
            detection_flag =
                (probability >= 0.70f) ? 1 : 0;

            // Convert the flag into a readable result
            bool bag_detected = detection_flag == 1;

            // Print the latest inference result
            printf(
                "Bag probability: %.3f | Confirmed: %s\n",
                probability,
                bag_detected ? "YES" : "NO"
            );

        } else {
            printf("ERROR: Inference failed\n");
        }

        // Delay before processing the next camera frame
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}
