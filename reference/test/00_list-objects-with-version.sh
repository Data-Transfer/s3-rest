#!/bin/env sh
if [ $# -eq 0 ]; then
    echo "List objects with version info"
    echo "usage: $0 <json configuration file> <bucket name>"
    exit 1
fi
./_check_env.sh
CMDLINE="./s3-rest.py -c $1 -m get -b $2 -t versions="
echo "list objects with version info in bucket $2"
echo "$CMDLINE"
sleep 2
$CMDLINE
