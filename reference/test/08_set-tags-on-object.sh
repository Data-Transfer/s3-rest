#!/bin/env sh
if [ $# -eq 0 ]; then
    echo "Assign tags to existing object"
    echo "usage: $0 <json configuration file> <bucket name> <object name> <xml tags file>"
    exit 1
fi
./_check_env.sh
CMDLINE="./s3-rest.py -c $1 -m put -b $2 -k $3 -p $4 -f -t tagging=" 
echo "setting tags from $4 on object $2/$3"
echo "$CMDLINE"
sleep 2
$CMDLINE