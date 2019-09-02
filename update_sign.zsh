#!/usr/bin/env zsh
pio run -e msign_board -d stm
pio run -e msign_board -d esp

curl --user "$1:$2" -F 'esp=@esp/.pio/build/msign_board/firmware.bin' -F 'stm=@stm/.pio/build/msign_board/firmware.bin' -X POST http://msign/a/updatefirm
