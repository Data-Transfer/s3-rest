#!/bin/env sh
if [ $# -eq 0 ]; then
    echo "Print usage statistics, redirect 2>&1 when writing to file~"
    echo "usage: $0 <json configuration file>"
    exit 1
fi
./_check_env.sh
CMDLINE="./s3-rest.py -c $0 -t \"usage=\""
echo "print usage information"
echo "$CMDLINE"
sleep 2
$CMDLINE