#!/bin/env sh
if [ $# -eq 0 ]; then
    echo "Create notification for all events on bucket"
    echo "pass 'arn' URI returned from CreateTopic request"
    echo "request parameters read from xml file"
    echo "usage: $0 <json configuration file> <bucket> <xml file>"
    exit 1
fi
./_check_env.sh
CMDLINE="./s3-rest.py -c $1 -m put -b $2 -p $3 -f -t notification="
echo "create notification on bucket $2"
echo "$CMDLINE"
sleep 2
$CMDLINE