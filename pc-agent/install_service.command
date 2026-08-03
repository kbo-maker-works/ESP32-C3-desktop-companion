#!/bin/bash
# Install & Launch ESP32-C3 Companion Agent macOS Service

PLIST_DEST="$HOME/Library/LaunchAgents/com.user.companion-agent.plist"

echo "============================================================"
echo " 🚀 Instalando Servicio ESP32-C3 Companion Agent (launchd)"
echo "============================================================"

# Ensure Python requirements are installed
echo "📦 Verificando dependencias de Python..."
python3 -m pip install -q psutil pyserial 2>/dev/null || true

# Write LaunchAgent plist
mkdir -p "$HOME/Library/LaunchAgents"
cat << 'EOF' > "$PLIST_DEST"
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>Label</key>
    <string>com.user.companion-agent</string>
    <key>EnvironmentVariables</key>
    <dict>
        <key>PATH</key>
        <string>/Users/alejandro.cabo/.pyenv/shims:/usr/local/bin:/usr/bin:/bin:/usr/sbin:/sbin</string>
    </dict>
    <key>ProgramArguments</key>
    <array>
        <string>/Users/alejandro.cabo/.pyenv/shims/python3</string>
        <string>/Users/alejandro.cabo/Things/ESP32-C3-desktop-companion/pc-agent/companion_agent.py</string>
    </array>
    <key>RunAtLoad</key>
    <true/>
    <key>KeepAlive</key>
    <true/>
    <key>StandardOutPath</key>
    <string>/Users/alejandro.cabo/Library/Logs/companion-agent.log</string>
    <key>StandardErrorPath</key>
    <string>/Users/alejandro.cabo/Library/Logs/companion-agent.err</string>
</dict>
</plist>
EOF

# Load service into macOS launchd
launchctl unload "$PLIST_DEST" 2>/dev/null || true
launchctl load "$PLIST_DEST" 2>/dev/null || launchctl bootstrap gui/$(id -u) "$PLIST_DEST" 2>/dev/null || true

echo ""
echo "✅ ¡Servicio instalado y activado correctamente!"
echo "📍 Se ejecutará de forma invisible en segundo plano cada vez que inicies sesión."
echo "📋 Logs disponibles en: ~/Library/Logs/companion-agent.log"
echo ""
echo "Presiona cualquier tecla para cerrar esta ventana..."
read -n 1 -s
