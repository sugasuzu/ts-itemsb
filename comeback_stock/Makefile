# ========================================
# Makefile for Comeback Stock Analysis
# GNP-based Rule Discovery for Nikkei 225
# ========================================

CC = gcc
CFLAGS = -O2 -Wall
LDFLAGS = -lm
TARGET = main
SRC = main.c

# Top 10 High-Volatility Stocks
# (Based on nikkei225_volatility_ranking.csv)
STOCK_TOP1 = 3436
STOCK_TOP2 = 4755
STOCK_TOP3 = 9984
STOCK_TOP4 = 1808
STOCK_TOP5 = 5541
STOCK_TOP6 = 6976
STOCK_TOP7 = 5707
STOCK_TOP8 = 7003
STOCK_TOP9 = 6857
STOCK_TOP10 = 7202

# ========================================
# Build Rules
# ========================================

all: $(TARGET)

$(TARGET): $(SRC)
	@echo "=========================================="
	@echo "  Compiling GNMiner for Stock Analysis"
	@echo "=========================================="
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC) $(LDFLAGS)
	@echo "✓ Compilation successful: $(TARGET)"
	@echo ""

# ========================================
# Test and Run Rules
# ========================================

# Test with top volatility stock (3436)
test: $(TARGET)
	@echo "=========================================="
	@echo "  Test Run: Stock $(STOCK_TOP1)"
	@echo "  (Highest volatility: σ=6.62%)"
	@echo "=========================================="
	@echo ""
	./$(TARGET) $(STOCK_TOP1)
	@echo ""
	@echo "✓ Test complete"

# Run on specific stock
run: $(TARGET)
	@echo "Usage: make run STOCK=<code>"
	@echo "Example: make run STOCK=3436"

run-stock: $(TARGET)
	@if [ -z "$(STOCK)" ]; then \
		echo "ERROR: STOCK parameter required"; \
		echo "Usage: make run-stock STOCK=3436"; \
		exit 1; \
	fi
	@echo "=========================================="
	@echo "  Running: Stock $(STOCK)"
	@echo "=========================================="
	./$(TARGET) $(STOCK)

# ========================================
# Batch Experiments: Top 10 Stocks
# ========================================

run-top10: $(TARGET)
	@echo "=========================================="
	@echo "  Batch Run: Top 10 High-Volatility Stocks"
	@echo "=========================================="
	@echo ""
	@for stock in $(STOCK_TOP1) $(STOCK_TOP2) $(STOCK_TOP3) $(STOCK_TOP4) $(STOCK_TOP5) \
	              $(STOCK_TOP6) $(STOCK_TOP7) $(STOCK_TOP8) $(STOCK_TOP9) $(STOCK_TOP10); do \
		echo ""; \
		echo "=========================================="; \
		echo "  Processing Stock: $$stock"; \
		echo "=========================================="; \
		./$(TARGET) $$stock; \
		echo "✓ Completed: $$stock"; \
	done
	@echo ""
	@echo "=========================================="
	@echo "  ✓ All 10 stocks completed"
	@echo "=========================================="

# Run top 3 only (for quick testing)
run-top3: $(TARGET)
	@echo "=========================================="
	@echo "  Quick Run: Top 3 Stocks"
	@echo "=========================================="
	@for stock in $(STOCK_TOP1) $(STOCK_TOP2) $(STOCK_TOP3); do \
		echo ""; \
		echo "Processing: $$stock"; \
		./$(TARGET) $$stock; \
	done
	@echo ""
	@echo "✓ Top 3 completed"

# ========================================
# Output Management
# ========================================

# Show results for a specific stock
show-results:
	@if [ -z "$(STOCK)" ]; then \
		echo "ERROR: STOCK parameter required"; \
		echo "Usage: make show-results STOCK=3436"; \
		exit 1; \
	fi
	@if [ -f "output/$(STOCK)/pool/zrp01a.txt" ]; then \
		echo "========================================"; \
		echo "  Results for Stock $(STOCK)"; \
		echo "========================================"; \
		head -20 output/$(STOCK)/pool/zrp01a.txt; \
	else \
		echo "No results found for stock $(STOCK)"; \
		echo "Run: make run-stock STOCK=$(STOCK)"; \
	fi

# List all completed experiments
list-results:
	@echo "=========================================="
	@echo "  Completed Experiments"
	@echo "=========================================="
	@if [ -d "output" ]; then \
		ls -1 output/ | grep -E '^[0-9]+$$' || echo "No results yet"; \
	else \
		echo "No output directory"; \
	fi

# Clean specific stock output
clean-stock:
	@if [ -z "$(STOCK)" ]; then \
		echo "ERROR: STOCK parameter required"; \
		echo "Usage: make clean-stock STOCK=3436"; \
		exit 1; \
	fi
	rm -rf output/$(STOCK)
	@echo "✓ Cleaned output for stock $(STOCK)"

# Clean all outputs
clean-output:
	rm -rf output/
	@echo "✓ Cleaned all outputs"

# ========================================
# Cleanup Rules
# ========================================

clean: clean-output
	rm -f $(TARGET)
	@echo "✓ Cleaned build files"

# Full cleanup (build + all outputs)
distclean: clean
	@echo "✓ Full cleanup complete"

# ========================================
# Help
# ========================================

help:
	@echo "=========================================="
	@echo "  GNMiner - Comeback Stock Analysis"
	@echo "=========================================="
	@echo ""
	@echo "Build:"
	@echo "  make          - Compile the program"
	@echo "  make clean    - Remove build files"
	@echo ""
	@echo "Run Experiments:"
	@echo "  make test           - Test run with top volatility stock (3436)"
	@echo "  make run-stock STOCK=3436  - Run on specific stock"
	@echo "  make run-top3       - Quick run: top 3 stocks"
	@echo "  make run-top10      - Full run: all top 10 stocks"
	@echo ""
	@echo "View Results:"
	@echo "  make list-results            - List completed experiments"
	@echo "  make show-results STOCK=3436 - Show results for specific stock"
	@echo ""
	@echo "Cleanup:"
	@echo "  make clean-stock STOCK=3436  - Clean specific stock output"
	@echo "  make clean-output            - Clean all outputs"
	@echo "  make distclean               - Full cleanup"
	@echo ""
	@echo "Top 10 High-Volatility Stocks:"
	@echo "  1. $(STOCK_TOP1)  (σ=6.62%)"
	@echo "  2. $(STOCK_TOP2)  (σ=3.40%)"
	@echo "  3. $(STOCK_TOP3)  (σ=3.34%)"
	@echo "  4. $(STOCK_TOP4)  (σ=3.29%)"
	@echo "  5. $(STOCK_TOP5)  (σ=3.22%)"
	@echo "  6. $(STOCK_TOP6)  (σ=3.13%)"
	@echo "  7. $(STOCK_TOP7)  (σ=2.96%)"
	@echo "  8. $(STOCK_TOP8)  (σ=2.94%)"
	@echo "  9. $(STOCK_TOP9)  (σ=2.93%)"
	@echo " 10. $(STOCK_TOP10) (σ=2.93%)"
	@echo ""
	@echo "=========================================="

.PHONY: all test run run-stock run-top10 run-top3 show-results list-results \
        clean-stock clean-output clean distclean help
