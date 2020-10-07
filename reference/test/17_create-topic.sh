#!/bin/env sh
if [ $# -eq 0 ]; then
    echo "Create topic and notification endpoint (http://, kafka://...)"
    echo "save content of returned <TopicArn>arn:aws...</TopicArn> tag"
    echo "for setting up notifications"
    echo "usage: $0 <json configuration file> <topic name> <push endpoint> <opaque data>"
    exit 1
fi
./_check_env.sh
CMDLINE="./s3-rest.py -c $1 -m post -t \"Action=CreateTopic;$2;OpaqueData=$4;push-endpoint=$3\" -X .//sns:TopicArn"
echo "create topic $2 with endpoint $3 and opaque data $4"
echo "$CMDLINE"
sleep 2
$CMDLINE

#example: ./s3-rest.py -c credentials.json -m post -t \
#"Action=CreateTopic;Name=ObjectCreated3;OpaqueData=data3;push-endpoint=http://146.118.66.215"

#DO NOT USE THE FOLLOWING FORMAT!
# [&Attributes.entry.1.key=amqp-exchange&Attributes.entry.1.value=<exchange>]
# [&Attributes.entry.2.key=amqp-ack-level&Attributes.entry.2.value=none|broker|routable]
# [&Attributes.entry.3.key=verify-ssl&Attributes.entry.3.value=true|false]
# [&Attributes.entry.4.key=kafka-ack-level&Attributes.entry.4.value=none|broker]
# [&Attributes.entry.5.key=use-ssl&Attributes.entry.5.value=true|false]
# [&Attributes.entry.6.key=ca-location&Attributes.entry.6.value=<file path>]
# [&Attributes.entry.7.key=OpaqueData&Attributes.entry.7.value=<opaque data>]
# [&Attributes.entry.8.key=push-endpoint&Attributes.entry.8.value=<endpoint>]
# [&Attributes.entry.9.key=persistent&Attributes.entry.9.value=true|false]

#USE: <param key>=<value> directly
#PUT OpaqueData *before* the other parameters