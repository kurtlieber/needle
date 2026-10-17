#!/bin/sh
# usage: shot.sh page.html out.png [scale]
cd "$(dirname "$0")"
chromium --headless=new --disable-gpu --hide-scrollbars --no-first-run --no-default-browser-check \
  --window-size=1600,1000 --force-device-scale-factor="${3:-1}" --virtual-time-budget=8000 \
  --screenshot="$2" "file://$PWD/$1" 2>/dev/null
