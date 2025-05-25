# Drowsiness Detection using MobileNetV2 on STM32H750

---

## 📌 Project Overview

This project implements a lightweight object detection model to detect whether a driver is drowsy or not. It leverages a reduced MobileNetV2 backbone trained and converted for real-time inference on STM32H750 using STM32Cube.AI. The model takes grayscale input from a camera and displays predictions on an LCD screen.

Target application: embedded, on-device drowsiness monitoring for smart vehicles or helmets.

---

## 🎯 Objectives

- Detect drowsy vs. non-drowsy states in real-time
- Use grayscale camera input (128x128 resolution)
- Deploy deep learning model directly on STM32H750 (no cloud/edge server)
- Display results on 0.96” TFT LCD screen
- Run with minimal power and memory usage

---

## 🧠 AI Model Details

- Backbone: MobileNetV2 (width multiplier α = 0.1)
- Truncated: first 5 blocks used (stride = 8)
- Input: 128×128 grayscale image → repeated to 3 channels
- Output: feature map size (3 × 16 × 16)
  - 0 = background
  - 1 = drowsy
  - 2 = non-drowsy
- Head:
  - Depthwise Conv + Pointwise Conv
  - Interpolated to 16x16 output grid
- Activation: ReLU
- Trained using PyTorch and exported via X-Cube.AI

---

## 🛠 Hardware & Software Requirements

🧩 Hardware:

- WeAct STM32H750 board [link](https://github.com/WeActStudio/MiniSTM32H7xx)
- OV2640 camera 
- 0.96” TFT LCD 

💻 Software:

- STM32CubeIDE
- Python 3.10+
- PyTorch
- OpenCV
- Jupyter Notebook

---

## 📦 Project Structure

```
.
├── model/
│ ├── best_model_grayscale_128x128_24_05_2025.pth
│ └── best_model_grayscale_128x128_24_05_2025.onnx
├── dataset/
│ ├── train/
│ │ └──_annotations.csv
│ ├── valid
│ │ └──_annotations.csv
│ └── test/
│   └──_annotations.csv
├── stm32_project/ # STM32CubeIDE project
│ ├── Core/
│ ├── Drivers/
│ └── edge_AI_drowsiness.ioc
├── notebooks/
│ └── train_model.ipynb
└── README.md
```

---


## 🚀 Getting Started

1. Clone the repository
2. Train the model (optional, model is provided)
3. Convert model to C array using X-Cube-AI
4. Open stm32_project in STM32CubeIDE
5. Flash firmware to STM32H750
6. Connect camera + LCD and run

---


## 📈 Performance

- Accuracy on test set: ~80%
- Model complexity: 24.98M MACC
- Flash usage: ~30 KB (model only)
- RAM usage: ~407 KB
- Real-time FPS on STM32H750: ~15 FPS

---


## 📎 License

This project is licensed under the MIT License.  
Please refer to LICENSE file for details.

Note: The dataset used is derived from synthetic driving scenes in Roblox and is intended for educational/research use only.

---


📷 [Demo Video](https://youtu.be/xnOyMFRJEb4)
