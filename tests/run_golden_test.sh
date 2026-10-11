#!/bin/bash
set -e

if [ ! -f "$1" ]; then
    echo "Usage: run_golden_test.sh <path_to_binance_capture>"
    exit 1
fi

BIN=$(realpath "$1")
cd "$(dirname "$0")"

echo "Running golden test..."
$BIN --replay fixture_market_data.csv -o output_order_book.csv

echo "Diffing output..."
diff -u fixture_order_book_expected.csv output_order_book.csv
echo "Golden test passed!"
rm output_order_book.csv
