# to be included from shell script to verify command to send rest requests
# is accessible
FILE="./s3-rest.py"
if [ ! -f "$FILE" ]; then
    echo "$FILE does not exist, create symlink to s3-rest.py file"
    echo "or copy s3-rest.py file into current directory, also make"
    echo "sure s3v4_rest.py module is in python module search path"
    echo "or make it accessible from current directory through a symlink."
    exit 1
fi