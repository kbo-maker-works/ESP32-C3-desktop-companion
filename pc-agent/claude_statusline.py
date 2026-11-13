#!/usr/bin/env python3
"""
Claude Code statusLine hook for the ESP32-C3 Desktop Companion.
---------------------------------------------------------------
Claude Code invokes this script on every statusline update and pipes a JSON
payload to stdin that includes the account's REAL 5-hour and weekly quota
usage (`rate_limits.five_hour` / `rate_limits.seven_day`). That data is not
available anywhere else on disk, so this script caches it to a small JSON
file that companion_agent.py reads instead of estimating usage from token
counts.

Setup (in ~/.claude/settings.json):
{
  "statusLine": {
    "type": "command",
    "command": "python3 /absolute/path/to/pc-agent/claude_statusline.py"
  }
}

Note: rate_limits is only populated for Pro/Max subscription accounts (not
API-key billing), and only after the first API response of a session.
"""
import fcntl
import json
import os
import sys
import tempfile
import time
from datetime import datetime, timezone

CACHE_PATH = os.path.expanduser(os.path.join("~", ".claude", "companion_rate_limits.json"))
LOCK_PATH = CACHE_PATH + ".lock"
WINDOWS = (("five_hour", "five_hour_pct", "five_hour_resets_at"),
           ("seven_day", "week_pct", "week_resets_at"))


def normalize_ts(value):
    """resets_at as epoch seconds (int), or None. Accepts epoch s/ms (number or
    numeric string) and ISO 8601 ("Z" or offset; naive is taken as UTC)."""
    if value is None or isinstance(value, bool):
        return None
    if isinstance(value, (int, float)):
        ts = float(value)
    elif isinstance(value, str):
        text = value.strip()
        if not text:
            return None
        try:
            ts = float(text)
        except ValueError:
            try:
                dt = datetime.fromisoformat(text.replace("Z", "+00:00"))
            except ValueError:
                return None
            if dt.tzinfo is None:
                dt = dt.replace(tzinfo=timezone.utc)
            ts = dt.timestamp()
    else:
        return None
    if ts > 1e12:  # milliseconds
        ts /= 1000.0
    return int(ts)


def merge_window(old_pct, old_reset, new_pct, new_reset, now):
    """Merges one quota window. Several Claude Code sessions write the same
    cache and each only knows its own last API response, so an idle session
    must not overwrite a fresher value with an old one.

    Returns (pct, resets_at)."""
    if new_pct is None:
        return old_pct, old_reset                 # no data: keep what we have
    if old_pct is None:
        return new_pct, new_reset
    if old_reset is not None and old_reset <= now:
        return new_pct, new_reset                 # stored window expired
    if old_reset is None or new_reset is None:
        return max(old_pct, new_pct), (new_reset if new_reset is not None else old_reset)
    if new_reset > old_reset:
        return new_pct, new_reset                 # a new window started
    if new_reset < old_reset:
        return old_pct, old_reset                 # stale snapshot
    return max(old_pct, new_pct), old_reset       # same window: usage only grows


def merge_cache(old, rate_limits, now):
    """Returns the merged cache dict, or None if there is nothing to write."""
    old = old if isinstance(old, dict) else {}
    merged = {}
    got_any = False
    for window, pct_key, reset_key in WINDOWS:
        data = rate_limits.get(window) or {}
        new_pct = data.get("used_percentage")
        if new_pct is not None:
            got_any = True
        pct, reset = merge_window(old.get(pct_key), normalize_ts(old.get(reset_key)),
                                  new_pct, normalize_ts(data.get("resets_at")), now)
        merged[pct_key] = pct
        merged[reset_key] = reset
    if not got_any:
        return None
    merged["updated_at"] = int(now)
    return merged


def read_cache():
    try:
        with open(CACHE_PATH, "r", encoding="utf-8") as f:
            data = json.load(f)
        return data if isinstance(data, dict) else {}
    except Exception:
        return {}


def write_cache_atomic(cache):
    directory = os.path.dirname(CACHE_PATH)
    fd, tmp = tempfile.mkstemp(prefix=".companion_rate_limits.", suffix=".tmp", dir=directory)
    try:
        with os.fdopen(fd, "w", encoding="utf-8") as f:
            json.dump(cache, f)
        os.replace(tmp, CACHE_PATH)
    except Exception:
        try:
            os.unlink(tmp)
        except OSError:
            pass
        raise


def update_cache(rate_limits, now):
    """Read-merge-write under an exclusive lock so concurrent sessions don't
    lose each other's updates. Returns the cache as it now stands."""
    os.makedirs(os.path.dirname(CACHE_PATH), exist_ok=True)
    with open(LOCK_PATH, "a") as lock:
        fcntl.flock(lock, fcntl.LOCK_EX)
        try:
            current = read_cache()
            merged = merge_cache(current, rate_limits, now)
            if merged is None:
                return current
            if merged != current:
                write_cache_atomic(merged)
            return merged
        finally:
            fcntl.flock(lock, fcntl.LOCK_UN)


def main():
    try:
        payload = json.load(sys.stdin)
    except Exception:
        payload = {}

    rate_limits = payload.get("rate_limits") or {}
    try:
        cache = update_cache(rate_limits, time.time())
    except Exception:
        cache = {}
    five_pct = cache.get("five_hour_pct")
    week_pct = cache.get("week_pct")

    # Keep a normal-looking statusline so nothing regresses in the terminal.
    model = (payload.get("model") or {}).get("display_name", "Claude")
    cwd = (payload.get("workspace") or {}).get("current_dir", "")
    five_txt = f"{five_pct:.0f}%" if five_pct is not None else "--"
    week_txt = f"{week_pct:.0f}%" if week_pct is not None else "--"
    ctx_pct = (payload.get("context_window") or {}).get("used_percentage")
    ctx_txt = f"{ctx_pct:.0f}%" if ctx_pct is not None else "--"
    print(f"{model} | {os.path.basename(cwd)} | ctx:{ctx_txt} | 5h:{five_txt} wk:{week_txt}")


if __name__ == "__main__":
    main()
