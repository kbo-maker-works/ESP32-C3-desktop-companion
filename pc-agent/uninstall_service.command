#!/bin/bash
# Uninstall ESP32-C3 Companion Agent macOS Service

PLIST_DEST="$HOME/Library/LaunchAgents/com.user.companion-agent.plist"

echo "============================================================"
echo " 🛑 Stopping & Uninstalling ESP32-C3 Companion Agent"
echo "============================================================"

launchctl unload "$PLIST_DEST" 2>/dev/null || true
rm -f "$PLIST_DEST"

echo ""
echo "✅ Service successfully stopped and uninstalled."
echo ""
echo "Press any key to close this window..."
read -n 1 -s
