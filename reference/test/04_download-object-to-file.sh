#!/bin/env sh
if [ $# -eq 0 ]; then
    echo "Download object to file"
    echo "usage: $0 <json configuration file> <bucket name> <object name>"
    exit 1
fi
./_check_env.sh
CMDLINE="./s3-rest.py -c $1 -m get -b $2 -k $3 -n $4"
echo "downloading object $2/$3 into file $4"
echo "$CMDLINE"
sleep 2
$CMDLINE
