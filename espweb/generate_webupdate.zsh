#!/usr/bin/zsh

rm -rf web
yarn build
cd web
ar q webui.ar *
