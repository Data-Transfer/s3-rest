
#!/bin/env sh
if [ $# -eq 0 ]; then
    echo "Create public read-only object"
    echo "usage: $0 <json configuration file> <bucket name> <object name>"
    exit 1
fi
./_check_env.sh
CMDLINE="./s3-rest.py -c $1 -b $2 -k $3 -m put -e \"x-amz-grant-read:uri=http://acs.amazonaws.com/groups/global/AllUsers\""
echo "make $2/$3 publicly readable"
echo "$CMDLINE"
sleep 2
$CMDLINE