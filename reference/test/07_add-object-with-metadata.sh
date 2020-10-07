#!/bin/env sh
if [ $# -eq 0 ]; then
    echo "Add object with meta-data"
    echo "usage: $0 <json configuration file> <bucket name> <object> <meta key> <meta value>"
    exit 1
fi
./_check_env.sh
META="x-amz-meta-$4:$5"
CMDLINE="./s3-rest.py -c $1 -m put -b $2 -k $3 -e \"$META\""
echo "creating object $2/$3 with metadata $4=$5"
echo "$CMDLINE"
sleep 2
$CMDLINE