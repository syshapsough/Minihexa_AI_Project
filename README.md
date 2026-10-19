# MiniHexa AI Project — Getting Started

A collection of reference projects for experimenting with **camera-based perception, AI inference, and movement control** on the Hiwonder MiniHexa hexapod robot.

This repository is intended to help COE476 teams understand the existing implementations and build their own autonomous applications. **The formal project requirements and assessment details are in the separate COE476 project brief**, not in this README.

## What's in this repository?

| Folder | Purpose | What to explore |
|---|---|---|
| `MiniHexa_Camera_AI/` | Camera and embedded AI reference project | Camera setup, frame acquisition, model inference, preprocessing, and integration points |
| `MiniHexa_Control/` | MiniHexa movement/control reference project | Movement commands, action groups, and ways to trigger behaviors |

> **Note:** These descriptions are a starting guide based on the project folder names and previous demonstrations. Consult each project's source files to confirm its entry point, hardware configuration, build procedure, and supported features. Some dependencies may be downloaded automatically rather than stored in Git.

## How the pieces fit together

![MiniHexa system architecture](diagrams/system_architecture.png)

The intended workflow is straightforward: capture an image, run perception, decide what the robot should do, and invoke an appropriate movement. Optional MQTT messages can report detections or actions to another system.

The existing **punching-bag tracking demonstration** illustrates this approach: the camera identifies a target and the robot responds using its movement/action-group logic. Use it to understand the integration pattern, not as a finished solution for a new application.

## 1. Explore the reference projects

**Start with `MiniHexa_Control/`.** Identify how the robot initializes, which functions or action groups produce movement, and how to stop or return to a safe pose. Test movements individually in a clear area before linking them to camera predictions.

**Then inspect `MiniHexa_Camera_AI/`.** Find where camera frames enter the application, how images are resized/normalized, where the model is loaded, and how predictions are interpreted. Look for model files, class labels, confidence thresholds, and the code that triggers an action.

Keep notes on the exact files/functions you plan to reuse. You do not need to copy the entire reference project into a new application.

## 2. Set up your development environment

1. Clone or download this repository.
2. Inspect the README, build configuration, and dependencies inside the reference project you intend to run.
3. Install the toolchain appropriate to that project. For ESP-IDF-based firmware, use a compatible ESP-IDF version and the project-specific configuration. For Raspberry Pi/Linux-based code, use its Python/runtime dependencies.
4. Connect the required MiniHexa controller, camera, and power supply according to the hardware setup for that project.
5. Build or run the reference project **without modifying its model or behavior first**.
6. Verify camera input and basic movement separately before combining them.

**Do not assume both folders use the same processor, toolchain, or inference runtime.** Check their actual source and configuration first.

## 3. Train and deploy your own vision model

![Model development pipeline](diagrams/model_pipeline.png)

Use the existing camera and inference code as a reference, but create a model appropriate to your own task.

1. **Define the task:** choose classification or detection/localization based on what the robot must do.
2. **Collect data:** capture varied images under realistic backgrounds, angles, distances, and lighting.
3. **Label and split:** create training, validation, and test sets; avoid placing near-identical video frames in different splits.
4. **Train and evaluate:** record class definitions, model architecture, metrics, and representative failure cases.
5. **Export:** convert the trained model to `.tflite` when appropriate, and validate the exported model on test inputs.
6. **Integrate:** match the model's expected input dimensions, normalization, output ordering, and confidence thresholds in your inference code.
7. **Test on hardware:** measure prediction quality and inference latency on the actual device.

**TFLite vs. TFLite Micro:** Standard TFLite is generally used on Linux-capable boards; TFLite Micro is designed for supported microcontrollers. Microcontroller deployment may require INT8 quantization, compatible operators, and exporting the model as a C array. Use the format that matches your actual controller. A classification model does **not** automatically provide target coordinates; plan how your robot will determine where to turn or move.

## 4. Connect predictions to movement

![Example robot behavior state machine](diagrams/robot_state_machine.png)

A useful application does more than trigger an action from a single frame. Consider a loop that:

- **Searches** by scanning or moving through a safe area.
- **Confirms** a target using confidence thresholds or repeated observations.
- **Approaches or repositions** based on the target's estimated direction and distance.
- **Responds** with an appropriate MiniHexa movement or action group.
- **Recovers** when the target disappears, confidence drops, or a movement times out.

The diagram is an **example**, not a required implementation. Choose states and transitions that fit your application, and include a safe stop behavior.

## 5. Optional MQTT reporting

MQTT can be used to publish a detection or action event to a broker for monitoring. For example:

```json
{
  "class": "target",
  "confidence": 0.91,
  "state": "RESPOND",
  "action": "gesture_complete"
}
```

Choose your own topic naming and payload format. Do not hardcode passwords or broker credentials into a public repository.

## 6. Common problems

| Problem | What to check |
|---|---|
| Camera does not initialize | Camera connection, power, pin configuration, and driver settings |
| Model fails to load | File path, runtime compatibility, tensor shapes, supported operators, and memory |
| Poor predictions | Image preprocessing, class-label order, training data, lighting, and confidence thresholds |
| Robot moves incorrectly | Action-group compatibility, servo initialization, movement parameters, and safe starting pose |
| Robot keeps reacting repeatedly | State transitions, cooldowns, and multi-frame confirmation |
| ESP-IDF project has many generated files | Ignore `build/` and `managed_components/` when dependencies can be restored by the build system |

## 7. Working with the repository

You can clone or fork this repository to experiment without changing the original. Keep your own model, configuration, and application code clearly organized, and document any reused functions or action groups.

Suggested structure for your own application:

```text
my_minihexa_application/
├── README.md
├── model/                 # Model, labels, preprocessing notes
├── src/                   # Camera, inference, control integration
├── actions/               # Required movement/action-group files
└── results/               # Tests and evaluation summaries
```

Only commit files necessary to reproduce your application. Avoid committing generated build directories, local virtual environments, credentials, or downloaded dependency trees unless there is a specific reason.

---

**COE476 teams:** Refer to the separate course project brief for the official scope, requirements, deliverables, and evaluation criteria. This README is a guide to the code and a starting point for your own implementation.
