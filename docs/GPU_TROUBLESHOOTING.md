# NextViper GPU Troubleshooting Guide

This guide details common GPU issues, error messages, and their solutions in NextViper.

---

## 1. `GPU unavailable: No compatible GPU or Vulkan compute device found`

### Cause
NextViper could not find a Vulkan physical device with compute queue capabilities (`VK_QUEUE_COMPUTE_BIT`).

### Solutions
1. Check if GPU drivers are installed:
   ```bash
   vulkaninfo --summary
   ```
2. On Linux, ensure `libvulkan1` and Mesa Vulkan drivers (`mesa-vulkan-drivers`) are installed:
   ```bash
   sudo apt install -y libvulkan1 mesa-vulkan-drivers
   ```
3. For NVIDIA GPUs, ensure the proprietary NVIDIA driver (`nvidia-driver-xxx`) is installed and loaded.
4. For headless cloud containers or CI environments without physical GPUs, install Mesa Lavapipe:
   ```bash
   sudo apt install -y libvulkan-dev mesa-vulkan-drivers
   ```
5. In user code, use `device: "auto"` to automatically fallback to CPU when running on machines without GPUs.

---

## 2. `GPU Out of Memory: Failed to allocate X bytes on device`

### Cause
The requested tensor size exceeded the available VRAM on the target GPU.

### Solutions
1. Reduce the batch size in training (`batch_size: 32` or `batch_size: 16`).
2. Free unused GPU tensors by reassigning references or calling `tensor.to("cpu")`.
3. Check available GPU memory:
   ```nextviper
   import tensor
   print("Active device:", tensor.device_name())
   ```

---

## 3. `Matrix multiplication dimension mismatch`

### Cause
Inner dimensions of matrices do not match for matrix multiplication $A_{M \times K} \times B_{K \times N}$.

### Solution
Ensure `a.shape[1] == b.shape[0]`. Use `a.T()` or `a.reshape(...)` to align dimensions before matrix multiplication.
