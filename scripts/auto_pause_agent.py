import os
import subprocess
import time

Import("env")

PLIST_PATH = os.path.expanduser("~/Library/LaunchAgents/com.user.companion-agent.plist")

def before_upload(source, target, env):
    if os.path.exists(PLIST_PATH):
        print("\n[AutoPause] Pausing companion-agent to free USB serial port...")
        subprocess.run(["launchctl", "unload", PLIST_PATH], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        time.sleep(0.5)

def after_upload(source, target, env):
    if os.path.exists(PLIST_PATH):
        print("\n[AutoPause] Resuming companion-agent service...")
        subprocess.run(["launchctl", "load", PLIST_PATH], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)

env.AddPreAction("upload", before_upload)
env.AddPostAction("upload", after_upload)
