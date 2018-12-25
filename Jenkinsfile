pipeline {
  agent {
    node {
      label 'scala'
    }

  }
  stages {
    stage('Build STM') {
      parallel {
        stage('Build STM') {
          steps {
            sh 'pio run -d stm'
          }
        }
        stage('Build ESP') {
          steps {
            sh 'pio run -d esp'
          }
        }
      }
    }
    stage('Archive') {
      steps {
        archiveArtifacts(artifacts: '*/.pioenvs/**/firmware.elf', onlyIfSuccessful: true)
      }
    }
  }
}