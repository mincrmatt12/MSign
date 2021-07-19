// TODO: build website
pipeline {
	agent {
		dockerfile {
			label 'linux && docker'
			args "-u 1001:1001"
			filename 'Dockerfile.build'
		}
	}
	stages {
		stage ("Build") {
			parallel {
				stage("Build STM") {
					steps {
						// build for board
						dir("stm/build_board") {
							sh "cmake .. -GNinja -DCMAKE_TOOLCHAIN_FILE=../toolchain.cmake -DMSIGN_BUILD_TYPE=board"
							sh "ninja"
						}
						// build for nucleo
						dir("stm/build_nucleo") {
							sh "cmake .. -GNinja -DCMAKE_TOOLCHAIN_FILE=../toolchain.cmake -DMSIGN_BUILD_TYPE=nucleo"
							sh "ninja"
						}

						// archive
						archiveArtifacts artifacts: 'stm/build_*/stm.bin', fingerprint: true
					}
				}
				stage("Build ESP") {
					steps {
						dir("vendor/ESP8266_RTOS_SDK") {
							sh "git apply ../ESP8266_RTOS_SDK.patch"
						}
						dir("esp/build") {
							sh ". ../idf_export.sh; cmake .. -GNinja; ninja"
						}

						archiveArtifacts artifacts: 'esp/build/msign-esp.bin', fingerprint: true
					}
				}
				stage("Build STMboot") {
					steps {
						// build for board
						dir("stmboot/build_board") {
							sh "cmake .. -GNinja -DCMAKE_TOOLCHAIN_FILE=../toolchain.cmake -DMSIGN_BUILD_TYPE=board"
							sh "ninja"
						}
						// build for nucleo
						dir("stmboot/build_nucleo") {
							sh "cmake .. -GNinja -DCMAKE_TOOLCHAIN_FILE=../toolchain.cmake -DMSIGN_BUILD_TYPE=nucleo"
							sh "ninja"
						}

						// archive
						archiveArtifacts artifacts: 'stmboot/build_*/stmboot.bin', fingerprint: true
					}
				}
			}
		}
		stage ("Plots") {
			steps {
				writeFile file: "get_size.py", text: '''
from elftools.elf.elffile import ELFFile

cfg = {
    "stm/build_board/stm": (
        "stm.csv",
        [".rodata", ".isr_vector", ".text", ".data", ".crash_data", ".init_array", ".fw_dbg"],
        [".bss", ".data", ".vram"]
    ),
    "esp/build/msign-esp.elf": (
        "esp.csv",
        [".iram0.vectors", ".iram0.text", ".dram0.data", ".flash.text", ".flash.rodata"],
        [".iram0.bss", ".iram0.text", ".dram0.data", ".dram0.bss", ".iram0.vectors"]
    )
}

for fname, (csvname, flashs, rams) in cfg.items():
    f = open(fname, "rb")
    ef = ELFFile(f)
    with open(csvname, "w") as fout:
        fout.write('"flash","ram"\\n')
        fout.write("{},{}".format(
            sum(ef.get_section_by_name(x).data_size for x in flashs),
            sum(ef.get_section_by_name(x).data_size for x in rams)
        ))
    f.close()
'''
				sh 'python3 get_size.py'
				plot(csvFileName: 'plot-signcode-stmsize.csv', csvSeries: [[file: 'stm.csv', exclusionValues: '', displayTableFlag: false, inclusionFlag: 'OFF', url: '']], group: 'SignCode Size', title: 'STM', style: 'area')
				plot(csvFileName: 'plot-signcode-espsize.csv', csvSeries: [[file: 'esp.csv', exclusionValues: '', displayTableFlag: false, inclusionFlag: 'OFF', url: '']], group: 'SignCode Size', title: 'ESP', style: 'area')
			}
		}
	}
}
