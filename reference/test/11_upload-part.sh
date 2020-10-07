#!/bin/env sh
if [ $# -eq 0 ]; then
    echo "Upload part. Pass UploadId returned by 'start upload' request."
    echo "ETag id shall be copied and added to list of uploaded parts in XML file"
    echo "usage: $0 <json configuration file> <bucket name> <oject name> <file name> <part number> <upload id>"
    exit 1
fi
./_check_env.sh
./s3-rest.py  -m put -b $2 -k $3 \
           -c $1 -p $4 -f \
           -t "partNumber=$5;uploadId=$6" -H "ETag"