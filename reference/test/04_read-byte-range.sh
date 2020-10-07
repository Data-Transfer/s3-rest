#!/bin/env sh
if [ $# -eq 0 ]; then
    echot "Read byte range from object"
    echo "usage: $0 <json configuration file> <bucket name> <object name> <start location> <end location>"
    exit 1
fi
./_check_env.sh
CMDLINE="./s3-rest.py -c $1 -m get -b $2 -k $3 -e Range:bytes=$4-$5"
echo "reading byte range [$4, $5] from object $2/$3"
echo "$CMDLINE"
sleep 2
$CMDLINE
