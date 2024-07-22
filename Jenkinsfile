pipeline {
	agent {
		dockerfile {
			label 'linux && docker'
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
							sh "cmake .. -GNinja -DCMAKE_TOOLCHAIN_FILE=../toolchain.cmake -DMSIGN_BUILD_TYPE=board -DCMAKE_BUILD_TYPE=Release"
							sh "ninja"
						}
						// build for nucleo
						dir("stm/build_nucleo") {
							sh "cmake .. -GNinja -DCMAKE_TOOLCHAIN_FILE=../toolchain.cmake -DMSIGN_BUILD_TYPE=nucleo -DCMAKE_BUILD_TYPE=Release"
							sh "ninja"
						}
						// build for minisign
						dir("stm/build_minisign") {
							sh "cmake .. -GNinja -DCMAKE_TOOLCHAIN_FILE=../toolchain.cmake -DMSIGN_BUILD_TYPE=minisign -DCMAKE_BUILD_TYPE=Release"
							sh "ninja"
						}

						// archive
						archiveArtifacts artifacts: 'stm/build_*/stm.bin', fingerprint: true
						// also archive the elf for debugging
						archiveArtifacts artifacts: 'stm/build_*/stm'
					}
				}
				stage("Build ESP") {
					steps {
						dir("vendor") {
							sh 'quilt push -a'
						}
						dir("esp/build") {
							sh ". ../idf_export.sh; cmake .. -GNinja; ninja"
						}

						archiveArtifacts artifacts: 'esp/build/msign-esp.bin', fingerprint: true
						archiveArtifacts artifacts: 'esp/build/msign-esp.elf'
					}
				}
				stage("Build STMboot") {
					steps {
						// build for board
						dir("stmboot/build_board") {
							sh "cmake .. -GNinja -DCMAKE_TOOLCHAIN_FILE=../toolchain.cmake -DMSIGN_BUILD_TYPE=board -DCMAKE_BUILD_TYPE=Release"
							sh "ninja"
						}
						// build for nucleo
						dir("stmboot/build_nucleo") {
							sh "cmake .. -GNinja -DCMAKE_TOOLCHAIN_FILE=../toolchain.cmake -DMSIGN_BUILD_TYPE=nucleo -DCMAKE_BUILD_TYPE=Release"
							sh "ninja"
						}
						// build for minisign
						dir("stmboot/build_minisign") {
							sh "cmake .. -GNinja -DCMAKE_TOOLCHAIN_FILE=../toolchain.cmake -DMSIGN_BUILD_TYPE=minisign -DCMAKE_BUILD_TYPE=Release"
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
    "stm/build_minisign/stm": (
        "stm.csv",
        [".rodata", ".isr_vector", ".text", ".data", ".crash_data", ".init_array", ".fw_dbg", ".ram_func"],
        [".bss", ".data", ".vram", ".ram_func", ".ram_isr_vector"]
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
        flash_sz = sum(ef.get_section_by_name(x).data_size for x in flashs)
        ram_sz = sum(ef.get_section_by_name(x).data_size for x in rams)
        fout.write("{},{}".format(
            flash_sz,
            ram_sz
        ))

        print("{}: flash: {}, ram: {}".format(fname, flash_sz, ram_sz))
    f.close()
'''
				sh 'python3 get_size.py'
				plot(csvFileName: 'plot-signcode-stmsize.csv', csvSeries: [[file: 'stm.csv', exclusionValues: '', displayTableFlag: false, inclusionFlag: 'OFF', url: '']], group: 'SignCode Size', title: 'STM', style: 'area')
				plot(csvFileName: 'plot-signcode-espsize.csv', csvSeries: [[file: 'esp.csv', exclusionValues: '', displayTableFlag: false, inclusionFlag: 'OFF', url: '']], group: 'SignCode Size', title: 'ESP', style: 'area')
			}
		}
	}
}
