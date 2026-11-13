#!/usr/bin/env python3
"""
Claude Code hook for the ESP32-C3 Desktop Companion ("keep me alert").
---------------------------------------------------------------------
Appends one JSON line per relevant event to ~/.claude/companion_events.jsonl.
companion_agent.py tails that file and forwards the latest event over serial
so the Claw mascot can react (speech bubble, expression, page switch).

Events written ("ev"):
  input  - Notification: "<session> necesita permiso" / "te pregunta algo"
           (not the idle reminder after a finished turn)
  done   - Stop: "Tarea finalizada"
  fail   - PostToolUse / PostToolUseFailure (Bash): a real build or test
           failure in the output (FAILURE_PATTERNS), not any non-zero exit

Setup (in ~/.claude/settings.json, next to any existing hooks):
  "Notification":       [{"hooks": [{"type": "command", "command": "python3 /abs/path/claude_events_hook.py"}]}]
  "Stop":               [{"hooks": [{"type": "command", "command": "python3 /abs/path/claude_events_hook.py"}]}]
  "PostToolUse":        [{"matcher": "Bash", "hooks": [{"type": "command", "command": "python3 /abs/path/claude_events_hook.py"}]}]
  "PostToolUseFailure": [{"matcher": "Bash", "hooks": [{"type": "command", "command": "python3 /abs/path/claude_events_hook.py"}]}]

The hook never blocks Claude Code: it prints nothing and always exits 0.
"""
import glob
import json
import os
import re
import sys
import time

EVENTS_PATH = os.path.expanduser(os.path.join("~", ".claude", "companion_events.jsonl"))
MAX_FILE_BYTES = 256 * 1024  # truncate the log when it grows past this
# Only real build/test failures count; an ordinary non-zero exit (a grep with no
# match, a probe) is part of normal work and is ignored.
FAILURE_PATTERNS = [
    re.compile(r"BUILD FAILURE"),                        # Maven
    re.compile(r"BUILD FAILED"),                         # Gradle / Ant
    re.compile(r"=+ \[FAILED\] Took"),                 # PlatformIO
    re.compile(r"Tests run: \d+, Failures: [1-9]"),     # Surefire
    re.compile(r"Tests run: \d+, Failures: \d+, Errors: [1-9]"),
    re.compile(r"^npm ERR!", re.M),
]


def _text_of(value):
    if isinstance(value, str):
        return value
    if isinstance(value, (dict, list)):
        return json.dumps(value, ensure_ascii=False)
    return ""


def session_name(session_id):
    """Name of the Claude Code session (/rename or auto-derived), read from the
    ~/.claude/sessions/<pid>.json registry. Not a documented API: returns "" if
    the format changes or the session is not found."""
    if not session_id:
        return ""
    for path in glob.glob(os.path.expanduser(os.path.join("~", ".claude", "sessions", "*.json"))):
        try:
            with open(path, "r", encoding="utf-8") as f:
                data = json.load(f)
        except Exception:
            continue
        if isinstance(data, dict) and data.get("sessionId") == session_id:
            return str(data.get("name") or "")
    return ""


def classify(payload, who):
    """Returns (ev, title, subtitle) for the payload, or None when nothing should be shown.

    `who` is the session name (or project folder). The display shows the title
    big (max 15 chars) and the subtitle small underneath.
    """
    hook = payload.get("hook_event_name", "")

    if hook == "Notification":
        # Only when Claude Code needs an action from you. The idle reminder
        # ("waiting for your input") comes after a finished turn, which Stop
        # already reports as done.
        ntype = payload.get("notification_type", "")
        if ntype == "permission_prompt":
            return "input", who or "Claude Code", "necesita permiso"
        if ntype == "elicitation_dialog":
            return "input", who or "Claude Code", "te pregunta algo"
        if not ntype and "permission" in str(payload.get("message", "")).lower():
            return "input", who or "Claude Code", "necesita permiso"
        return None

    if hook == "Stop":
        return "done", "Tarea finalizada", who

    if hook in ("PostToolUse", "PostToolUseFailure"):
        out = _text_of(payload.get("tool_response")) + "\n" + _text_of(payload.get("error"))
        if any(p.search(out) for p in FAILURE_PATTERNS):
            return "fail", "BUILD FAIL!" if "BUILD" in out else "Tests/build KO", who
        return None

    return None


def main():
    try:
        payload = json.load(sys.stdin)
    except Exception:
        return
    if not isinstance(payload, dict):
        return

    if payload.get("hook_event_name") not in ("Notification", "Stop", "PostToolUse", "PostToolUseFailure"):
        return
    project = os.path.basename((payload.get("cwd") or "").rstrip(os.sep))
    who = session_name(payload.get("session_id")) or project

    result = classify(payload, who)
    if not result:
        return
    ev, text, sub = result

    record = {
        "ts": round(time.time(), 3),
        "ev": ev,
        "text": text,
        "sub": sub,
        "project": project,
        "session": (payload.get("session_id") or "")[:8],
    }
    try:
        os.makedirs(os.path.dirname(EVENTS_PATH), exist_ok=True)
        if os.path.exists(EVENTS_PATH) and os.path.getsize(EVENTS_PATH) > MAX_FILE_BYTES:
            open(EVENTS_PATH, "w").close()
        with open(EVENTS_PATH, "a", encoding="utf-8") as f:
            f.write(json.dumps(record, ensure_ascii=False) + "\n")
    except Exception:
        pass


if __name__ == "__main__":
    try:
        main()
    except Exception:
        pass
    sys.exit(0)
