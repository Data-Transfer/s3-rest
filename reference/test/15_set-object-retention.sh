#!/bin/env sh
if [ $# -eq 0 ]; then
    echo "Set object expiration date"
    echo "usage: $0 <json configuration file> <bucket name> <object name> <xml file>"
    exit 1
fi
./_check_env.sh
CMDLINE="./s3-rest.py -c $1 -m put -b $2 -k $3 -p $4 -f -t retention="
echo "set expiration date for $2/$3 reading from $4"
echo "$CMDLINE"
sleep 2
$CMDLINE