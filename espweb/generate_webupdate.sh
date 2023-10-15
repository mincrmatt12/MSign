#!/bin/sh

rm -rf web
yarn build

cd web
mv page.*.css page.css
mv page.*.js page.js
mv vendor.*.js vendor.js
gzip -k -9 page.css page.html page.js vendor.js
ar q webui.ar *
