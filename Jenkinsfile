pipeline {
  agent {
    node {
      label 'scala'
    }

  }
  stages {
    stage('Build') {
      parallel {
        stage('Build STM') {
          steps {
            sh 'python2 -m platformio run -d stm'
			script {
				var espsize = sh(returnStdout: true, script: 'arm-none-eabi-size esp/.pioenvs/nodemcuv2/firmware.elf').readLines()[1].split('\t')
				var flash = espsize[0] as Integer + espsize[1] as Integer
				var ram = espsize[1] as Integer + espsize[2] as Integer

				File out = new File("esp.csv")
				out << '"flash","ram"\n'
				out << sprintf('%d,%d\n', [flash, ram])
			}
			plot csvFileName: 'plot-signcode-espsize.csv',
				csvSeries: [[file: 'esp.csv', exclusionValues: '', displayTableFlag: false, inclusionFlag: 'OFF', url: '']],
				group: 'SignCode Size',
				title: 'ESP',
				style: 'line'
          }
        }
        stage('Build ESP') {
          steps {
            sh 'python2 -m platformio run -d esp'
			script {
				var stmsize = sh(returnStdout: true, script: 'arm-none-eabi-size stm/.pioenvs/nucleo_f207zg/firmware.elf').readLines()[1].split('\t')
				var flash = stmsize[0] as Integer + stmsize[1] as Integer
				var ram = stmsize[1] as Integer + stmsize[2] as Integer

				File out = new File("stm.csv")
				out << '"flash","ram"\n'
				out << sprintf('%d,%d\n', [flash, ram])
			}
			plot csvFileName: 'plot-signcode-stmsize.csv',
				csvSeries: [[file: 'stm.csv', exclusionValues: '', displayTableFlag: false, inclusionFlag: 'OFF', url: '']],
				group: 'SignCode Size',
				title: 'STM',
				style: 'line'
          }
        }
      }
    }
    stage('Archive') {
      steps {
        archiveArtifacts(artifacts: '*/.pioenvs/**/firmware.bin', onlyIfSuccessful: true)
      }
    }
  }
}
