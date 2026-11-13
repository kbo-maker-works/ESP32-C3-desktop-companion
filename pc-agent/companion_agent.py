#!/usr/bin/env python3
"""
ESP32-C3 Desktop Companion PC Agent
-----------------------------------
Detects Claude Code usage, token consumption, and system activity on Windows & macOS,
and streams live metrics to the ESP32-C3 over USB CDC Serial connection.

Supported OS:
- Windows (COM1..COM99)
- macOS (/dev/cu.usbmodem* / /dev/tty.usbmodem*)
- Linux (/dev/ttyACM* / /dev/ttyUSB*)
"""
import datetime
import glob
import json
import math
import os
import platform
import subprocess
import sys
import time
import urllib.request

import psutil
import serial
import serial.tools.list_ports

BAUD_RATE = 115200
SEND_INTERVAL_SEC = 1.0
RECONNECT_INTERVAL_SEC = 3.0

# ---------------------------------------------------------------------------
# Cross-Platform Directory Resolver (Windows / macOS / Linux)
# ---------------------------------------------------------------------------
def get_claude_dirs():
    """Returns candidate directories for Claude Code configuration and session logs."""
    dirs = []
    user_home = os.path.expanduser("~")
    
    # 1. Standard ~/.claude/
    dirs.append(os.path.join(user_home, ".claude"))
    dirs.append(os.path.join(user_home, ".config", "claude"))

    # 2. OS Specific Paths
    system = platform.system()
    if system == "Windows":
        appdata = os.environ.get("APPDATA", "")
        localappdata = os.environ.get("LOCALAPPDATA", "")
        if appdata:
            dirs.append(os.path.join(appdata, "Claude"))
            dirs.append(os.path.join(appdata, "claude-code"))
        if localappdata:
            dirs.append(os.path.join(localappdata, "Claude"))
    elif system == "Darwin": # macOS
        dirs.append(os.path.join(user_home, "Library", "Application Support", "Claude"))
        dirs.append(os.path.join(user_home, "Library", "Application Support", "claude-code"))
        dirs.append(os.path.join(user_home, "Library", "Logs", "Claude"))
    
    return [d for d in dirs if os.path.exists(d)]


def find_serial_port():
    """Auto-detects ESP32 USB CDC serial port on Windows, macOS, and Linux."""
    system = platform.system()
    for port in serial.tools.list_ports.comports():
        dev = (port.device or "").lower()
        desc = (port.description or "").lower()
        hwid = (port.hwid or "").lower()
        
        # Skip Bluetooth incoming ports on macOS / Windows
        if "bluetooth" in dev or "bluetooth" in desc:
            continue

        if system == "Windows":
            if dev.startswith("com") or "usb" in desc or "cdc" in desc or "jtag" in desc:
                return port.device
        else: # macOS / Linux
            if any(k in dev for k in ["usbmodem", "usbserial", "wchusbserial", "slab_usb", "ttyacm", "ttyusb"]):
                return port.device
            if any(k in desc or k in hwid for k in ["cdc", "usb serial", "usb-jtag", "esp32", "ch340", "cp210"]):
                return port.device
    return None


# ---------------------------------------------------------------------------
# Token Scanner & Active Session Monitor
# ---------------------------------------------------------------------------
_start_time = time.time()
_last_log_check_time = 0
_cached_tokens = 0

# Incremental transcript scan state (see scan_real_claude_tokens)
_scan_day = None
_file_offsets = {}        # path -> byte offset already consumed
_seen_message_ids = set() # (message.id, requestId) already counted today
_today_tokens_acc = 0

RATE_LIMIT_CACHE = os.path.expanduser(os.path.join("~", ".claude", "companion_rate_limits.json"))


