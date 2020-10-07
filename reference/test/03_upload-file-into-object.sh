#!/bin/env sh
if [ $# -eq 0 ]; then
    echo "Copy file content into object"
    echo "usage: $0 <json configuration file> <bucket name> <object name> <file name>"
    exit 1
fi
./_check_env.sh
CMDLINE="./s3-rest.py -c $1 -m put -b $2 -k $3 -p $4 -f"
echo "put file $4 into $2/$3 object"
echo "$CMDLINE"
sleep 2
$CMDLINE