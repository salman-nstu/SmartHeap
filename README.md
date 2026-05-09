# 🧠 SmartHeap

**Thread-Safe Custom Memory Allocator for Windows**

A production-quality custom memory allocator in C implementing `malloc()`, `free()`, `calloc()`, and `realloc()` with advanced features including multiple allocation strategies, block coalescing, a terminal heap visualizer, fragmentation analyzer, and leak detector.

Built using native Windows APIs (`VirtualAlloc`, `CRITICAL_SECTION`, `CreateThread`) — no POSIX or third-party dependencies.

---

## ✨ Features

| Feature | Description |
|---------|------------|
| **Custom `malloc/free/calloc/realloc`** | Full dynamic memory management API |
| **Multiple Strategies** | First Fit, Best Fit, Worst Fit — switchable at runtime |
| **Block Splitting** | Splits large blocks to reduce internal fragmentation |
| **Block Coalescing** | Merges adjacent free blocks to reduce external fragmentation |
| **Arena-Based Allocation** | Requests large OS chunks via `VirtualAlloc`, sub-allocates internally |
| **Thread Safety** | Windows `CRITICAL_SECTION` for concurrent access protection |
| **Terminal Heap Visualizer** | ANSI-colored memory map with Unicode box-drawing |
| **Fragmentation Analysis** | External/internal fragmentation metrics |
| **Memory Leak Detector** | Reports unfreed blocks with source file/line |
| **Corruption Detection** | Magic numbers detect heap corruption and double-frees |
| **Interactive CLI** | Command-line interface for real-time heap exploration |

---

## 🏗️ Architecture

```
┌─────────────────────────────────────────────────────┐
│                    Application                       │
│              (test programs / CLI)                    │
├─────────────────────────────────────────────────────┤
│              smartheap.h (Public API)                 │
│     sh_malloc / sh_free / sh_calloc / sh_realloc    │
├─────────────────────────────────────────────────────┤
│              Block Manager (block.c)                 │
│     Linked list metadata, splitting, coalescing      │
├──────────┬──────────┬───────────────────────────────┤
│ First Fit│ Best Fit │ Worst Fit  (strategy.c)       │
├──────────┴──────────┴───────────────────────────────┤
│              Arena Manager (arena.c)                 │
│        Large chunk allocation from OS                │
├─────────────────────────────────────────────────────┤
│         Windows Platform (platform_win32.c)          │
│               VirtualAlloc / VirtualFree             │
├─────────────────────────────────────────────────────┤
│              Windows OS (kernel32.dll)                │
└─────────────────────────────────────────────────────┘
```

---

## 📁 Project Structure

```
SmartHeap/
├── include/                    # Header files
│   ├── smartheap.h             # Public API
│   ├── smartheap_internal.h    # Internal types & macros
│   ├── platform.h              # OS abstraction interface
│   ├── arena.h                 # Arena management
│   ├── block.h                 # Block operations
│   ├── strategy.h              # Allocation strategies
│   ├── visualizer.h            # Heap visualization
│   ├── stats.h                 # Statistics & fragmentation
│   └── leak_detector.h         # Leak detection
├── src/                        # Implementation
│   ├── smartheap.c             # Core allocator (CRITICAL_SECTION)
│   ├── platform_win32.c        # VirtualAlloc/VirtualFree backend
│   ├── arena.c                 # Arena manager
│   ├── block.c                 # Block list operations
│   ├── strategy.c              # Fit strategies
│   ├── visualizer.c            # Terminal visualizer
│   ├── stats.c                 # Statistics engine
│   └── leak_detector.c         # Leak tracker
├── tests/
│   └── test_runner.c           # Test suite (25 tests)
├── demo/
│   ├── demo_basic.c            # Step-by-step feature demo
│   ├── demo_interactive.c      # Interactive CLI
│   └── demo_benchmark.c        # Performance benchmarks
├── Makefile                    # Build system
└── README.md                   # This file
```

---

## 🔧 Prerequisites

You need **GCC** and **GNU Make** installed on Windows. The easiest way is via **MSYS2**:

