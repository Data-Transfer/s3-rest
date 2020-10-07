#!/bin/env sh
if [ $# -eq 0 ]; then
    echo "Create new object and store data into it"
    echo "usage: $0 <json configuration file> <bucket name> <object name> <text>"
    exit 1
fi
./_check_env.sh
CMDLINE="./s3-rest.py -c $1 -m put -b $2 -k $3 -p \"$4\""
echo "put \"$4\" into $2/$3 object"
echo "$CMDLINE"
sleep 2
$CMDLINE
