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
    stage('Archive') {
      steps {
        archiveArtifacts(artifacts: '*/.pioenvs/**/firmware.bin', onlyIfSuccessful: true)
      }
    }
  }
}