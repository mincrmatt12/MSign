from setuptools import setup
import os

with open(os.path.join(os.path.dirname(__file__), "requirements.txt"), "r") as fh:
    install_requires = fh.read().splitlines()

setup(
    name="cfgserver",
    version="0.1.0",
    description="",
    packages=["cfgserver"],
    install_requires=install_requires,
    python_requires=">=3.7"
)
