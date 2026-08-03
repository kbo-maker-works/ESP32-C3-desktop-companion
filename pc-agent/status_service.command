#!/bin/bash
# Check status and view live logs for ESP32-C3 Companion Agent

echo "============================================================"
echo " 📊 ESP32-C3 Companion Agent Status & Logs"
echo "============================================================"

launchctl list | grep companion || echo "Service is NOT currently loaded."

echo ""
echo "--- Live Logs (~/Library/Logs/companion-agent.log) ---"
echo "Press Ctrl+C to stop viewing logs."
echo ""

tail -f ~/Library/Logs/companion-agent.log
