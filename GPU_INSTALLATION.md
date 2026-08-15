# NextViper GPU Installation & Setup Guide

This guide describes how to configure your system for GPU-accelerated computing with NextViper.

---

## 1. Requirements

NextViper uses the universal Vulkan Compute standard. Any system with a Vulkan 1.1+ capable driver supports NextViper GPU acceleration out of the box.

Supported hardware:
- **NVIDIA GPUs**: GeForce, RTX, Quadro, Tesla (Driver 450+)
- **AMD GPUs**: Radeon RX 5000/6000/7000, Radeon Pro, Instinct (Mesa RADV or AMDGPU-PRO)
- **Intel GPUs**: Arc Discrete GPUs, Iris Xe, UHD Integrated Graphics (Mesa ANV or Intel Compute Runtime)
- **Apple Silicon**: M1, M2, M3, M4 Macs via MoltenVK
- **Headless / CI**: Mesa Lavapipe software Vulkan compute driver

---

## 2. Linux Setup

### Ubuntu / Debian
```bash
sudo apt update
sudo apt install -y libvulkan-dev vulkan-tools
```

To verify your GPU driver:
```bash
vulkaninfo --summary
```

### Fedora / RHEL
```bash
sudo dnf install -y vulkan-loader-devel vulkan-tools mesa-vulkan-drivers
```

### Arch Linux
```bash
sudo pacman -S vulkan-devel vulkan-tools
```

---

## 3. macOS Setup (Apple Silicon & Intel)

Install the Vulkan SDK or MoltenVK via Homebrew:
```bash
brew install molten-vk
```

NextViper automatically links with MoltenVK's dynamic runtime on macOS.

---

## 4. Windows Setup

1. Install the latest official GPU drivers for your graphics card (NVIDIA GeForce Experience, AMD Adrenalin, or Intel Graphics Command Center).
2. All modern GPU drivers on Windows include the Vulkan runtime (`vulkan-1.dll`) by default.

---

## 5. Verifying Installation in NextViper

Create a script `check_gpu.nv`:

```nextviper
import tensor

print("NextViper GPU Subsystem Status:")
print("------------------------------")
print("GPU Available :", tensor.is_gpu_available())
print("Device Count  :", tensor.device_count())
print("Device Name   :", tensor.device_name())
print("Default Target:", tensor.default_device())
```

Run with NextViper:
```bash
nextviper run check_gpu.nv
```
