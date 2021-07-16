import subprocess
from datetime import datetime

branch   = subprocess.check_output(["git", "rev-parse", "--abbrev-ref", "HEAD"]).decode("ascii").strip()
revision = subprocess.check_output(["git", "rev-parse", "HEAD"]).decode("ascii").strip()[:7]
try:
    subprocess.check_call(["git", "diff", "--quiet", "HEAD"])
except subprocess.CalledProcessError:
    revision += "-dev"

print("-{}-{}".format(branch, revision), end="")
