#!/bin/sh

rm -rf web
yarn build
cd web
gzip -k -9 page.css page.html page.js
ar q webui.ar *
