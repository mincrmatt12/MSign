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
			sh 'python3 get_sizes.py'
			plot csvFileName: 'plot-signcode-stmsize.csv',
				csvSeries: [[file: 'stm.csv', exclusionValues: '', displayTableFlag: false, inclusionFlag: 'OFF', url: '']],
				group: 'SignCode Size',
				title: 'STM',
				style: 'line'
			plot csvFileName: 'plot-signcode-stmsize.csv',
				csvSeries: [[file: 'esp.csv', exclusionValues: '', displayTableFlag: false, inclusionFlag: 'OFF', url: '']],
				group: 'SignCode Size',
				title: 'ESP',
				style: 'line'
		}
	}
    stage('Archive') {
      steps {
        archiveArtifacts(artifacts: '*/.pioenvs/**/firmware.bin', onlyIfSuccessful: true)
      }
    }
  }
}
