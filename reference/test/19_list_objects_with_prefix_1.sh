#!/bin/env sh
if [ $# -eq 0 ]; then
    echo "List objects with specific prefix in bucket"
    echo "usage: $0 <json configuration file> <bucket name>"
    exit 1
fi
./_check_env.sh
CMDLINE="./s3-rest.py -c $1 -m get -b $2 -t prefix=19_root/level1"
echo "list all objects in bucket $2 with prefix 19_root/level1/"
echo "$CMDLINE"
sleep 2
$CMDLINE