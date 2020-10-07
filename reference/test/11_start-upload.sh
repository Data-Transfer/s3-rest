#!/bin/env sh
if [ $# -eq 0 ]; then
    echo "Start upload: send request and extract UploadId field in from returned XML response"
    echo "UploadId shall be passed to all subsequent upload requests"
    echo "usage: $0 <json configuration file> <bucket name> <oject name>"
    exit 1
fi
./_check_env.sh
./s3-rest.py -c $1 -b $2 -k $3 -m post -t"uploads=" -X .//aws:UploadId