# edge_AI_drowsy_driver
Driver Drowsiness Detection using MobileNetV2 on STM32

📌 Project Overview
This project implements a lightweight object detection model to detect whether a driver is drowsy or not. It leverages a reduced MobileNetV2 backbone trained and converted for real-time inference on STM32H750 using STM32Cube.AI. The model takes grayscale input from a camera and displays predictions on an LCD screen.

🎯 Objectives
Detect drowsy vs. non-drowsy states in real-time

Use OV2640 to capture grayscale input (128x128 resolution)

Deploy deep learning model directly on STM32H750 (no cloud/edge server)

Display results on 0.96” TFT LCD screen

Run with minimal power and memory usage

🧠 AI Model Details
Backbone: MobileNetV2 (width multiplier α = 0.1)

Truncated: first 5 blocks used (stride = 8)

Input: 128×128 grayscale image → repeated to 3 channels

Output: feature map size (3 × 16 × 16)

0 = background

1 = drowsy

2 = non-drowsy

Head:

Depthwise Conv + Pointwise Conv

Interpolated to 16x16 output grid

Activation: ReLU

Trained using PyTorch and exported via X-Cube.AI

🛠 Hardware & Software Requirements
🧩 Hardware:

STM32H750 board (tested on STM32H750VBT6)

OV2640 camera (connected via DCMI)

0.96” TFT LCD (SPI interface)

Optional: External QSPI Flash (for model storage)

💻 Software:

STM32CubeIDE

Python 3.10+

PyTorch

OpenCV (for preprocessing and augmentation)

Jupyter Notebook (optional for training scripts)

📦 Project Structure
.
├── model/ # AI model files
│ ├── mobilenetv2_fomo.pt
│ ├── model.onnx
│ └── ai_model.c/.h (from X-Cube-AI)
├── data/ # Training and validation data
│ ├── train/
│ ├── test/
│ └── _annotations.csv
├── stm32_project/ # STM32CubeIDE project
│ ├── Core/
│ ├── Inc/
│ ├── Src/
│ └── main.c
├── notebooks/ # Training & conversion notebooks
│ ├── train_model.ipynb
│ └── convert_model_xcubeai.ipynb
├── README.md
└── requirements.txt

🚀 Getting Started
Clone the repository

Train the model (optional, model is provided)

Convert model to C array using X-Cube-AI

Open stm32_project in STM32CubeIDE

Flash firmware to STM32H750

Connect camera + LCD and run

📈 Performance
Accuracy on test set: ~75%

Model complexity: 24.98M MACC

Flash usage: ~30 KB (model only)

RAM usage: ~407 KB

Real-time FPS on STM32H750: ~15 FPS

🔧 Re-training & Conversion
Training data format: 128×128 grayscale images

Labels: stored in _annotations.csv (bounding boxes + class)

Conversion path:
→ PyTorch (.pth) → ONNX → X-Cube-AI (.c/.h)

📋 Inference Logic
Image divided into 16×16 grid

For each grid cell:

If overlaps with a bounding box:
→ label = 1 (drowsy) or 2 (non-drowsy)

Else: label = 0

Output: 3x16x16 probability map

📎 License
This project is licensed under the MIT License.
Please refer to LICENSE file for details.

Note: The dataset used is derived from synthetic driving scenes in Roblox and is intended for educational/research use only.

📷 Demo Image

