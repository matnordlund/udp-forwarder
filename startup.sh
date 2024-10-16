#!/bin/bash

# Startyp Charon and initiate IPsec tunnel
/usr/libexec/ipsec/charon &
echo "Sleeping for 5 seconds to allow charon to load"
sleep 5
/usr/sbin/swanctl --load-all
/usr/sbin/swanctl --initiate -c sase_logging_net

# Create ipsec0 interface and setup routes
/usr/libexec/ipsec/xfrmi --name ipsec0 --id 42 --dev eth0
ip link set ipsec0 up

input_file="/etc/swanctl/swanctl.conf"
local_ts=$(grep -oP '(?<=local_ts =).*' "$input_file")
remote_ts=$(grep -oP '(?<=remote_ts =).*' "$input_file")

ip addr add $local_ts dev ipsec0

# Remove trailing /32 if it exists
local_ts=${local_ts%/32}

ip route add $remote_ts src $local_ts dev ipsec0

# Startup upd_forwarder
/app/udp-forwarder -s -c /data/config.ini