def format_reset(epoch_ts):
    """Formats a Unix epoch reset timestamp as a human-readable countdown."""
    if not epoch_ts:
        return "--"
    delta = epoch_ts - time.time()
    if delta <= 0:
        return "0m"
    days = int(delta // 86400)
    hours = int((delta % 86400) // 3600)
    minutes = int((delta % 3600) // 60)
    if days > 0:
        return f"{days}d {hours}h"
    if hours > 0:
        return f"{hours}h {minutes}m"
    return f"{minutes}m"


def read_real_usage():
    """Reads the real 5h/weekly quota usage captured by claude_statusline.py.

    That data comes from Claude Code's own statusline payload (rate_limits),
    the only local source of the account's actual quota percentages - it
    can't be derived from token counts in transcripts.
    """
    try:
        with open(RATE_LIMIT_CACHE, "r", encoding="utf-8") as f:
            data = json.load(f)
    except Exception:
        return None

    if data.get("five_hour_pct") is None and data.get("week_pct") is None:
        return None

    # The cache is only refreshed while Claude Code is open. Once a window's
    # reset time has passed, its last % is stale: that window is at 0 now.
    now = time.time()
    for pct_key, reset_key in (("five_hour_pct", "five_hour_resets_at"), ("week_pct", "week_resets_at")):
        resets_at = data.get(reset_key)
        try:
            expired = resets_at is not None and float(resets_at) <= now
        except (TypeError, ValueError):
            expired = False
        if expired:
            data[pct_key] = 0.0
            data[reset_key] = None
    return data


def _reset_token_state(day_str):
    """Clears the incremental scan state (called on start and when the day rolls over)."""
    global _scan_day, _file_offsets, _seen_message_ids, _today_tokens_acc
    _scan_day = day_str
    _file_offsets = {}
    _seen_message_ids = set()
    _today_tokens_acc = 0


def _count_usage_line(raw_line, today_start_ts):
    """Returns the tokens of one transcript line, or 0 if it is not new usage from today.

    An assistant message is written as several lines (one per content block),
    all carrying the same usage, so each message is counted once by its id.
    """
    try:
        entry = json.loads(raw_line)
    except Exception:
        return 0
    if not isinstance(entry, dict):
        return 0
    message = entry.get("message") or {}
    if not isinstance(message, dict):
        message = {}
    usage = entry.get("usage") or message.get("usage") or {}
    if not usage:
        return 0

    ts = entry.get("timestamp")
    if ts:
        try:
            entry_ts = datetime.datetime.fromisoformat(ts.replace("Z", "+00:00")).timestamp()
            if entry_ts < today_start_ts:
                return 0
        except Exception:
            pass

    msg_id = message.get("id")
    if msg_id:
        key = (msg_id, entry.get("requestId"))
        if key in _seen_message_ids:
            return 0
        _seen_message_ids.add(key)

    inp = (usage.get("input_tokens") or 0) + (usage.get("cache_creation_input_tokens") or 0)
    out = usage.get("output_tokens") or 0
    return inp + out


def scan_real_claude_tokens():
    """Today's Claude Code token consumption from ~/.claude/projects transcripts.

    Reads each .jsonl incrementally (only the bytes appended since the last
    scan) and keeps a running total that resets when the day changes.
    Falls back to stats-cache.json when no transcript activity is found.
    """
    global _last_log_check_time, _cached_tokens, _today_tokens_acc
    now = time.time()

    if now - _last_log_check_time < 2 and _cached_tokens >= 0:
        return _cached_tokens

    _last_log_check_time = now

    now_dt = datetime.datetime.now()
    today_start_ts = now_dt.replace(hour=0, minute=0, second=0, microsecond=0).timestamp()
    today_str = now_dt.strftime("%Y-%m-%d")

    if _scan_day != today_str:
        _reset_token_state(today_str)

    claude_dirs = get_claude_dirs()

    # 1. Scan active session .jsonl files in ~/.claude/projects for today's tokens
    for c_dir in claude_dirs:
        projects_dir = os.path.join(c_dir, "projects")
        if not os.path.exists(projects_dir):
            continue
        for root, dirs, files in os.walk(projects_dir):
            for f in files:
                if not f.endswith(".jsonl"):
                    continue
                p = os.path.join(root, f)
                try:
                    st = os.stat(p)
                    if st.st_mtime < today_start_ts:
                        continue
                    offset = _file_offsets.get(p, 0)
                    if st.st_size < offset:
                        offset = 0  # file was truncated/rewritten; dedup avoids double counting
                    if st.st_size == offset:
                        continue
                    with open(p, "rb") as jf:
                        jf.seek(offset)
                        chunk = jf.read()
                    # Only consume complete lines; a partial last line is re-read next scan
                    last_nl = chunk.rfind(b"\n")
                    if last_nl < 0:
                        continue
                    for raw in chunk[:last_nl].split(b"\n"):
                        if raw.strip():
                            _today_tokens_acc += _count_usage_line(raw, today_start_ts)
                    _file_offsets[p] = offset + last_nl + 1
                except Exception:
                    pass

    today_tokens = _today_tokens_acc
    if today_tokens > 0:
        _cached_tokens = today_tokens
        return today_tokens

    # 2. Fallback: check stats-cache.json for today's entry
    for c_dir in claude_dirs:
        stats_file = os.path.join(c_dir, "stats-cache.json")
        if os.path.exists(stats_file):
            try:
                with open(stats_file, "r", encoding="utf-8") as f:
                    sdata = json.load(f)
                    daily_models = sdata.get("dailyModelTokens", [])
                    for entry in reversed(daily_models):
                        if entry.get("date") == today_str:
                            tokens = sum(entry.get("tokensByModel", {}).values())
                            if tokens > 0:
                                _cached_tokens = tokens
                                return tokens
            except Exception:
                pass

    _cached_tokens = today_tokens
    return today_tokens


CLAUDE_THINKING_VERBS = [
    # Cerebrales y de pensamiento
    "Cogitating...",
    "Ruminating...",
    "Contemplating...",
    "Deliberating...",
    "Pondering...",
    "Cerebrating...",
    "Musing...",
    "Inferring...",
    "Deciphering...",
    # Culinarias
    "Baking...",
    "Brewing...",
    "Marinating...",
    "Simmering...",
    "Stewing...",
    "Percolating...",
    "Fermenting...",
    "Caramelizing...",
    # Orgánicas y crecimiento
    "Incubating...",
    "Germinating...",
    "Crystallizing...",
    "Weaving...",
    # Humorísticas y extravagantes
    "Clauding...",
    "Schlepping...",
    "Moseying...",
    "Honking...",
    "Actioning...",
    "Actualizing...",
    "Jitterbugging...",
    "Hoolabalooing...",
    "Calculating...",
    "Computing...",
    "Conjuring..."
]


def is_claude_actively_thinking():
    """Checks if Claude Code is actively generating/thinking right now.
    
    Returns True if a Claude process is running AND the active transcript log 
    was modified within the last 12 seconds.
    """
    now = time.time()
    process_found = False
    for proc in psutil.process_iter(['name', 'cmdline']):
        try:
            name = (proc.info.get('name') or '').lower()
            cmd = " ".join([str(c) for c in (proc.info.get('cmdline') or []) if c]).lower()
            if 'claude' in name or 'claude' in cmd or 'antigravity' in name:
                process_found = True
                break
        except Exception:
            pass

    if not process_found:
        return False

    claude_dirs = get_claude_dirs()
    for c_dir in claude_dirs:
        projects_dir = os.path.join(c_dir, "projects")
        if os.path.exists(projects_dir):
            for root, dirs, files in os.walk(projects_dir):
                for f in files:
                    if f.endswith(".jsonl"):
                        try:
                            mtime = os.path.getmtime(os.path.join(root, f))
                            if (now - mtime) <= 12:
                                return True
                        except Exception:
                            pass
    return False


def format_claude_account(email_or_name, max_len=21):
    """Formats the account identifier to fit neatly on the 240px companion screen."""
    if not email_or_name:
        return ""
    val = email_or_name.strip()
    if len(val) <= max_len:
        return val

    # If it's an email address, try smart shortening:
    if "@" in val:
        user, domain = val.split("@", 1)
        # e.g. "alejandro.cabo1991@gmail.com" -> "a.cabo1991@gmail.com"
        if "." in user:
            parts = user.split(".")
            short_u = parts[0][0] + "." + ".".join(parts[1:])
            candidate = f"{short_u}@{domain}"
            if len(candidate) <= max_len:
                return candidate

        # If still too long, abbreviate user part with ellipsis:
        domain_part = f"@{domain}"
        avail_u = max_len - len(domain_part) - 2
        if avail_u >= 3:
            return f"{user[:avail_u]}..{domain_part}"

        # If domain is huge, return username
        if len(user) <= max_len:
            return user
        return user[:max_len - 2] + ".."

    return val[:max_len - 2] + ".."


def get_claude_account():
    """Reads ~/.claude.json to extract the currently logged-in account email or name."""
    candidate_paths = [
        os.path.expanduser("~/.claude.json"),
        os.path.expanduser(os.path.join("~", ".claude", "claude.json")),
    ]
    for p in candidate_paths:
        if os.path.exists(p):
            try:
                with open(p, "r", encoding="utf-8") as f:
                    data = json.load(f)
                    oauth = data.get("oauthAccount") or {}
                    email = oauth.get("emailAddress") or ""
                    if email:
                        return format_claude_account(email)
                    name = oauth.get("displayName") or oauth.get("fullName") or ""
                    if name:
                        return format_claude_account(name)
            except Exception:
                pass
    return ""


def get_claude_metrics():
    """Returns real-time Claude metrics, process status, and mascot mood."""
    actively_thinking = is_claude_actively_thinking()

    # Detect if any claude process is running at all
    claude_running = False
    for proc in psutil.process_iter(['name', 'cmdline']):
        try:
            name = (proc.info.get('name') or '').lower()
            cmd = " ".join([str(c) for c in (proc.info.get('cmdline') or []) if c]).lower()
            if 'claude' in name or 'claude' in cmd or 'antigravity' in name:
                claude_running = True
                break
        except Exception:
            pass

    tokens = scan_real_claude_tokens()
    elapsed = int(time.time() - _start_time)
    real_usage = read_real_usage()
    user_acc = get_claude_account()

    if real_usage:
        h5_pct = real_usage.get("five_hour_pct") or 0.0
        h5_reset = format_reset(real_usage.get("five_hour_resets_at"))
        week_pct = real_usage.get("week_pct") or 0.0
        week_reset = format_reset(real_usage.get("week_resets_at"))
    else:
        h5_pct = round(min(99.9, (tokens % 100000) / 1000.0), 1)
        h5_reset = f"{int(59 - ((elapsed // 60) % 60))}m" if claude_running else "29m"
        week_pct = round(min(99.9, (tokens / 500000.0) * 100.0), 1)
        week_reset = "4d 22h"

    if actively_thinking:
        # Cycle through official Claude Code thinking verbs every 30 seconds
        verb_idx = (elapsed // 30) % len(CLAUDE_THINKING_VERBS)
        status = CLAUDE_THINKING_VERBS[verb_idx]
        mood = "coding" if (verb_idx % 2 == 0) else "thinking"
    else:
        # Show nothing (blank) when idle, exactly like terminal prompt
        status = ""
        moods = ["happy", "hyped", "sleeping", "happy"]
        mood = moods[(elapsed // 10) % len(moods)]

    return {
        "c_tok": tokens,
        "c_5h": h5_pct,
        "c_5hr": h5_reset,
        "c_wk": week_pct,
        "c_wkr": week_reset,
        "c_st": status,
        "c_user": user_acc,
        "claw_mood": mood
    }


# ---------------------------------------------------------------------------
# Claude Code hook events ("keep me alert") -> Claw mascot reactions
# ---------------------------------------------------------------------------
EVENTS_PATH = os.path.expanduser(os.path.join("~", ".claude", "companion_events.jsonl"))
EVENT_TTL_SEC = 30       # stop forwarding an event after this long
EVENT_STICKY_SEC = 10    # a "done" does not hide a fresher "input"/"fail"
_events_offset = None    # None = not opened yet (start at end of file)
_current_event = None


def _ascii(text, max_len):
    """The ST7789 font is plain ASCII: drop accents/emoji and clamp length."""
    import unicodedata
    text = unicodedata.normalize("NFKD", text or "").encode("ascii", "ignore").decode("ascii")
    return text[:max_len]


def get_claw_event():
    """Tails companion_events.jsonl (written by claude_events_hook.py) and returns
    the serial fields for the latest event, or {} when there is none recent."""
    global _events_offset, _current_event
    try:
        size = os.path.getsize(EVENTS_PATH)
    except OSError:
        size = None

    if size is not None:
        if _events_offset is None or size < _events_offset:
            # First run: skip history so old events don't fire on agent start.
            # Shrunk file: the hook truncated it, read it from the beginning.
            _events_offset = size if _events_offset is None else 0
        if size > _events_offset:
            try:
                with open(EVENTS_PATH, "rb") as f:
                    f.seek(_events_offset)
                    chunk = f.read()
                last_nl = chunk.rfind(b"\n")
                if last_nl >= 0:
                    _events_offset += last_nl + 1
                    for raw in chunk[:last_nl].split(b"\n"):
                        try:
                            ev = json.loads(raw)
                        except Exception:
                            continue
                        if not isinstance(ev, dict) or not ev.get("ev"):
                            continue
                        cur = _current_event
                        if (ev["ev"] == "done" and cur and cur.get("ev") in ("input", "fail")
                                and (ev.get("ts", 0) - cur.get("ts", 0)) < EVENT_STICKY_SEC):
                            continue
                        _current_event = ev
            except OSError:
                pass

    ev = _current_event
    if not ev or time.time() - float(ev.get("ts", 0)) > EVENT_TTL_SEC:
        return {}
    return {
        "claw_ev": ev.get("ev", ""),
        "claw_ev_txt": _ascii(ev.get("text", ""), 16),
        "claw_ev_prj": _ascii(ev.get("sub", ev.get("project", "")), 32),
        "claw_ev_id": int(float(ev.get("ts", 0)) * 1000) % 2147483647,
    }


def get_antigravity_metrics():
    """Scans real Antigravity (~/.gemini/antigravity) transcripts and process activity for live metrics."""
    ag_dir = os.path.expanduser(os.path.join("~", ".gemini", "antigravity"))
    brain_dir = os.path.join(ag_dir, "brain")

    ag_active = False
    model_name = "Gemini 3.6 Flash"
    last_tool = "standby"
    subagents_count = 0
    total_tokens = 84200
    context_pct = 18.5

    # 1. Process activity scan
    for proc in psutil.process_iter(['name', 'cmdline']):
        try:
            name = (proc.info.get('name') or '').lower()
            cmd_args = [str(c) for c in (proc.info.get('cmdline') or []) if c]
            cmd = " ".join(cmd_args).lower()
            if any(k in name or k in cmd for k in ['antigravity', 'gemini', 'cagent']):
                ag_active = True
                break
        except Exception:
            pass

    # 2. Real transcript inspection from ~/.gemini/antigravity/brain
    if os.path.exists(brain_dir):
        transcripts = glob.glob(os.path.join(brain_dir, "*", ".system_generated", "logs", "transcript.jsonl"))
        if transcripts:
            latest_transcript = max(transcripts, key=os.path.getmtime)
            mtime = os.path.getmtime(latest_transcript)
            age_sec = time.time() - mtime
            file_size = os.path.getsize(latest_transcript)

            # If modified in last 60s, Antigravity is actively processing!
            if age_sec < 60:
                ag_active = True

            try:
                with open(latest_transcript, "r", encoding="utf-8", errors="ignore") as f:
                    lines = f.readlines()

                total_steps = len(lines)
                total_tokens = int(file_size / 4.0)
                context_pct = round(min(99.0, max(1.0, (file_size / 1572864.0) * 100.0)), 1)

                # Scan recent steps for tool calls, model settings, and subagents
                for line in reversed(lines[-30:]):
                    try:
                        data = json.loads(line)
                        content = str(data.get("content") or "")

                        if "Gemini 3.6 Pro" in content or "pro" in content.lower():
                            model_name = "Gemini 3.6 Pro"

                        t_calls = data.get("tool_calls", [])
                        if t_calls and last_tool == "standby":
                            t_name = t_calls[0].get("name", "")
                            if t_name:
                                if "replace_file" in t_name: last_tool = "replace_file"
                                elif "run_command" in t_name: last_tool = "run_command"
                                elif "view_file" in t_name: last_tool = "view_file"
                                elif "write_to_file" in t_name: last_tool = "write_file"
                                elif "list_dir" in t_name: last_tool = "list_dir"
                                elif "grep_search" in t_name: last_tool = "grep_search"
                                elif "ask_question" in t_name: last_tool = "ask_question"
                                else: last_tool = t_name[:12]

                        if "invoke_subagent" in line or "define_subagent" in line:
                            subagents_count += 1
                    except Exception:
                        pass
            except Exception:
                pass

    status = "Coding..." if ag_active and (int(time.time()) % 4 < 2) else ("Pensando..." if ag_active else "Idle")
    if not ag_active:
        last_tool = "standby"

    return {
        "ag_m": model_name,
        "ag_st": status,
        "ag_tok": total_tokens,
        "ag_ctx": context_pct,
        "ag_sub": subagents_count,
        "ag_t": last_tool
    }


# ---------------------------------------------------------------------------
# OpenSky Network Live Flight Radar Metrics (Central Madrid 25km Radius)
# ---------------------------------------------------------------------------
MADRID_LAT = 40.4168
MADRID_LON = -3.7038

AIRLINES = {
    "IBE": "Iberia", "IBS": "Iberia Express", "ANE": "Air Nostrum", "AEA": "Air Europa",
    "RYR": "Ryanair", "VLG": "Vueling", "EJU": "easyJet", "EZY": "easyJet",
    "TAP": "TAP Portugal", "DLH": "Lufthansa", "LHT": "Lufthansa", "AFR": "Air France",
    "BAW": "British Airways", "SWR": "Swiss", "UAE": "Emirates", "QTR": "Qatar Airways",
    "AAL": "American", "DAL": "Delta", "UAL": "United",
    "AME": "Ejercito Aire", "EJC": "Ejercito Tierra", "TRT": "Fuerza Aerea",
    "FDX": "FedEx Cargo", "UPS": "UPS Cargo", "DHL": "DHL Cargo", "BCS": "DHL Cargo",
    "SWT": "Swiftair Cargo", "CYO": "Cygnus Cargo", "TNT": "TNT Cargo", "CLX": "Cargolux"
}

MILITARY_PREFIXES = {
    "AME", "EJC", "TRT", "VJP", "RCH", "CNV", "FORTE", "LAGR", "BAF", "GAF",
    "FAF", "RRR", "IAM", "HRZ", "NATO", "AF1", "SAM", "FSM"
}

CARGO_PREFIXES = {
    "FDX", "UPS", "DHL", "BCS", "QNK", "DHK", "DHX", "SWT", "CYO", "TNT", "TAY",
    "CLX", "GTI", "CKS", "NPT", "PAC", "BOX", "SQC", "MNB", "ABW", "TFL"
}


def get_flight_category(icao24, callsign):
    """
    Returns category code:
    1 = Military (Red)
    2 = Cargo (Green)
    0 = Commercial Passenger (Yellow)
    """
    icao = (icao24 or "").lower()
    call = (callsign or "").strip().upper()

    for prefix in MILITARY_PREFIXES:
        if call.startswith(prefix):
            return 1
    if icao.startswith("ae") or icao.startswith("af") or icao.startswith("35") or icao.startswith("348") or icao.startswith("349") or icao.startswith("43c") or icao.startswith("3b") or icao.startswith("3e"):
        return 1

    for prefix in CARGO_PREFIXES:
        if call.startswith(prefix):
            return 2

    return 0


def check_is_military(icao24, callsign):
    return get_flight_category(icao24, callsign) == 1


# Disk Cache for Flight Metrics to guarantee persistent flight data against OpenSky HTTP 429 rate-limiting
FLIGHT_CACHE_FILE = os.path.expanduser(os.path.join("~", ".gemini", "antigravity", "flight_cache.json"))


def load_flight_cache_from_disk():
    try:
        if os.path.exists(FLIGHT_CACHE_FILE):
            with open(FLIGHT_CACHE_FILE, "r", encoding="utf-8") as f:
                data = json.load(f)
                if isinstance(data, dict) and data.get("fl_cnt", 0) > 0:
                    return data
    except Exception:
        pass
    return None


def save_flight_cache_to_disk(payload):
    try:
        os.makedirs(os.path.dirname(FLIGHT_CACHE_FILE), exist_ok=True)
        with open(FLIGHT_CACHE_FILE, "w", encoding="utf-8") as f:
            json.dump(payload, f)
    except Exception:
        pass


_last_flight_fetch_time = 0
_cached_flight_payload = load_flight_cache_from_disk() or {
    "fl_act": False, "fl_call": "Sin Vuelos", "fl_air": "Madrid Sky", "fl_type": "Comercial",
    "fl_alt": 0, "fl_spd": 0, "fl_dist": 0.0, "fl_head": 0, "fl_dir": "N", "fl_cnt": 0
}


def haversine_dist(lat1, lon1, lat2, lon2):
    R = 6371.0
    dlat = math.radians(lat2 - lat1)
    dlon = math.radians(lon2 - lon1)
    a = math.sin(dlat / 2)**2 + math.cos(math.radians(lat1)) * math.cos(math.radians(lat2)) * math.sin(dlon / 2)**2
    c = 2 * math.atan2(math.sqrt(a), math.sqrt(1 - a))
    return R * c


def bearing_deg(lat1, lon1, lat2, lon2):
    dlon = math.radians(lon2 - lon1)
    y = math.sin(dlon) * math.cos(math.radians(lat2))
    x = math.cos(math.radians(lat1)) * math.sin(math.radians(lat2)) - math.sin(math.radians(lat1)) * math.cos(math.radians(lat2)) * math.cos(dlon)
    b = math.degrees(math.atan2(y, x))
    return int((b + 360) % 360)


def cardinal_dir(b):
    dirs = ["N", "NE", "E", "SE", "S", "SW", "W", "NW"]
    idx = int((b + 22.5) // 45) % 8
    return dirs[idx]


def get_flight_radar_metrics():
    """Fetches live ADS-B flights around Central Madrid (25km radius) using FlightRadar24 (Primary) + OpenSky (Fallback)."""
    global _last_flight_fetch_time, _cached_flight_payload
    now = time.time()

    # 3s cache interval for true real-time flight tracking
    if now - _last_flight_fetch_time < 3 and _cached_flight_payload.get("fl_cnt", 0) > 0:
        return _cached_flight_payload

    _last_flight_fetch_time = now
    flights = []

    # 1. Primary High-Speed Real-Time ADS-B Provider: FlightRadar24 Cloud Feed
    try:
        url_fr24 = "https://data-cloud.flightradar24.com/zones/fcgi/feed.js?faa=1&satellite=1&mlat=1&flarm=1&adsb=1&gnd=0&air=1&vehicles=0&estimated=1&maxage=14400&gliders=0&stats=1&bounds=40.80,40.00,-4.30,-3.10"
        headers_fr24 = {
            "User-Agent": "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/128.0.0.0 Safari/537.36",
            "Accept": "application/json"
        }
        req = urllib.request.Request(url_fr24, headers=headers_fr24)
        with urllib.request.urlopen(req, timeout=4) as resp:
            data = json.loads(resp.read().decode())
            for k, v in data.items():
                if not isinstance(v, list) or len(v) < 6:
                    continue
                icao24 = (v[0] or "").lower()
                lat = v[1]
                lon = v[2]
                track = v[3]
                alt_ft = v[4]
                spd_kts = v[5]
                callsign = v[16] if len(v) > 16 and v[16] else (v[13] if len(v) > 13 and v[13] else icao24.upper())
                callsign = str(callsign).strip()

                if alt_ft < 250 or lat is None or lon is None:
                    continue  # Ignore grounded / parked vehicles

                dist_km = haversine_dist(MADRID_LAT, MADRID_LON, lat, lon)
                brg = bearing_deg(MADRID_LAT, MADRID_LON, lat, lon)
                card = cardinal_dir(brg)
                cat = get_flight_category(icao24, callsign)
                is_mil = (cat == 1)
                is_cargo = (cat == 2)
                airline = "Militar" if is_mil else ("Cargo" if is_cargo else "Comercial")
                if len(callsign) >= 3 and callsign[:3].upper() in AIRLINES:
                    airline = AIRLINES[callsign[:3].upper()]

                flights.append({
                    "callsign": callsign,
                    "airline": airline,
                    "dist_km": round(dist_km, 1),
                    "alt_m": int(alt_ft * 0.3048),
                    "spd_kmh": int(spd_kts * 1.852),
                    "bearing": int(brg),
                    "cardinal": card,
                    "track": int(track),
                    "cat": cat,
                    "is_military": is_mil
                })
    except Exception as fr24_err:
        # 2. Secondary Fallback Provider: OpenSky Network
        try:
            lamin, lomin = 40.00, -4.20
            lamax, lomax = 40.80, -3.20
            url_os = f"https://opensky-network.org/api/states/all?lamin={lamin}&lomin={lomin}&lamax={lamax}&lomax={lomax}"
            req = urllib.request.Request(url_os, headers={"User-Agent": "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7)"})
            with urllib.request.urlopen(req, timeout=5) as resp:
                data = json.loads(resp.read().decode())
                states = data.get("states", []) or []
                for s in states:
                    icao24 = s[0] or ""
                    callsign = (s[1] or "").strip()
                    lon, lat, alt_m, on_ground, vel_ms, track = s[5], s[6], s[7] or 0, s[8], s[9] or 0, s[10] or 0
                    if on_ground or lat is None or lon is None:
                        continue
                    dist_km = haversine_dist(MADRID_LAT, MADRID_LON, lat, lon)
                    brg = bearing_deg(MADRID_LAT, MADRID_LON, lat, lon)
                    card = cardinal_dir(brg)
                    cat = get_flight_category(icao24, callsign)
                    is_mil = (cat == 1)
                    is_cargo = (cat == 2)
                    airline = "Militar" if is_mil else ("Cargo" if is_cargo else "Comercial")
                    if len(callsign) >= 3 and callsign[:3].upper() in AIRLINES:
                        airline = AIRLINES[callsign[:3].upper()]
                    flights.append({
                        "callsign": callsign or icao24.upper(),
                        "airline": airline,
                        "dist_km": round(dist_km, 1),
                        "alt_m": int(alt_m),
                        "spd_kmh": int(vel_ms * 3.6),
                        "bearing": int(brg),
                        "cardinal": card,
                        "track": int(track),
                        "cat": cat,
                        "is_military": is_mil
                    })
        except Exception as os_err:
            pass

    if flights:
        flights.sort(key=lambda x: x["dist_km"])
        f = flights[0]
        type_str = "Militar" if f["cat"] == 1 else ("Cargo" if f["cat"] == 2 else "Comercial")
        new_payload = {
            "fl_act": True,
            "fl_call": f["callsign"][:12],
            "fl_air": f["airline"][:12],
            "fl_type": type_str,
            "fl_alt": f["alt_m"],
            "fl_spd": f["spd_kmh"],
            "fl_dist": f["dist_km"],
            "fl_head": f["bearing"],
            "fl_dir": f["cardinal"],
            "fl_cnt": len(flights)
        }

        # Populate multi-flight entries (up to 8 flights) for Radar Scope & Cards List
        for idx, item in enumerate(flights[:8], 1):
            new_payload[f"fl_c{idx}"] = item["callsign"][:10]
            new_payload[f"fl_a{idx}"] = item["airline"][:10]
            new_payload[f"fl_d{idx}"] = item["dist_km"]
            new_payload[f"fl_h{idx}"] = item["bearing"]
            new_payload[f"fl_dir{idx}"] = item["cardinal"]
            new_payload[f"fl_alt{idx}"] = item["alt_m"]
            new_payload[f"fl_spd{idx}"] = item["spd_kmh"]
            new_payload[f"fl_m{idx}"] = 1 if item["cat"] == 1 else 0
            new_payload[f"fl_t{idx}"] = item["cat"] # 0=Pax, 1=Mil, 2=Cargo

        _cached_flight_payload = new_payload
        save_flight_cache_to_disk(new_payload)
        return _cached_flight_payload

    # Try loading disk cache if RAM payload was empty
    disk_cache = load_flight_cache_from_disk()
    if disk_cache:
        _cached_flight_payload = disk_cache
        return _cached_flight_payload

    _cached_flight_payload = {
        "fl_act": False, "fl_call": "Sin Vuelos", "fl_air": "Madrid Sky", "fl_type": "Comercial",
        "fl_alt": 0, "fl_spd": 0, "fl_dist": 0.0, "fl_head": 0, "fl_dir": "N", "fl_cnt": 0
    }
    return _cached_flight_payload


def main():
    print(f"=== ESP32-C3 Desktop Companion Agent ({platform.system()}) ===", flush=True)
    
    claude_dirs = get_claude_dirs()
    if claude_dirs:
        print(f"Detected Claude directories: {claude_dirs}", flush=True)
    else:
        print("No local Claude directory found. Will use auto-fallback metrics.", flush=True)

    explicit_port = sys.argv[1] if len(sys.argv) > 1 else None

    while True:
        port = explicit_port or find_serial_port()
        if not port:
            print(f"[{datetime.datetime.now().strftime('%H:%M:%S')}] Searching for ESP32 USB CDC serial port...", flush=True)
            time.sleep(RECONNECT_INTERVAL_SEC)
            continue

        print(f"[{datetime.datetime.now().strftime('%H:%M:%S')}] Connecting to {port} @ {BAUD_RATE} baud...", flush=True)

        try:
            with serial.Serial(port, BAUD_RATE, timeout=1) as ser:
                ser.dtr = False
                ser.rts = False
                print(f"[{datetime.datetime.now().strftime('%H:%M:%S')}] Connected to {port}! Streaming AI & Flight Radar metrics every {SEND_INTERVAL_SEC}s.", flush=True)
                while True:
                    try:
                        payload = get_claude_metrics()
                        payload.update(get_claw_event())
                        payload.update(get_antigravity_metrics())
                        flight_p = get_flight_radar_metrics()
                        payload.update(flight_p)

                        line = json.dumps(payload) + "\n"
                        ser.write(line.encode("utf-8"))
                        ser.flush()

                        # Verbose Console Status Logging
                        fl_cnt = flight_p.get("fl_cnt", 0)
                        fl_act = flight_p.get("fl_act", False)
                        c_tok = payload.get("c_tok", 0)
                        c_usr = payload.get("c_user", "")
                        ag_m = payload.get("ag_m", "Flash")
                        ag_st = payload.get("ag_st", "Idle")

                        usr_log = f" [{c_usr}]" if c_usr else ""
                        if payload.get("claw_ev"):
                            usr_log += f" | Event={payload['claw_ev']} ({payload.get('claw_ev_prj', '')})"
                        print(f"[{datetime.datetime.now().strftime('%H:%M:%S')}] TX Serial ({len(line)} B): Claude={c_tok:,} tok{usr_log} | AG={ag_m} [{ag_st}] | Flights Overhead={fl_cnt} (Active={fl_act})", flush=True)
                        if fl_cnt > 0:
                            for idx in range(1, min(fl_cnt, 8) + 1):
                                c_name = flight_p.get(f"fl_c{idx}", "")
                                c_air = flight_p.get(f"fl_a{idx}", "")
                                c_dist = flight_p.get(f"fl_d{idx}", 0.0)
                                c_dir = flight_p.get(f"fl_dir{idx}", "N")
                                cat_code = flight_p.get(f"fl_t{idx}", 0)
                                if cat_code == 1:
                                    c_type_str = "🔴 MILITAR"
                                elif cat_code == 2:
                                    c_type_str = "🟢 CARGO"
                                else:
                                    c_type_str = "🟡 PASAJEROS"
                                if c_name:
                                    print(f"   ✈ {idx}. {c_name} ({c_air}) - {c_dist}km {c_dir} [{c_type_str}]", flush=True)

                    except (serial.SerialException, OSError) as write_err:
                        print(f"Serial write error ({write_err}). Reconnecting...", flush=True)
                        break
                    except Exception as loop_err:
                        print(f"Metrics calculation error: {loop_err}", flush=True)
                    time.sleep(SEND_INTERVAL_SEC)
        except serial.SerialException as exc:
            print(f"Serial connection lost/error ({exc}). Retrying in {RECONNECT_INTERVAL_SEC}s...", flush=True)
            time.sleep(RECONNECT_INTERVAL_SEC)
        except KeyboardInterrupt:
            print("\nAgent stopped.", flush=True)
            break


if __name__ == "__main__":
    main()
