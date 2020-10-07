#!/bin/env sh
if [ $# -eq 0 ]; then
    echo "Copy object into other bucket"
    echo "usage: $0 <json configuration file> <source bucket> <source object> <destination bucket> <destination object>"
    exit 1
fi
./_check_env.sh
CP="\"x-amz-copy-source:/$2/$3\""
CMDLINE="./s3-rest.py -c $1 -m put -b $4 -k $5 -e $CP"
echo "copy object $2/$3 object into bucket $4 as $5"
echo "$CMDLINE"
sleep 2
$CMDLINE