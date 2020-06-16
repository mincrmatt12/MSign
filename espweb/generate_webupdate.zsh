#!/usr/bin/zsh

rm -rf web
yarn build
cd web
gzip -k -8 page.css page.html page.js
ar q webui.ar *
