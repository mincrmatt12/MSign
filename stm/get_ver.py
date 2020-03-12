import subprocess
from datetime import datetime

revision = subprocess.check_output(["git", "rev-parse", "HEAD"]).strip()
try:
    subprocess.check_call(["git", "diff", "--quiet", "HEAD"])
except subprocess.CalledProcessError:
    revision += "-dev"

print("-DMSIGN_GIT_REV=\\\"-{}\\\"".format(revision[:7]))
