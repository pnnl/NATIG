#!/bin/bash
case ${PR_STATUS_REPORT} in
    *Succeeded*)
        BUILD_MESSAGE=":tada: **helics-ns3** integration test passed: [[build log]](https://dev.azure.com/HELICS-test/helics-ns3/_build/results?buildId=${BUILD_BUILDID}) [[commit]](https://github.com/GMLC-TDC/HELICS/commit/${HELICS_COMMITISH})"
    ;;
    *Failed*)
        BUILD_MESSAGE=":confused: **helics-ns3** integration test had some problems: [[build log]](https://dev.azure.com/HELICS-test/helics-ns3/_build/results?buildId=${BUILD_BUILDID})  [[commit]](https://github.com/GMLC-TDC/HELICS/commit/${HELICS_COMMITISH})"
    ;;
esac

# Report build status to PR
if [[ "${BUILD_MESSAGE}" != "" ]]; then
    echo "Reporting build status $PR_STATUS_REPORT to github.com/${HELICS_PR_SLUG}/issues/${HELICS_PR_NUM}"
    body='{"body": "'${BUILD_MESSAGE}'"}'
    curl -s -X POST \
        -H "User-Agent: HELICS-bot" \
        -H "Content-Type: application/json" \
        -H "Accept: application/json" \
        -H "Authorization: token ${HELICSBOT_GH_TOKEN}" \
        -d "$body" \
        https://api.github.com/repos/${HELICS_PR_SLUG}/issues/${HELICS_PR_NUM}/comments
fi
