# ================================================================================
# GNMiner Phase 2 - Directional Extreme Rule Discovery Makefile
# ================================================================================
#
# This Makefile builds and runs directional extreme value analysis for forex
# trading rule discovery. Each currency pair is analyzed separately for
# positive extremes (BUY signals) and negative extremes (SELL signals).
#
# Usage:
#   make          # Build the executable
#   make run      # Run all 20 pairs × 2 directions (40 analyses)
#   make test     # Test with USDJPY positive and negative
#   make clean    # Remove build artifacts
#
# ================================================================================

CC = gcc
CFLAGS = -Wall -g -O2
LDFLAGS = -lm
TARGET = main
SOURCE = main.c

# All forex pairs (20 pairs)
FOREX_PAIRS = USDJPY EURJPY GBPJPY AUDJPY NZDJPY CADJPY CHFJPY \
              EURUSD GBPUSD AUDUSD NZDUSD USDCAD USDCHF \
              EURGBP EURAUD EURCHF GBPAUD GBPCAD AUDCAD AUDNZD

# Directions
DIRECTIONS = positive negative

# ================================================================================
# Build Targets
# ================================================================================

# Default: build executable
all: $(TARGET)

# Build main executable
$(TARGET): $(SOURCE)
	@echo "=========================================="
	@echo "  Building GNMiner Phase 2"
	@echo "=========================================="
	$(CC) $(CFLAGS) -o $(TARGET) $(SOURCE) $(LDFLAGS)
	@echo "✓ Build complete: $(TARGET)"
	@echo ""

# ================================================================================
# Execution Targets
# ================================================================================

# Run all pairs and directions (20 pairs × 2 directions = 40 analyses)
run: $(TARGET)
	@echo "=========================================="
	@echo "  Running Full Batch Analysis"
	@echo "  Total: 20 pairs × 2 directions = 40 runs"
	@echo "=========================================="
	@echo ""
	@total=0; success=0; failed=0; \
	for pair in $(FOREX_PAIRS); do \
		for direction in $(DIRECTIONS); do \
			total=$$((total + 1)); \
			echo "----------------------------------------"; \
			echo "[$$total/40] Processing: $$pair ($$direction)"; \
			echo "----------------------------------------"; \
			if ./$(TARGET) $$pair $$direction; then \
				success=$$((success + 1)); \
				echo "✓ SUCCESS: $$pair $$direction"; \
			else \
				failed=$$((failed + 1)); \
				echo "✗ FAILED: $$pair $$direction"; \
			fi; \
			echo ""; \
		done; \
	done; \
	echo "=========================================="; \
	echo "  Batch Processing Complete"; \
	echo "=========================================="; \
	echo "Total runs:    $$total"; \
	echo "Success:       $$success"; \
	echo "Failed:        $$failed"; \
	echo "Success rate:  $$((success * 100 / total))%"; \
	echo "=========================================="; \
	echo ""

# Test with single pair (both directions)
test: $(TARGET)
	@echo "=========================================="
	@echo "  Test Run: USDJPY (both directions)"
	@echo "=========================================="
	@echo ""
	@echo "--- Testing USDJPY positive ---"
	./$(TARGET) USDJPY positive
	@echo ""
	@echo "--- Testing USDJPY negative ---"
	./$(TARGET) USDJPY negative
	@echo ""
	@echo "✓ Test complete"

# Run specific pair and direction
# Usage: make run-pair PAIR=USDJPY DIR=positive
run-pair: $(TARGET)
	@if [ -z "$(PAIR)" ] || [ -z "$(DIR)" ]; then \
		echo "ERROR: PAIR and DIR must be specified"; \
		echo "Usage: make run-pair PAIR=USDJPY DIR=positive"; \
		exit 1; \
	fi
	./$(TARGET) $(PAIR) $(DIR)

# ================================================================================
# Utility Targets
# ================================================================================

# Clean build artifacts and output files
clean:
	@echo "Cleaning build artifacts..."
	rm -f $(TARGET)
	rm -f *.log
	@echo "✓ Clean complete"

# Deep clean (remove all output)
clean-all: clean
	@echo "Removing all output files..."
	rm -rf output/
	@echo "✓ Deep clean complete"

# Show help
help:
	@echo "=========================================="
	@echo "  GNMiner Phase 2 Makefile"
	@echo "=========================================="
	@echo ""
	@echo "Targets:"
	@echo "  make          - Build the executable"
	@echo "  make run      - Run all 20 pairs × 2 directions (40 analyses)"
	@echo "  make test     - Test with USDJPY (both directions)"
	@echo "  make run-pair - Run specific pair/direction"
	@echo "  make clean    - Remove build artifacts"
	@echo "  make clean-all - Remove all output files"
	@echo "  make help     - Show this help message"
	@echo ""
	@echo "Examples:"
	@echo "  make run-pair PAIR=EURJPY DIR=positive"
	@echo "  make run-pair PAIR=GBPUSD DIR=negative"
	@echo ""

.PHONY: all run test run-pair clean clean-all help