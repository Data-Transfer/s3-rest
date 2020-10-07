#!/bin/env sh
if [ $# -eq 0 ]; then
    echo "List all buckets"
    echo "usage: $0 <json configuration file>"
    exit 1
fi
./_check_env.sh
CMDLINE="./s3-rest.py -c $1 -l DEBUG"
echo "List buckets"
echo "$CMDLINE"
sleep 2
$CMDLINE
