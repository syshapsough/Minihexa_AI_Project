// // // // // // // #include <Arduino.h>
// // // // // // // #include "hiwonder_robot.h"

// // // // // // // Robot minihexa;
// // // // // // // uint8_t red[4] = {0};

// // // // // // // void setup() {
// // // // // // //   Serial.begin(115200);
// // // // // // //   minihexa.begin();
// // // // // // //   delay(200);
// // // // // // // Serial.println("MiniHexa camera I2C test - NO walking commands");
// // // // // // // }

// // // // // // // void loop() {
// // // // // // //   red[0] = red[1] = red[2] = red[3] = 0;
// // // // // // //   // Official camera protocol: I2C 0x52, register 0xA1, 4 bytes.
// // // // // // //   uint16_t received = minihexa.sensor.camera.region2_red_block_detection(red, sizeof(red));
// // // // // // //   if (received == 4) {
// // // // // // //     Serial.printf("Red: x=%u y=%u width=%u length=%u\n", red[0], red[1], red[2], red[3]);
// // // // // // //     if (red[2] == 0 || red[3] == 0) Serial.println("No red ribbon detected");
// // // // // // //   } else {
// // // // // // //     Serial.printf("Camera I2C read failed (received=%u)\n", received);
// // // // // // //   }
// // // // // // //   delay(300);
// // // // // // // }
// // // // // // #include <Arduino.h>
// // // // // // #include <Wire.h>

// // // // // // void setup() {
// // // // // //     Serial.begin(115200);
// // // // // //     delay(2000);

// // // // // //     Wire.begin(22, 23);
// // // // // //     Serial.println("Scanning I2C bus...");
// // // // // // }

// // // // // // void loop() {
// // // // // //     for (uint8_t address = 1; address < 127; address++) {
// // // // // //         Wire.beginTransmission(address);

// // // // // //         if (Wire.endTransmission() == 0) {
// // // // // //             Serial.printf("Found I2C device at 0x%02X\n", address);
// // // // // //         }
// // // // // //     }

// // // // // //     Serial.println("Scan complete.");
// // // // // //     delay(3000);
// // // // // // }

// // // // // #include <Arduino.h>
// // // // // #include <Wire.h>

// // // // // #include "hiwonder_robot.h"

// // // // // Robot minihexa;

// // // // // constexpr uint8_t CAMERA_ADDR = 0x52;

// // // // // int lastDetection = -1;

// // // // // void setup() {
// // // // //     Serial.begin(115200);
// // // // //     Wire.begin(22, 23);

// // // // //     delay(1000);
// // // // //     Serial.println("MiniHexa AI camera ready!");
// // // // // }

// // // // // void loop() {
// // // // //     int received = Wire.requestFrom(CAMERA_ADDR, (uint8_t)1);

// // // // //     if (received == 1 && Wire.available()) {
// // // // //         int detection = Wire.read();

// // // // //         if (detection == 0 || detection == 1) {
// // // // //             Serial.printf("Camera returned: %d\n", detection);
// // // // //         } else {
// // // // //             Serial.printf("Invalid camera value: %d\n", detection);
// // // // //         }
// // // // //     } else {
// // // // //         Serial.println("Camera read failed");
// // // // //     }

// // // // //     delay(100);
// // // // // }

// // // #include <Arduino.h>
// // // #include "hiwonder_robot.h"

// // // Robot minihexa;

// // // Velocity_t velocity = {0.0f, 0.0f, 0.0f};
// // // Vector_t position = {0.0f, 0.0f, 0.0f};
// // // Euler_t euler = {0.0f, 0.0f, 0.0f};

// // // bool movementStarted = false;
// // // bool movementFinished = false;

// // // unsigned long startTime = 0;

// // // void setup()
// // // {
// // //     Serial.begin(115200);

// // //     minihexa.begin();

// // //     Serial.println("MiniHexa initialized");
// // //     Serial.println("Starting movement test in 3 seconds");

// // //     startTime = millis();
// // // }

// // // void loop()
// // // {
// // //     // Robot movement is updated by the library's internal timer.

// // //     if (!movementStarted && millis() - startTime >= 3000)
// // //     {
// // //         Serial.println("Walking forward");

// // //         velocity = {0.0f, 1.0f, 0.0f};

// // //         minihexa.move(
// // //             &velocity,
// // //             &position,
// // //             &euler,
// // //             1000,
// // //             2
// // //         );

// // //         movementStarted = true;
// // //     }

// // //     if (movementStarted &&
// // //         !movementFinished &&
// // //         minihexa.move_state == Robot::REST)
// // //     {
// // //         Serial.println("Movement completed");
// // //         movementFinished = true;
// // //     }

// // //     delay(20);
// // // }



// // // // #include <Arduino.h>
// // // // #include "hiwonder_robot.h"

// // // // Robot minihexa;

// // // // // Rough estimate for a 7.4V (2-cell) Li-ion battery
// // // // int batteryPercentage(float voltage)
// // // // {
// // // //     float percentage = (voltage - 7.0f) / (8.4f - 7.0f) * 100.0f;
// // // //     return constrain((int)roundf(percentage), 0, 100);
// // // // }

// // // // void setup()
// // // // {
// // // //     Serial.begin(115200);
// // // //     minihexa.begin();

// // // //     Serial.println("MiniHexa Battery Monitor");
// // // //     delay(2000);
// // // // }

// // // // void loop()
// // // // {
// // // //     float voltage = minihexa.board.bat_voltage / 1000.0f;

// // // //     Serial.printf("Battery: %.2f V | Estimated charge: %d%%\n",
// // // //                   voltage,
// // // //                   batteryPercentage(voltage));

// // // //     delay(1000);
// // // // }




