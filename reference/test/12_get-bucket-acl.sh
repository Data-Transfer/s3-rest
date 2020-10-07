#!/bin/env sh
if [ $# -eq 0 ]; then
    echo "Retrieve bucket ACL"
    echo "usage: $0 <json configuration file> <bucket name>"
    exit 1
fi
./_check_env.sh
CMDLINE="./s3-rest.py -c $1 -b $2 -t acl="
echo "retrieven ACL for bucket $2"
echo "$CMDLINE"
sleep 2
$CMDLINE