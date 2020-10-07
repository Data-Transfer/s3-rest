#!/bin/env sh
if [ $# -eq 0 ]; then
    echo "Read object content"
    echo "usage: $0 <json configuration file> <bucket name> <object name>"
    exit 1
fi
./_check_env.sh
CMDLINE="./s3-rest.py -c $1 -m get -b $2 -k $3"
echo "reading from $2/$3 object"
echo "$CMDLINE"
sleep 2
$CMDLINE
