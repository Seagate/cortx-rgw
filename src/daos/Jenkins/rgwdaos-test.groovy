// This file must be manually inserted into the pipeline section of the jenkins configuration
// https://eos-jenkins.colo.seagate.com/job/Release_Engineering/job/re-workspace/job/sv_space/job/rgwdaos-test/configure
pipeline {
    agent {
        node {
            label "rgwdaos-node"
        }
    }

    stages {
        stage ('Build docker images, run s3-tests') {
            steps {
                script { build_stage = env.STAGE_NAME }
                sh label: 'Run DAOS-CORTX S3-Tests', script: '''
					echo $HOSTNAME
					docker images
					export RUN_DATE="$(date +"%Y-%m-%d")"
					export DOCKER_RUN=/opt/ceph/src/daos/docker
                    export ARTIFACTS_FOLDER=/opt/$RUN_DATE
                    export CEPH_PATH=/opt/ceph
                    export DAOS_PATH=/opt/daos
                    export S3TESTS_PATH=/opt/s3-tests
                    export LOCAL_ARTIFACTS=/opt/$RUN_DATE
					cd $DOCKER_RUN && sh test.sh --artifacts-folder=$ARTIFACTS_FOLDER --local-artifacts=$LOCAL_ARTIFACTS
					ls -l $ARTIFACTS_FOLDER/*.csv
					cp $LOCAL_ARTIFACTS/test_diff.csv $WORKSPACE
					cp $LOCAL_ARTIFACTS/test_output.csv $WORKSPACE
					cp $LOCAL_ARTIFACTS/test_summary.csv $WORKSPACE
                '''
                // archiveArtifacts artifacts: "*.csv", onlyIfSuccessful: false, allowEmptyArchive: true 
            }
        }
    }

    post {
        cleanup {
            sh label: 'Collect Artifacts', script: '''
				# reboot the node in 30 seconds, hugepages need to be cleared between runs
				(sudo bash -c "(sleep 30 && sudo shutdown -r now) &") &
            '''
            script {
                // Archive Deployment artifacts in jenkins build
                // writeFile(file: '$WORKSPACE/results.html', text: ${SCRIPT, template="cluster-setup-email.template"})
                archiveArtifacts artifacts: "*"
            }
        }
        always { 
            script {
                // Email Notification
                def recipientProvidersClass = [[$class: 'RequesterRecipientProvider']]
                mailRecipients = "seagate-daos@seagate.com"
                emailext ( 
                    body: '''${SCRIPT, template="cluster-setup-email.template"}''',
                    mimeType: 'text/html',
                    subject: "[Jenkins Build ${currentBuild.currentResult}] : ${env.JOB_NAME}",
                    attachLog: false,
                    to: "${mailRecipients}",
                    recipientProviders: recipientProvidersClass
                )
            }
        }
    }
}
