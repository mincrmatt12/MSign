import os

fs = os.popen("{} stm/.pioenvs/nucleo_f207zg/firmware.elf esp/.pioenvs/nodemcuv2/firmware.elf".format(os.path.expanduser("~/.platformio/packages/toolchain-gccarmnoneeabi/bin/arm-none-eabi-size"))).read().split("\n")[1:]

sections = [[int(x) for x in y.split("\t")[:3]] for y in fs[:-1]]

with open("stm.csv", "w") as f:
    f.write('"flash","ram"\n')
    f.write("{},{}".format(sections[0][0] + sections[0][1], sections[0][1] + sections[0][2]))

with open("esp.csv", "w") as f:
    f.write('"flash","ram"\n')
    f.write("{},{}".format(sections[1][0] + sections[1][1], sections[1][1] + sections[1][2]))
