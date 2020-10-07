#!/bin/env sh
if [ $# -eq 0 ]; then
    echo "Create new bucket"
    echo "usage: $0 <json configuration file> <bucket name>"
    exit 1
fi
./_check_env.sh
CMDLINE="./s3-rest.py -c $1 -m put -b $2"
echo "create bucket $2"
echo "$CMDLINE"
sleep 2
$CMDLINE
