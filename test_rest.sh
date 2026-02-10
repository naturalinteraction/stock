#!/bin/bash

# Test script for REST server functionality

echo "=== Testing Stock Chart REST API ==="
echo

# Test 1: Get available tickers
echo "1. Getting available tickers..."
curl -s http://localhost:8080/tickers || echo "Failed to connect to REST server"
echo
echo

# Test 2: Get API info
echo "2. Getting API info..."
curl -s http://localhost:8080/ || echo "Failed to connect to REST server"
echo
echo
sleep 5

# Test 3: Set ticker (this requires the application to be running)
echo "3. Setting ticker to VWCE.DE..."
curl -s -X POST -H "Content-Type: application/json" -d '{"ticker":"VWCE.DE"}' http://localhost:8080/set-ticker || echo "Failed to connect to REST server"
echo
echo
sleep 5

# Test 3: Set ticker (this requires the application to be running)
echo "3. Setting ticker to VHYL.AS..."
curl -s -X POST -H "Content-Type: application/json" -d '{"ticker":"VHYL.AS"}' http://localhost:8080/set-ticker || echo "Failed to connect to REST server"
echo
echo
sleep 5

# Test 4: Try to set invalid ticker
echo "4. Trying invalid ticker..."
curl -s -X POST -H "Content-Type: application/json" -d '{"ticker":"INVALID"}' http://localhost:8080/set-ticker || echo "Failed to connect to REST server"
echo
echo

echo "=== Tests completed ==="