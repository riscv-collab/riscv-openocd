#!/bin/bash


function process_report {
  input=$1
  build_number=$(xmlstarlet sel -t -m "/workflowRun/number" -v . "$input")
  build_result=$(xmlstarlet sel -t -m "/workflowRun/result" -v . "$input")
  build_timestamp_ms=$(xmlstarlet sel -t -m "/workflowRun/timestamp" -v . "$input")
  build_timestamp=$(echo "$build_timestamp_ms" | rev | cut -c4- | rev)

  echo "build_number: ${build_number}"
  echo "build_result: ${build_result}"
  echo "build_timestamp_ms: ${build_timestamp_ms}"
  echo "build_timestamp: ${build_timestamp}"
  echo "build_date: $(date -d @${build_timestamp})"

  if [ "$build_result" = "SUCCESS" ]; then
    echo "great success!"
    return 0
  else
    echo "miserable failure"
    return 1
  fi
}

RESULT=0
for report in *.xml; do
  echo ""
  echo "---processing $report"
  process_report $report || RESULT=1
done

exit $RESULT
