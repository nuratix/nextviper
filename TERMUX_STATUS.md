# NextViper Termux & Android Compatibility Investigation

## 1. Issue Analysis: Signal 9 (SIGKILL) on Android / Termux
When building large C++ codebases or executing heavy compiler tasks in Termux on Android 12+ (API Level 31 and above), processes may abruptly terminate with `Signal 9` (SIGKILL).

### Root Causes
1. **Android Phantom Process Killer**:
   Android 12 introduced a feature to terminate background child processes spawned by apps if:
   - The total number of child processes across all apps exceeds 32.
   - A child process uses significant CPU/memory while the parent activity is backgrounded.
2. **Low Memory Killer Daemon (LMKD)**:
   Termux runs inside Android's process sandboxing without swap memory enabled by default. Compiling C++ with `-O3` and `-j4`/`-j8` causes memory spikes above Android's per-process RSS limit, triggering `lmkd` to send `SIGKILL`.

---

## 2. Recommended Workarounds and Configurations for Termux

### A. Disable Phantom Process Killer (via ADB)
For Android 12, 13, and 14 devices with ADB access:
```bash
adb shell "/system/bin/device_config set_sync_disabled_for_tests persistent"
adb shell "/system/bin/device_config put activity_manager max_phantom_processes 2147483647"
adb shell "setprop persist.sys.fflag.override.settings_enable_monitor_phantom_procs false"
```

### B. Conservative Build Concurrency
Build NextViper with reduced parallelism to prevent peak RSS spikes:
```bash
# In Termux: Use 1 or 2 build jobs instead of -j4
make -j2
```

### C. Compiler Memory Optimization Flags
In `Makefile`, compile with `-O2` or `-Os` in Termux environments:
```makefile
CXXFLAGS += -O2 --param ggc-min-expand=20 --param ggc-min-heapsize=32768
```

### D. Runtime Environment Variables
```bash
export MALLOC_ARENA_MAX=1
```