// #include <Arduino.h>
// #include <Wire.h>
// #include "hiwonder_robot.h"

// Robot minihexa;

// // Configuration
// #define CAMERA_ADDR     0x52
// #define STOP_DISTANCE   150    // 10 cm in mm
// #define RESET_DISTANCE  150    // 15 cm in mm
// #define COMBAT_ACTION   7      // Hiwonder Combat 1

// Velocity_t velocity = {0.0f, 0.0f, 0.0f};
// Vector_t position = {0.0f, 0.0f, 0.0f};
// Euler_t euler = {0.0f, 0.0f, 0.0f};

// bool walking = false;
// bool attacked = false;

// unsigned long lastCheck = 0;

// void stopWalking()
// {
//     if (!walking) return;

//     velocity = {0.0f, 0.0f, 0.0f};
//     minihexa.move(&velocity, &position, &euler, 400);

//     walking = false;
//     delay(450);
// }

// void startWalking()
// {
//     if (walking) return;

//     velocity = {0.0f, 1.0f, 0.0f};
//     euler = {0.0f, 0.0f, 0.0f};

//     minihexa.move(
//         &velocity,
//         &position,
//         &euler,
//         1000,
//         -1
//     );

//     walking = true;

//     Serial.println("APPROACHING TARGET");
// }

// void attack()
// {
//     Serial.println("COMBAT MODE ACTIVATED!");

//     minihexa.action_group_run(COMBAT_ACTION);

//     Serial.println("ATTACK COMPLETED");
// }

// void setup()
// {
//     Serial.begin(115200);

//     minihexa.begin();

//     Serial.println("MiniHexa AI Boxing Robot");
//     Serial.println("Checking available action groups...");

//     minihexa.list_action_group_dir();

//     Serial.println("Waiting for target...");
// }

// void loop()
// {
//     if (millis() - lastCheck < 100) return;
//     lastCheck = millis();

//     // Read AI camera detection
//     uint8_t detected = 0;

//     if (Wire.requestFrom((uint8_t)CAMERA_ADDR,
//                          (uint8_t)1) == 1)
//     {
//         detected = Wire.read();
//     }

//     // Read ultrasonic distance in mm
//     uint16_t distance = minihexa.sensor.get_distance();

//     Serial.printf(
//         "AI: %d | Distance: %d mm\n",
//         detected,
//         distance
//     );

//     // Invalid distance: stop
//     if (distance == 0)
//     {
//         stopWalking();
//         return;
//     }

//     // Reset attack when target moves away
//     if (distance >= RESET_DISTANCE)
//     {
//         attacked = false;
//     }

//     // Target detected and within attack range
//     if (detected == 1 && distance <= STOP_DISTANCE)
//     {
//         stopWalking();

//         if (!attacked)
//         {
//             attacked = true;
//             attack();
//         }

//         return;
//     }

//     // Stop if target is lost or already attacked
//     if (detected != 1 || attacked)
//     {
//         stopWalking();
//         return;
//     }

//     // Target detected and farther than 10 cm
//     if (distance > STOP_DISTANCE)
//     {
//         startWalking();
//     }
// }

#include <Arduino.h>
#include <Wire.h>
#include "hiwonder_robot.h"

Robot minihexa;

constexpr uint8_t CAMERA_ADDR = 0x52;
constexpr uint8_t COMBAT_ACTION = 8; // Change to 7 for Combat 1
constexpr uint16_t STOP_DISTANCE_MM = 150;
constexpr uint16_t RESET_DISTANCE_MM = 200;
constexpr unsigned long CHECK_INTERVAL_MS = 100;

Velocity_t velocity = {0.0f, 0.0f, 0.0f};
Vector_t position = {0.0f, 0.0f, 0.0f};
Euler_t euler = {0.0f, 0.0f, 0.0f};

bool walking = false;
bool attacked = false;
unsigned long lastCheck = 0;

void stopWalking() {
    if (!walking) return;
    velocity = {0.0f, 0.0f, 0.0f};
    minihexa.move(&velocity, &position, &euler, 400);
    walking = false;
    delay(450);
}

void startWalking() {
    if (walking) return;
    velocity = {0.0f, 1.0f, 0.0f};
    euler = {0.0f, 0.0f, 0.0f};
    minihexa.move(&velocity, &position, &euler, 1000, -1);
    walking = true;
    Serial.println("APPROACHING TARGET");
}

int readDetection() {
    if (Wire.requestFrom(CAMERA_ADDR, (uint8_t)1) != 1 || !Wire.available()) {
        return -1; // Camera communication failed
    }
    int value = Wire.read();
    return (value == 0 || value == 1) ? value : -1;
}

void setup() {
    Serial.begin(115200);
    minihexa.begin();
    Wire.begin(22, 23);
    delay(1000);
    Serial.println("MiniHexa AI Boxing Robot ready");
    minihexa.list_action_group_dir();
}

void loop() {
    if (millis() - lastCheck < CHECK_INTERVAL_MS) return;
    lastCheck = millis();

    int detected = readDetection();
    uint16_t distance = minihexa.sensor.get_distance();
    Serial.printf("AI: %d | Distance: %u mm\n", detected, distance);

    if (detected != 1 || distance == 0) {
        stopWalking();
        return;
    }

    if (distance >= RESET_DISTANCE_MM) attacked = false;

    if (distance <= STOP_DISTANCE_MM) {
        stopWalking();
        if (!attacked) {
            attacked = true;
            Serial.println("COMBAT MODE ACTIVATED!");
            minihexa.action_group_run(COMBAT_ACTION);
            Serial.println("COMBAT COMPLETE");
            // The action group ends in its final pose; next move() resumes gait control.
        }
        return;
    }

    if (!attacked) startWalking();
    else stopWalking();
    delay(100);
}
