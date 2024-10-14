#!/bin/bash

CONNECTION_NAME="sase_logging_net"  # Name of the strongSwan connection to monitor
IKE_NAME="sase_logging_tunnel" # Name of the strongSwan IKE 

CHILD_SA_STATUS=$(swanctl --list-sas | grep -A 5 "${CONNECTION_NAME}" | grep "INSTALLED")
    
if [[ -n "$CHILD_SA_STATUS" ]]; then
    echo "$(date): Child SAs for connection '${CONNECTION_NAME}' are UP" >&1  # Log to stdout for Docker logs
    exit 0
else
    echo "$(date): ERROR: Child SAs for connection '${CONNECTION_NAME}' are DOWN" >&1  # Log to stdout for Docker logs
    echo "$(date): Trying to restart the connection '${IKE_NAME}'" >&1
    #swanctl --terminate --ike ${IKE_NAME}
    #swanctl --initiate --ike ${IKE_NAME}
    /usr/bin/pkill charon
    /usr/libexec/ipsec/charon &
    sleep 5
    /usr/sbin/swanctl --load-all
    /usr/sbin/swanctl --initiate -c ${CONNECTION_NAME}
    exit 1       
fi
