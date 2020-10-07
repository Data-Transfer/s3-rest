#!/bin/env sh
if [ $# -eq 0 ]; then
    echo "Delete object"
    echo "usage: $0 <json configuration file> <bucket name> <object name>"
    exit 1
fi
./_check_env.sh
CMDLINE="./s3-rest.py -c $1 -m delete -b $2 -k $3"
echo "deleting $2/$3 object"
echo "$CMDLINE"
sleep 2
$CMDLINE
