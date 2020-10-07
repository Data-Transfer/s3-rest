#!/bin/env sh
if [ $# -eq 0 ]; then
    echo "Finish multipart upload operation: pass filename of xml file containing list of uploaded parts (ETags)"
    echo "usage: $0 <json configuration file> <bucket name> <oject name> <upload id> <xml file>"
    exit 1
fi
./_check_env.sh
./s3-rest.py -c $1 -b $2 -k $3 -m post -t "uploadId=$4" -p $5 -f