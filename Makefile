# ══════════════════════════════════════════════════════════════
#  SmartHeap - Custom Memory Allocator (Windows)
#  Makefile
# ══════════════════════════════════════════════════════════════

CC       = gcc
CFLAGS   = -Wall -Wextra -O2 -Iinclude -finput-charset=UTF-8 -fexec-charset=UTF-8
LDFLAGS  = -lkernel32

# Directories
BUILD_DIR = build
SRC_DIR   = src
TEST_DIR  = tests
DEMO_DIR  = demo
DASH_DIR  = dashboard

# Source files (Windows only)
CORE_SRCS = $(SRC_DIR)/smartheap.c \
            $(SRC_DIR)/arena.c \
            $(SRC_DIR)/block.c \
            $(SRC_DIR)/strategy.c \
            $(SRC_DIR)/visualizer.c \
            $(SRC_DIR)/stats.c \
            $(SRC_DIR)/leak_detector.c \
            $(SRC_DIR)/platform_win32.c

# Targets
TEST_BIN     = $(BUILD_DIR)/test_runner.exe
DEMO_BIN     = $(BUILD_DIR)/demo_basic.exe
INTERACT_BIN = $(BUILD_DIR)/demo_interactive.exe
BENCH_BIN    = $(BUILD_DIR)/demo_benchmark.exe
DASH_BIN     = $(BUILD_DIR)/dashboard.exe

# ── Default target ───────────────────────────────────────────
.PHONY: all test demo interactive benchmark dashboard clean help

all: $(TEST_BIN) $(DEMO_BIN) $(INTERACT_BIN) $(BENCH_BIN) $(DASH_BIN)
	@echo ""
	@echo "  [SmartHeap] Build complete!"
	@echo "  Test:        $(TEST_BIN)"
	@echo "  Demo:        $(DEMO_BIN)"
	@echo "  Interactive: $(INTERACT_BIN)"
	@echo "  Benchmark:   $(BENCH_BIN)"
	@echo "  Dashboard:   $(DASH_BIN)"

# ── Build directory ──────────────────────────────────────────
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

# ── Test runner ──────────────────────────────────────────────
$(TEST_BIN): $(TEST_DIR)/test_runner.c $(CORE_SRCS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# ── Demos ────────────────────────────────────────────────────
$(DEMO_BIN): $(DEMO_DIR)/demo_basic.c $(CORE_SRCS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(INTERACT_BIN): $(DEMO_DIR)/demo_interactive.c $(CORE_SRCS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

$(BENCH_BIN): $(DEMO_DIR)/demo_benchmark.c $(CORE_SRCS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# ── Dashboard server ─────────────────────────────────────────
$(DASH_BIN): $(DASH_DIR)/server.c $(CORE_SRCS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS) -lws2_32

# ── Convenience targets ─────────────────────────────────────
test: $(TEST_BIN)
	@echo ""
	@echo "  [SmartHeap] Running tests..."
	@echo ""
	./$(TEST_BIN)

demo: $(DEMO_BIN)
	./$(DEMO_BIN)

interactive: $(INTERACT_BIN)
	./$(INTERACT_BIN)

benchmark: $(BENCH_BIN)
	./$(BENCH_BIN)

dashboard: $(DASH_BIN)
	@echo ""
	@echo "  [SmartHeap] Starting dashboard server..."
	@echo "  Open http://localhost:8080 in your browser"
	@echo ""
	./$(DASH_BIN)

clean:
	rm -rf $(BUILD_DIR)
	@echo "  [SmartHeap] Clean complete."

help:
	@echo ""
	@echo "  SmartHeap Build Targets:"
	@echo "    make            - Build everything"
	@echo "    make test       - Build and run test suite"
	@echo "    make demo       - Build and run basic demo"
	@echo "    make interactive - Build and run interactive CLI"
	@echo "    make benchmark  - Build and run benchmarks"
	@echo "    make dashboard  - Build and run web dashboard"
	@echo "    make clean      - Remove build artifacts"
	@echo ""
