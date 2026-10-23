
/*
 * ============================================================
 * MiniHexa - Autonomous Punching Bag Tracking and Combat
 * ============================================================
 *
 * Hardware: Hiwonder MiniHexa Robot
 * Framework: Arduino
 *
 * Description:
 * Controls the MiniHexa robot using detection results
 * received from an ESP32-S3 camera over I2C.
 *
 * The robot uses:
 *   - Camera AI detection to identify the punching bag.
 *   - Ultrasonic distance sensing to approach the target.
 *   - Predefined action groups to perform combat movements.
 *
 * Robot behavior:
 *
 *   1. Read the camera detection result.
 *   2. Measure the distance to the target.
 *   3. Walk forward when the target is detected.
 *   4. Stop when the target is sufficiently close.
 *   5. Execute the selected combat action.
 *
 * ============================================================
 */


/* ============================================================
 * SECTION 1: LIBRARIES AND GLOBAL CONFIGURATION
 * ============================================================ */

// Arduino framework and standard functions
#include <Arduino.h>

// Arduino I2C communication library
#include <Wire.h>

// Hiwonder MiniHexa robot control library
#include "hiwonder_robot.h"

// Create the robot control object
Robot minihexa;

/*
 * System configuration
 *
 * CAMERA_ADDR:
 *   I2C address of the ESP32-S3 camera module.
 *
 * COMBAT_ACTION:
 *   Action group executed when the target is reached.
 *   7 = Combat 1
 *   8 = Combat 2
 *
 * STOP_DISTANCE_MM:
 *   Distance at which the robot stops and attacks.
 *
 * RESET_DISTANCE_MM:
 *   Distance required to reset the attack state.
 *
 * CHECK_INTERVAL_MS:
 *   Minimum interval between sensor checks.
 */
constexpr uint8_t CAMERA_ADDR = 0x52;
constexpr uint8_t COMBAT_ACTION = 8; // Change to 7 for Combat 1

constexpr uint16_t STOP_DISTANCE_MM = 150;
constexpr uint16_t RESET_DISTANCE_MM = 200;

constexpr unsigned long CHECK_INTERVAL_MS = 100;

/*
 * Robot movement parameters
 *
 * Velocity_t:
 *   Controls translational and rotational movement.
 *
 * Vector_t:
 *   Controls the robot's body position.
 *
 * Euler_t:
 *   Controls the robot's body orientation.
 *
 * All values are initially set to zero.
 */
Velocity_t velocity = {0.0f, 0.0f, 0.0f};
Vector_t position = {0.0f, 0.0f, 0.0f};
Euler_t euler = {0.0f, 0.0f, 0.0f};

/*
 * Robot state variables
 *
 * walking:
 *   Indicates whether the robot is currently walking.
 *
 * attacked:
 *   Prevents the combat action from repeating continuously
 *   while the robot remains close to the target.
 *
 * lastCheck:
 *   Stores the time of the previous sensor check.
 */
bool walking = false;
bool attacked = false;

unsigned long lastCheck = 0;


/* ============================================================
 * SECTION 2: STOP ROBOT MOVEMENT
 * ============================================================ */

/*
 * stopWalking()
 *
 * Stops the robot's walking motion.
 *
 * The function:
 *   1. Checks whether the robot is currently walking.
 *   2. Sets all velocity components to zero.
 *   3. Sends the updated movement command.
 *   4. Updates the walking state.
 *
 * The delay allows the robot to complete the transition
 * from walking to a stationary pose.
 */
void stopWalking()
{
    // Avoid sending unnecessary stop commands
    if (!walking) {
        return;
    }

    // Set movement velocity to zero
    velocity = {0.0f, 0.0f, 0.0f};

    // Command the robot to stop over 400 ms
    minihexa.move(
        &velocity,
        &position,
        &euler,
        400
    );

    // Update the movement state
    walking = false;

    // Allow the stopping movement to complete
    delay(450);
}


/* ============================================================
 * SECTION 3: START ROBOT MOVEMENT
 * ============================================================ */

/*
 * startWalking()
 *
 * Commands the robot to walk forward toward the target.
 *
 * The function:
 *   1. Checks whether the robot is already walking.
 *   2. Sets the forward movement velocity.
 *   3. Resets the body orientation.
 *   4. Starts continuous walking.
 *
 * The final argument (-1) requests continuous movement
 * rather than a fixed number of gait cycles.
 */
void startWalking()
{
    // Do not restart the gait if already walking
    if (walking) {
        return;
    }

    // Set forward walking velocity
    velocity = {0.0f, 1.0f, 0.0f};

    // Keep the robot's body orientation neutral
    euler = {0.0f, 0.0f, 0.0f};

    // Start continuous forward movement
    minihexa.move(
        &velocity,
        &position,
        &euler,
        1000,
        -1
    );

    // Update the movement state
    walking = true;

    Serial.println("APPROACHING TARGET");
}


/* ============================================================
 * SECTION 4: READ CAMERA DETECTION RESULT
 * ============================================================ */

/*
 * readDetection()
 *
 * Requests the latest object detection result from the
 * ESP32-S3 camera module using I2C.
 *
 * Communication configuration:
 *   Protocol      : I2C
 *   Controller    : MiniHexa (Master)
 *   Camera module : ESP32-S3 (Slave)
 *   Slave address : 0x52
 *   Message size  : 1 byte
 *
 * Expected values:
 *
 *   1  -> Punching bag detected
 *   0  -> Punching bag not detected
 *  -1  -> Communication error or invalid response
 *
 * Returns:
 *   int containing the detection status.
 */
