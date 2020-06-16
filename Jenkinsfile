pipeline {
	agent {
		dockerfile {
			label 'linux && docker'
			args "-u 1001:1001 -v/home/jenkins:/home/jenkins"
		}
	}
	stages {
		stage('Build') {
			parallel {
				stage('Build STM') {
					steps {
						sh 'python -m platformio run -d stm'
						sh 'python -m platformio run -d stmboot'
					}
				}
				stage('Build ESP') {
					steps {
						sh 'python -m platformio run -d esp'
					}
				}
			}
		}
		stage('Log size') {
		  steps {
			sh 'python get_sizes.py'
			plot(csvFileName: 'plot-signcode-stmsize.csv', csvSeries: [[file: 'stm.csv', exclusionValues: '', displayTableFlag: false, inclusionFlag: 'OFF', url: '']], group: 'SignCode Size', title: 'STM', style: 'line')
			plot(csvFileName: 'plot-signcode-espsize.csv', csvSeries: [[file: 'esp.csv', exclusionValues: '', displayTableFlag: false, inclusionFlag: 'OFF', url: '']], group: 'SignCode Size', title: 'ESP', style: 'line')
		  }
		}
		stage('Archive') {
			steps {
				archiveArtifacts(artifacts: '*/.pio/build/**/firmware.bin', onlyIfSuccessful: true)
			}
		}
	}
}
