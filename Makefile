# ================================================================================
# GNMiner Phase 2 - FX Rule Discovery Makefile
# ================================================================================
#
# This Makefile builds and runs rule discovery analysis for forex pairs.
#
# Usage:
#   make            # Build the executable
#   make run        # Run all 20 forex pairs (DEFAULT)
#   make test       # Test with USDJPY
#   make clean      # Remove build artifacts
#
# ================================================================================

CC = gcc
CFLAGS = -Wall -g -O2
LDFLAGS = -lm
TARGET = main
SOURCE = main.c

# FX Currency Pairs (20 major pairs)
FX_PAIRS = USDJPY EURJPY GBPJPY AUDJPY NZDJPY CADJPY CHFJPY \
           EURUSD GBPUSD AUDUSD NZDUSD USDCAD USDCHF \
           EURGBP EURAUD EURCHF GBPAUD GBPCAD AUDCAD AUDNZD

# ================================================================================
# Build Targets
# ================================================================================

# Default: build executable
all: $(TARGET)

# Build main executable
$(TARGET): $(SOURCE)
	@echo "=========================================="
	@echo "  Building GNMiner FX Edition"
	@echo "  Very Tight Cluster Settings:"
	@echo "    Maxsigx = 0.25%"
	@echo "    Minmean = 0.5%"
	@echo "    MIN_SUPPORT_COUNT = 8"
	@echo "=========================================="
	$(CC) $(CFLAGS) -o $(TARGET) $(SOURCE) $(LDFLAGS)
	@echo "✓ Build complete: $(TARGET)"
	@echo ""

# ================================================================================
# Execution Targets
# ================================================================================

# Run all forex pairs (default: 20 pairs)
run: $(TARGET)
	@echo "=========================================="
	@echo "  Running FX Batch Analysis"
	@echo "  Total: 20 currency pairs"
	@echo "  Setting: Very Tight Cluster"
	@echo "=========================================="
	@echo ""
	@total=0; success=0; failed=0; \
	for pair in $(FX_PAIRS); do \
		total=$$((total + 1)); \
		echo "----------------------------------------"; \
		echo "[$$total/20] Processing: $$pair"; \
		echo "----------------------------------------"; \
		if ./$(TARGET) $$pair 1; then \
			success=$$((success + 1)); \
			echo "✓ SUCCESS: $$pair"; \
		else \
			failed=$$((failed + 1)); \
			echo "✗ FAILED: $$pair"; \
		fi; \
		echo ""; \
	done; \
	echo "=========================================="; \
	echo "  FX Batch Processing Complete"; \
	echo "=========================================="; \
	echo "Total runs:    $$total"; \
	echo "Success:       $$success"; \
	echo "Failed:        $$failed"; \
	if [ $$total -gt 0 ]; then \
		echo "Success rate:  $$((success * 100 / total))%"; \
	fi; \
	echo "=========================================="; \
	echo ""

# Test with USD/JPY
test: $(TARGET)
	@echo "=========================================="
	@echo "  Test Run: USD/JPY"
	@echo "  Very Tight Cluster Settings"
	@echo "=========================================="
	@echo ""
	./$(TARGET) USDJPY 1
	@echo ""
	@echo "✓ Test complete"
	@echo ""
	@echo "Check results:"
	@echo "  cat 1-deta-enginnering/forex_data_daily/output/USDJPY/pool/zrp01a.txt"

# Run specific forex pair
# Usage: make run-pair PAIR=USDJPY
run-pair: $(TARGET)
	@if [ -z "$(PAIR)" ]; then \
		echo "ERROR: PAIR must be specified"; \
		echo "Usage: make run-pair PAIR=USDJPY"; \
		exit 1; \
	fi
	./$(TARGET) $(PAIR) 1

# Run major pairs only (USD/JPY, EUR/USD, GBP/USD)
run-major: $(TARGET)
	@echo "=========================================="
	@echo "  Running Major FX Pairs"
	@echo "=========================================="
	@echo ""
	@for pair in USDJPY EURUSD GBPUSD; do \
		echo "Processing: $$pair"; \
		./$(TARGET) $$pair 1; \
		echo ""; \
	done
	@echo "✓ Major pairs complete"

# ================================================================================
# Utility Targets
# ================================================================================

# Clean build artifacts and log files
clean:
	@echo "Cleaning build artifacts..."
	rm -f $(TARGET)
	rm -f *.log
	rm -f *.png
	@echo "✓ Clean complete"

# Deep clean (remove all output)
clean-all: clean
	@echo "Removing all FX output files..."
	rm -rf 1-deta-enginnering/forex_data_daily/output/
	@echo "✓ Deep clean complete"

# Show help
help:
	@echo "=========================================="
	@echo "  GNMiner FX Edition Makefile"
	@echo "=========================================="
	@echo ""
	@echo "Targets:"
	@echo "  make            - Build the executable"
	@echo "  make run        - Run all 20 forex pairs (DEFAULT)"
	@echo "  make test       - Test with USD/JPY"
	@echo "  make run-pair   - Run specific pair"
	@echo "  make run-major  - Run major pairs only"
	@echo "  make clean      - Remove build artifacts"
	@echo "  make clean-all  - Remove all output files"
	@echo "  make help       - Show this help message"
	@echo ""
	@echo "Examples:"
	@echo "  make                           # Build"
	@echo "  make run                       # All 20 pairs"
	@echo "  make test                      # USD/JPY only"
	@echo "  make run-pair PAIR=EURUSD      # EUR/USD"
	@echo "  make run-major                 # Major 3 pairs"
	@echo ""
	@echo "Available Pairs (20):"
	@echo "  JPY Cross: USDJPY EURJPY GBPJPY AUDJPY NZDJPY CADJPY CHFJPY"
	@echo "  USD Cross: EURUSD GBPUSD AUDUSD NZDUSD USDCAD USDCHF"
	@echo "  Others:    EURGBP EURAUD EURCHF GBPAUD GBPCAD AUDCAD AUDNZD"
	@echo ""
	@echo "Very Tight Cluster Settings:"
	@echo "  Maxsigx: 0.25%  (very low variance)"
	@echo "  Minmean: 0.5%   (strong directionality)"
	@echo "  MIN_CONCENTRATION: 0.6 (60% quadrant concentration)"
	@echo "  MIN_SUPPORT_COUNT: 8 (minimum 8 observations)"
	@echo ""

.PHONY: all run test run-pair run-major clean clean-all help
