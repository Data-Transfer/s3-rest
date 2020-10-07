#!/bin/env sh
if [ $# -eq 0 ]; then
    echo "Delete bucket"
    echo "usage: $0 <json configuration file> <bucket name>"
    exit 1
fi
./_check_env.sh
CMDLINE="./s3-rest.py -c $1 -m delete -b $2 -l DEBUG"
echo "delete bucket $2"
echo "$CMDLINE"
sleep 2
$CMDLINE
