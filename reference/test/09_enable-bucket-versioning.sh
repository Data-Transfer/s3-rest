#!/bin/env sh
if [ $# -eq 0 ]; then
    echo "Enable versioning on bucket, parameters read from XML file"
    echo "usage: $0 <json configuration file> <bucket name>"
    exit 1
fi
./_check_env.sh
VERSION_REQ="09_versioning-request-body.xml"
CMDLINE="./s3-rest.py -c $1 -m put -b $2 -p $VERSION_REQ -f -t versioning="
echo "create bucket $2"
echo "$CMDLINE"
sleep 2
$CMDLINE