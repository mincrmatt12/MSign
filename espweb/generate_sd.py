"""
This script does two things, the first is running webpack to generate the pack.js, css and html files.
Then it creates an archive file containing the ca certificates. It takes all the certs in the cert folder, converts them to der if neccesary and creates the cacert.ar file.
The SD card structure is then located in the build folder
"""

import os
import shutil
from subprocess import Popen, PIPE, call

def create_directory():
    if not os.path.exists("build"):
        os.mkdir("build")
    if os.path.exists("build/web"):
        shutil.rmtree("build/web")


def create_page_data():
    call(['yarn', 'build'])
    call(['gzip', '-k', '-f', '-8', 'web/page.html', 'web/page.js', 'web/page.css'])
    shutil.copytree("web", "build/web")


if __name__ == "__main__":
    create_directory()
    create_page_data()
