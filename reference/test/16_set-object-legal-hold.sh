#!/bin/env sh
if [ $# -eq 0 ]; then
    echo "Set object legal hold i.e. lock object"
    echo "usage: $0 <json configuration file> <bucket name> <object name> <xml file>"
    exit 1
fi
./_check_env.sh
CMDLINE="./s3-rest.py -c $1 -m put -b $2 -k $3 -p $4 -f -t legal-hold="
echo "set object $2/$3's legal host status reading from file $4"
echo "$CMDLINE"
sleep 2
$CMDLINE