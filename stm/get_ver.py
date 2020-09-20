import subprocess
from datetime import datetime

revision = subprocess.check_output(["git", "rev-parse", "HEAD"]).decode("ascii").strip()[:7]
try:
    subprocess.check_call(["git", "diff", "--quiet", "HEAD"])
except subprocess.CalledProcessError:
    revision += "-dev"

print("-" + revision, end="")