1. Download and install [MSYS2](https://www.msys2.org/)
2. Open the **MSYS2 UCRT64** terminal and run:
   ```bash
   pacman -S mingw-w64-ucrt-x86_64-gcc make
   ```
3. Add MSYS2 to your **System PATH** (one-time setup):
   - Add `C:\msys64\ucrt64\bin` and `C:\msys64\usr\bin` to your system PATH
   - Or run this in PowerShell before building:
     ```powershell
     $env:Path += ";C:\msys64\ucrt64\bin;C:\msys64\usr\bin"
     ```

---

## 🚀 How to Build & Run

Open **PowerShell** or **Windows Terminal** and navigate to the project folder:

```powershell
cd "d:\Custom Memory Allocator in C"
```

### Build Everything
```powershell
make all
```

This compiles 4 executables into the `build/` folder:
- `build/test_runner.exe` — Test suite
- `build/demo_basic.exe` — Basic demo
- `build/demo_interactive.exe` — Interactive CLI
- `build/demo_benchmark.exe` — Benchmarks

### ✅ Run the Test Suite (25 tests)
```powershell
.\build\test_runner.exe
```
You should see all **25 tests PASS** — covering malloc, free, calloc, realloc, strategies, stress testing, leak detection, and multi-threaded safety.

### 🎬 Run the Basic Demo
```powershell
.\build\demo_basic.exe
```
This walks through every feature step-by-step:
1. Allocating memory with `sh_malloc()`
2. Freeing memory (creates a hole in the heap)
3. Block reuse (freed space gets recycled)
4. Zero-initialized allocation with `sh_calloc()`
5. Resizing with `sh_realloc()`
6. Printing detailed statistics
7. Switching allocation strategy at runtime
8. Memory leak detection with source file:line tracking

### 🎮 Run the Interactive CLI
```powershell
.\build\demo_interactive.exe
```
This gives you a live command prompt to interact with the allocator:

```
SmartHeap> allocate 64         # Allocate 64 bytes → Block #1
SmartHeap> allocate 128        # Allocate 128 bytes → Block #2
SmartHeap> allocate 256        # Allocate 256 bytes → Block #3
SmartHeap> free 2              # Free block #2 (creates a hole)
SmartHeap> visualize           # See the heap memory map
SmartHeap> stats               # See detailed statistics
SmartHeap> strategy best_fit   # Switch to Best Fit strategy
SmartHeap> allocate 100        # New alloc uses Best Fit
SmartHeap> list                # List all active blocks
SmartHeap> leak                # Check for memory leaks
SmartHeap> quit                # Exit (runs final leak check)
```

**Available commands:**
| Command | Description |
|---------|-------------|
| `allocate <size>` | Allocate bytes |
| `free <id>` | Free a block by its ID |
| `calloc <n> <size>` | Allocate n × size zeroed bytes |
| `realloc <id> <size>` | Resize a block |
| `visualize` | Show the heap memory map |
| `stats` | Show statistics & fragmentation |
| `strategy <name>` | Switch strategy: `first_fit`, `best_fit`, `worst_fit` |
| `list` | List all active blocks |
| `leak` | Run leak detection |
| `quit` | Exit the program |

### 📊 Run the Benchmarks
```powershell
.\build\demo_benchmark.exe
```
Compares SmartHeap (First Fit, Best Fit, Worst Fit) against the standard C library `malloc` across 50,000 operations.

### 🧹 Clean Build Artifacts
```powershell
make clean
```

---

## 📖 API Reference

### Initialization
```c
#include "smartheap.h"

sh_init();              // Initialize allocator
sh_destroy();           // Release all memory to OS
```

### Core Allocation
```c
void *ptr = sh_malloc(64);          // Allocate 64 bytes
void *arr = sh_calloc(10, 4);      // Allocate 40 zeroed bytes
ptr = sh_realloc(ptr, 128);        // Resize to 128 bytes
sh_free(ptr);                       // Free memory
```

### Strategy Selection
```c
sh_set_strategy(SH_STRATEGY_FIRST_FIT);   // Fast
sh_set_strategy(SH_STRATEGY_BEST_FIT);    // Less waste
sh_set_strategy(SH_STRATEGY_WORST_FIT);   // Large remainders
```

### Visualization & Analysis
```c
sh_visualize();         // Print heap memory map
sh_print_stats();       // Print detailed statistics
sh_leak_check();        // Check for memory leaks
```

### Leak Tracking (with source locations)
```c
#define SH_TRACK_LEAKS
#include "smartheap.h"
// sh_malloc() now records __FILE__ and __LINE__
```

---

## 🧪 Test Suite

25 comprehensive tests covering:

| Suite | Tests | Coverage |
|-------|-------|----------|
| malloc | 5 | Basic, zero-size, multiple, large (2MB), alignment |
| free | 3 | NULL safety, block reuse, coalescing |
| calloc | 3 | Zero-init, zero params, overflow |
| realloc | 4 | Grow, shrink, NULL ptr, zero size |
| strategies | 3 | First Fit, Best Fit, Worst Fit |
| stress | 2 | 10K random ops, 1K reallocs |
| leak detection | 2 | No leaks, intentional leak detection |
| visualization | 2 | Memory map, statistics |
| thread safety | 1 | 4 concurrent threads (CreateThread), 10K ops |

---

## 📚 Systems Programming Concepts

- **Virtual Memory**: Direct OS memory management with `VirtualAlloc`/`VirtualFree`
- **Data Structures**: Doubly-linked block lists with O(1) coalescing
- **Pointer Arithmetic**: Block headers, alignment, memory layout
- **Concurrency**: `CRITICAL_SECTION` for thread-safe access, `CreateThread` for testing
- **Corruption Detection**: Magic numbers (`0xDEADBEEF` / `0xFEEDFACE`)
- **Software Engineering**: Modular design with clean separation of concerns

---

## 📄 License

MIT License — Educational project for systems programming.
