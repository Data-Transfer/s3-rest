#!/bin/env sh
if [ $# -eq 0 ]; then
    echo "Retrieve bucket location"
    echo "usage: $0 <json configuration file> <bucket name>"
    exit 1
fi
./_check_env.sh
CMDLINE="./s3-rest.py -c $1 -b $2 -t location="
echo "print location information of bucket $2"
echo "$CMDLINE"
sleep 2
$CMDLINE