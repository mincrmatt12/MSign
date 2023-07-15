#!/bin/sh

if [ ${WEBUPDATE_NO_REBUILD:-0} -ne 1 ]; then
	rm -rf web
	yarn build
else
	rm -rf web/*.gz web/webui.ar
fi

cd web
gzip -k -9 page.css page.html page.js
ar q webui.ar *