int readDetection()
{
    // Request one byte from the camera module
    if (Wire.requestFrom(CAMERA_ADDR, (uint8_t)1) != 1 ||
        !Wire.available()) {

        // Camera communication failed
        return -1;
    }

    // Read the detection result
    int value = Wire.read();

    // Accept only valid detection values (0 or 1)
    return (value == 0 || value == 1) ? value : -1;
}


/* ============================================================
 * SECTION 5: SYSTEM INITIALIZATION
 * ============================================================ */

/*
 * setup()
 *
 * Runs once when the controller starts.
 *
 * Initialization sequence:
 *
 * 1. Initialize serial communication.
 * 2. Initialize the MiniHexa robot.
 * 3. Configure I2C communication.
 * 4. Allow the hardware to stabilize.
 * 5. Display the available action groups.
 */
void setup()
{
    // Initialize serial monitoring at 115200 baud
    Serial.begin(115200);

    // Initialize the MiniHexa control system
    minihexa.begin();

    /*
     * Initialize the I2C master interface.
     *
     * SDA = GPIO 22
     * SCL = GPIO 23
     *
     * These pins connect to the camera module's
     * corresponding I2C pins.
     */
    Wire.begin(22, 23);

    // Allow the system to initialize
    delay(1000);

    Serial.println("MiniHexa AI Boxing Robot ready");

    // Display available robot action groups
    minihexa.list_action_group_dir();
}



/* ============================================================
 * SECTION 6: ROBOT DECISION TABLE
 * ============================================================
 *
 * The robot combines camera detection and ultrasonic
 * distance measurements to determine its next action.
 *
 * +------------+-------------+-------------+----------------------+
 * | Camera     | Distance    | Attacked    | Robot Action         |
 * +------------+-------------+-------------+----------------------+
 * | 0          | Any         | Any         | Stop walking         |
 * | -1         | Any         | Any         | Stop (I2C error)     |
 * | 1          | 0 mm        | Any         | Stop (invalid range) |
 * | 1          | >= 200 mm   | Any         | Reset and approach   |
 * | 1          | 151-199 mm  | false       | Approach target      |
 * | 1          | <= 150 mm   | false       | Stop and attack      |
 * | 1          | 151-199 mm  | true        | Remain stationary    |
 * | 1          | <= 150 mm   | true        | Remain stationary    |
 * +------------+-------------+-------------+----------------------+
 *
 * Camera values:
 *   1  = Punching bag detected
 *   0  = Punching bag not detected
 *  -1  = I2C communication error
 *
 * Distance thresholds:
 *   STOP_DISTANCE_MM  = 150 mm
 *   RESET_DISTANCE_MM = 200 mm
 *
 * The difference between the two thresholds creates
 * hysteresis, preventing repeated attacks when the
 * measured distance fluctuates near the stopping point.
 *
 * NOTE:
 * The robot does not actively search for a lost target.
 * If detection fails, it stops until the target is
 * detected again.
 *
 * ============================================================ */

void loop()
{
    /*
     * Limit the frequency of sensor checks.
     *
     * millis() returns the elapsed time since startup.
     */
    if (millis() - lastCheck < CHECK_INTERVAL_MS) {
        return;
    }

    // Record the current check time
    lastCheck = millis();

    // Read the latest AI detection result
    int detected = readDetection();

    // Read the ultrasonic distance sensor (millimeters)
    uint16_t distance = minihexa.sensor.get_distance();

    // Display the sensor readings
    Serial.printf(
        "AI: %d | Distance: %u mm\n",
        detected,
        distance
    );

    /*
     * Validate the detection and distance measurements.
     *
     * Stop if:
     *   - No punching bag is detected.
     *   - Camera communication fails.
     *   - Distance sensor returns zero.
     *
     * A zero distance is treated as an invalid reading.
     */
    if (detected != 1 || distance == 0) {

        stopWalking();

        return;
    }


    /* ========================================================
     * SECTION 7: MOVEMENT AND COMBAT DECISION-MAKING
     * ======================================================== */

    /*
     * Reset the attack state when the target is far enough.
     *
     * This allows the robot to perform another combat
     * action after the target has moved away.
     *
     * The difference between STOP_DISTANCE_MM and
     * RESET_DISTANCE_MM introduces hysteresis.
     *
     * This helps prevent repeated attacks caused by
     * small fluctuations in distance measurements.
     */
    if (distance >= RESET_DISTANCE_MM) {
        attacked = false;
    }

    /*
     * CASE 1: Target is within attacking distance.
     *
     * Stop walking and execute the selected action group.
     */
    if (distance <= STOP_DISTANCE_MM) {

        // Stop before performing the combat action
        stopWalking();

        // Execute the action only once per attack cycle
        if (!attacked) {

            // Mark the action as triggered
            attacked = true;

            Serial.println("COMBAT MODE ACTIVATED!");

            /*
             * Execute the predefined combat action group.
             *
             * COMBAT_ACTION = 7 -> Combat 1
             * COMBAT_ACTION = 8 -> Combat 2
             *
             * The action group contains predefined
             * servo movements for the robot's legs.
             */
            minihexa.action_group_run(COMBAT_ACTION);

            Serial.println("COMBAT COMPLETE");

            /*
             * The action group ends in its final pose.
             * A subsequent move() command resumes
             * control through the walking gait.
             */
        }

        return;
    }

    /*
     * CASE 2: Target detected but not yet close enough.
     *
     * If the robot has not attacked, approach the target.
     *
     * If it has already attacked, remain stationary
     * until the target moves beyond RESET_DISTANCE_MM.
     */
    if (!attacked) {

        startWalking();

    } else {

        stopWalking();
    }

    // Short delay before the next control-loop iteration
    delay(100);
}
