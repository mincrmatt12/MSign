pipeline {
	agent {
		dockerfile {
			label 'docker && linux'
		}
	}
	stages {
		stage('Notify') {
			agent any
				steps {
					discordSend description: env.JOB_NAME + '#' + env.BUILD_NUMBER + ' has started.', link: env.BUILD_URL, title: 'Jenkins', footer: 'Jenkins Report', webhookURL: 'https://discordapp.com/api/webhooks/464976908656836608/xZi9NW1GE3PeRwkxgBJHdb5ZKdzbpJqcm7GP5TtfcRvXZd3O1NDThUJG3aY2cafXshzy'
				}
		}
		stage('Build') {
			parallel {
				stage('Build STM') {
					steps {
						sh 'python2 -m platformio run -d stm'
					}
				}
				stage('Build ESP') {
					steps {
						sh 'python2 -m platformio run -d esp'
					}
				}
			}
		}
		stage('Log size') {
		  steps {
			sh 'python3.6 get_sizes.py'
			plot(csvFileName: 'plot-signcode-stmsize.csv', csvSeries: [[file: 'stm.csv', exclusionValues: '', displayTableFlag: false, inclusionFlag: 'OFF', url: '']], group: 'SignCode Size', title: 'STM', style: 'line')
			plot(csvFileName: 'plot-signcode-espsize.csv', csvSeries: [[file: 'esp.csv', exclusionValues: '', displayTableFlag: false, inclusionFlag: 'OFF', url: '']], group: 'SignCode Size', title: 'ESP', style: 'line')
		  }
		}
		stage('Archive') {
			steps {
				archiveArtifacts(artifacts: '*/.pioenvs/**/firmware.bin', onlyIfSuccessful: true)
			}
		}
	}
	post {
		always {
			discordSend description: env.JOB_NAME + '#' + env.BUILD_NUMBER + ' has ' + (currentBuild.resultIsBetterOrEqualTo('SUCCESS') ? 'succeeded.' : (currentBuild.resultIsBetterOrEqualTo('UNSTABLE') ? 'been marked unstable.' : 'failed!')), link: env.BUILD_URL, title: 'Jenkins',successful: currentBuild.resultIsBetterOrEqualTo('SUCCESS'), footer: 'Jenkins Report', webhookURL: 'https://discordapp.com/api/webhooks/464976908656836608/xZi9NW1GE3PeRwkxgBJHdb5ZKdzbpJqcm7GP5TtfcRvXZd3O1NDThUJG3aY2cafXshzy'
		}
	}
}
