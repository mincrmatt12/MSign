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
    if os.path.exists("build/ca"):
        shutil.rmtree("build/ca")
    os.mkdir("build/ca")
    if os.path.exists("build/web"):
        shutil.rmtree("build/web")


def copy_all_certificates():
    res = []

    for x in os.listdir("cert"):
        if os.path.splitext(x)[1] == ".der":
            shutil.copy(os.path.join("cert", x), os.path.join("build", x))
            res.append(os.path.join("build", x))
        else:
            newname = os.path.join("build", os.path.splitext(x)[0] + ".der")
            ssl = Popen(['openssl','x509','-inform','PEM','-outform','DER','-out', newname], shell = False, stdin = PIPE)
            pipe = ssl.stdin
            with open(os.path.join("cert", x)) as f:
                pipe.write(f.read())
            pipe.close()
            ssl.wait()
            res.append(newname)
    
    return res


def create_archive(files):
    print(files)
    cmd = ['ar', 'q', 'build/ca/cacert.ar'] + files
    print(cmd)
    call(cmd)

    for i in files:
        os.unlink(i)


def create_page_data():
    call(['yarn', 'build'])
    call(['gzip', '-k', '-f', '-8', 'web/page.html', 'web/page.js', 'web/page.css'])
    shutil.copytree("web", "build/web")


if __name__ == "__main__":
    create_directory()
    create_archive(copy_all_certificates())
    create_page_data()
