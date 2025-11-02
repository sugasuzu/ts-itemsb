# ================================================================================
# GNMiner Phase 2 - Rule Discovery Makefile
# ================================================================================
#
# This Makefile builds and runs rule discovery analysis for cryptocurrencies.
#
# Usage:
#   make            # Build the executable
#   make run        # Run all 20 cryptocurrencies (DEFAULT)
#   make run-stocks # Run all 225 Nikkei stocks
#   make test       # Test with BTC
#   make clean      # Remove build artifacts
#
# ================================================================================

CC = gcc
CFLAGS = -Wall -g -O2
LDFLAGS = -lm
TARGET = main
SOURCE = main.c

# Nikkei 225 stock codes (225 stocks)
STOCK_CODES = 1332 1333 1605 \
              1721 1801 1802 1803 1925 1928 1963 \
              2002 2269 2282 2501 2502 2503 2531 2801 2802 2871 2914 \
              3086 3401 3402 3407 \
              3861 3863 \
              4005 4021 4042 4043 4061 4063 4151 4183 4188 4202 4204 4208 4272 4324 4452 4502 4503 4506 4507 4519 4523 4543 4568 4578 4612 4631 4661 4681 4704 4751 4901 4911 \
              5001 5002 5019 5020 5101 5108 5201 5202 5214 5232 5233 5301 5332 5333 5401 5406 5411 5541 5631 5703 5706 5707 5711 5713 5714 5801 5802 5803 \
              6103 6113 6178 6201 6268 6301 6302 6305 6326 6361 6367 6471 6472 6473 6479 6501 6502 6503 6504 6506 6586 6594 6645 6701 6702 6723 6724 6752 6753 6758 6762 6841 6857 6861 6902 6920 6923 6952 6954 6963 6971 6976 7003 7004 7011 7012 7013 \
              7201 7202 7203 7205 7267 7269 7270 7272 \
              7731 7733 7735 7741 7751 7752 \
              7832 7911 7912 7951 \
              9501 9502 9503 9531 9532 \
              9001 9005 9007 9008 9020 9021 9022 \
              9101 9104 9107 \
              9201 9202 \
              9301 9303 9613 \
              4307 4324 4751 9432 9433 9437 9613 9697 9735 9766 9983 9984 \
              8001 8002 8015 8031 8053 8058 8233 8252 8267 8306 8316 8331 8411 \
              3086 3099 3382 7453 7606 7608 7611 8233 8252 8267 9983 \
              8301 8303 8304 8306 8308 8309 8316 8331 8354 8355 8411 8473 \
              8473 8601 8604 8628 8630 8697 8750 8766 8802 \
              8725 8750 8766 8795 \
              8802 \
              8801 8802 8830 \
              2413 4307 4324 4661 4751 6178 9432 9433 9437 9613 9697 9735 9766

# Cryptocurrency pairs (20 pairs) - Updated names without -USD suffix
CRYPTO_PAIRS = AAVE ADA ALGO ATOM AVAX \
               BNB BTC DOGE DOT ETC \
               ETH FIL LINK LTC MATIC \
               SOL UNI VET XLM XRP

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

# Run all cryptocurrencies (default: 20 cryptocurrencies)
run: $(TARGET)
	@echo "=========================================="
	@echo "  Running Crypto Batch Analysis"
	@echo "  Total: 20 cryptocurrencies"
	@echo "=========================================="
	@echo ""
	@total=0; success=0; failed=0; \
	for pair in $(CRYPTO_PAIRS); do \
		total=$$((total + 1)); \
		echo "----------------------------------------"; \
		echo "[$$total/20] Processing: $$pair"; \
		echo "----------------------------------------"; \
		if ./$(TARGET) $$pair 10; then \
			success=$$((success + 1)); \
			echo "✓ SUCCESS: $$pair"; \
		else \
			failed=$$((failed + 1)); \
			echo "✗ FAILED: $$pair"; \
		fi; \
		echo ""; \
	done; \
	echo "=========================================="; \
	echo "  Crypto Batch Processing Complete"; \
	echo "=========================================="; \
	echo "Total runs:    $$total"; \
	echo "Success:       $$success"; \
	echo "Failed:        $$failed"; \
	if [ $$total -gt 0 ]; then \
		echo "Success rate:  $$((success * 100 / total))%"; \
	fi; \
	echo "=========================================="; \
	echo ""

# Test with cryptocurrency (BTC)
test: $(TARGET)
	@echo "=========================================="
	@echo "  Test Run: Bitcoin (BTC)"
	@echo "=========================================="
	@echo ""
	./$(TARGET) BTC 10
	@echo ""
	@echo "✓ Test complete"

# Run all stocks (225 stocks)
run-stocks: $(TARGET)
	@echo "=========================================="
	@echo "  Running Stock Batch Analysis"
	@echo "  Total: 225 stocks"
	@echo "=========================================="
	@echo ""
	@total=0; success=0; failed=0; \
	for code in $(STOCK_CODES); do \
		total=$$((total + 1)); \
		echo "----------------------------------------"; \
		echo "[$$total/225] Processing: $$code"; \
		echo "----------------------------------------"; \
		if ./$(TARGET) $$code; then \
			success=$$((success + 1)); \
			echo "✓ SUCCESS: $$code"; \
		else \
			failed=$$((failed + 1)); \
			echo "✗ FAILED: $$code"; \
		fi; \
		echo ""; \
	done; \
	echo "=========================================="; \
	echo "  Stock Batch Processing Complete"; \
	echo "=========================================="; \
	echo "Total runs:    $$total"; \
	echo "Success:       $$success"; \
	echo "Failed:        $$failed"; \
	if [ $$total -gt 0 ]; then \
		echo "Success rate:  $$((success * 100 / total))%"; \
	fi; \
	echo "=========================================="; \
	echo ""

# Run specific stock
# Usage: make run-stock CODE=7203
run-stock: $(TARGET)
	@if [ -z "$(CODE)" ]; then \
		echo "ERROR: CODE must be specified"; \
		echo "Usage: make run-stock CODE=7203"; \
		exit 1; \
	fi
	./$(TARGET) $(CODE)

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
	rm -rf crypto_data/output/
	@echo "✓ Deep clean complete"

# Show help
help:
	@echo "=========================================="
	@echo "  GNMiner Phase 2 Makefile"
	@echo "=========================================="
	@echo ""
	@echo "Targets:"
	@echo "  make            - Build the executable"
	@echo "  make run        - Run all 20 cryptocurrencies (DEFAULT)"
	@echo "  make run-stocks - Run all 225 Nikkei stocks"
	@echo "  make test       - Test with BTC"
	@echo "  make run-stock  - Run specific stock/crypto"
	@echo "  make clean      - Remove build artifacts"
	@echo "  make clean-all  - Remove all output files"
	@echo "  make help       - Show this help message"
	@echo ""
	@echo "Examples:"
	@echo "  make                           # Build"
	@echo "  make run                       # All 20 cryptocurrencies"
	@echo "  make test                      # Bitcoin (BTC) only"
	@echo "  make run-stock CODE=BTC        # Bitcoin"
	@echo "  make run-stock CODE=ETH        # Ethereum"
	@echo "  make run-stocks                # All 225 stocks"
	@echo "  make run-stock CODE=7203       # Toyota (stock)"
	@echo ""
	@echo "Cryptocurrencies (20):"
	@echo "  $(CRYPTO_PAIRS)"
	@echo ""

.PHONY: all run run-stocks test run-stock clean clean-all help
