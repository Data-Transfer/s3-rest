#!/bin/env sh
if [ $# -eq 0 ]; then
    echo "Delete versioned object"
    echo "Retrieve object's versionId through list-objects-with-version script first"
    echo "usage: $0 <json configuration file> <bucket name> <key name> <versionId>"
    exit 1
fi
./_check_env.sh
CMDLINE="./s3-rest.py -c $1 -m delete -b $2 -k $3 -t versionId=$4"
echo "delete version $4 of object $2/$3"
echo "$CMDLINE"
sleep 2
$CMDLINE