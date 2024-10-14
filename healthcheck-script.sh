#!/bin/bash

CONNECTION_NAME="sase_logging_net"  # Name of the strongSwan connection to monitor
CHILD_SA_STATUS=$(swanctl --list-sas | grep -A 5 "${CONNECTION_NAME}" | grep "INSTALLED")
    
if [[ -n "$CHILD_SA_STATUS" ]]; then
      echo "$(date): Child SAs for connection '${CONNECTION_NAME}' are UP" 
      exit 0
else
      echo "$(date): ERROR: Child SAs for connection '${CONNECTION_NAME}' are DOWN" 
      exit 1
# Optionally: Restart the connection if the child SA is down
# echo "Restarting connection '${CONNECTION_NAME}'" 
# swanctl --terminate --ike ${CONNECTION_NAME}
# swanctl --initiate --ike ${CONNECTION_NAME}
fi
