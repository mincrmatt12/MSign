"""
This script does two things, the first is running webpack to generate the pack.js, css and html files.
Then it creates an archive file containing the ca certificates. It takes all the certs in the cert folder, converts them to der if neccesary and creates the cacert.ar file.
The final step is it creates a zip archive which should be extracted to the root of the SD card
"""
