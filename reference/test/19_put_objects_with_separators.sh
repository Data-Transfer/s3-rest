#!/bin/env sh
if [ $# -eq 0 ]; then
    echo "Create two objects with separators '/' (e.g. full posix path)"
    echo "and upload to object store"
    echo "separator string can be set to anything"
    echo "usage: $0 <json configuration file> <bucket name>"
    exit 1
fi
./_check_env.sh
CMDLINE="./s3-rest.py -c $1 -m put -b $2"
$CMDLINE "-f -p 19_root/level1/level2/file12.txt -k 19_root/level1/level2/file12.txt"
$CMDLINE "-f -p 19_root/level1/file1.txt -k 19_root/level1/file1.txt"