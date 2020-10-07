#!/bin/env sh
if [ $# -eq 0 ]; then
    echo "Create appendable object and append data to it"
    echo "location of next append operation is returned"
    echo "usage: $0 <json configuration file> <bucket name> <object name> <content> <location>"
    exit 1
fi
./_check_env.sh
POS="-t append=;position=$5"
SHOW_NEXT_POS="-H x-rgw-next-append-position"
CMDLINE="./s3-rest.py -c $1 -m put -b $2 -k $3 -p $4 $POS $SHOW_NEXT_POS"
echo "append \"$4\" to $2/$3 object at position $5"
echo "$CMDLINE"
sleep 2
$CMDLINE
