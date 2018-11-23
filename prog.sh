#!/usr/bin/expect

#spawn bash -c "JLinkExe -device EFR32MG12PxxxF1024 -speed 4000 -if SWD -autoconnect 1 -SelectEmuBySN 440085446"
#expect "J-Link>"
#send "loadfile ./output/efr32/bin/ot-cli-ftd.hex\n"
#expect "J-Link>"
#send "r\n"
#expect "J-Link>"
#send "q\n"
#expect eof

spawn bash -c "arm-none-eabi-objcopy -O ihex ./output/nrf52840/bin/ot-cli-ftd ./output/nrf52840/bin/ot-cli-ftd.hex"
expect eof

spawn bash -c "mergehex -m ./xiaofang_trim_4s.hex  ./output/nrf52840/bin/ot-cli-ftd.hex  -o ./output/nrf52840/bin/ot-cli-ftd-merge.hex"
expect eof

#spawn bash -c "nrfjprog -f nrf52 -s 683397607 --chiperase --program ./output/nrf52840/bin/ot-cli-ftd.hex  --reset"
#expect eof

spawn bash -c "echo \"nrfjprog -f nrf52 -s 683397607 --chiperase --program ./output/nrf52840/bin/ot-cli-ftd-merge.hex  --reset\""
expect eof

#spawn bash -c "nrfjprog -f nrf52 -s 683397607 --chiperase --program ./output/nrf52840/bin/ot-cli-ftd-merge.hex  --reset"
#expect eof

exit
