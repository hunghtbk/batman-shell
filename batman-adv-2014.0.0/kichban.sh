#!/bin/sh

### Main radio0 will broadcast one AP with no encryption, another AP with WPA2, 
### and both interfaces will be bridged together with eth0 and bat0
### Another VAP in adhoc mode is added to main radio0, 
### as well as adhoc networks in radio1 and radio2 if they are present.
### All three adhoc networks are added to bat0 and thus managed by batman-adv

### Node-specific settings
export HOSTNAME="meshnodeX" 
export IP="10.x.x.x" 
export WPA_ESSID="$HOSTNAME.wpa" 
export WPA_KEY="password" 

### These parameters should be consistent across all nodes
export NETMASK="255.0.0.0" 
export DNS="" 
export GATEWAY="" 
export PUBLIC_ESSID="3radio.mesh" 
export MESH0_BSSID="CA:CA:CA:CA:CA:00" 
export MESH0_ESSID="mesh0" 
export MESH0_CHANNEL="1" 
export MESH1_MODE="adhoc" 
export MESH1_BSSID="CA:CA:CA:CA:CA:01" 
export MESH1_ESSID="mesh1" 
export MESH1_CHANNEL="11" 
export MESH2_MODE="adhoc" 
export MESH2_BSSID="CA:CA:CA:CA:CA:02" 
export MESH2_ESSID="mesh2" 
export MESH2_CHANNEL="6" 

### Ensure of populating /etc/config/wireless with 
### autodetected wifi-device entries (radioX)
### to get all list_capab and hwmode correct. Otherwise
### OpenWRT might fail to configure the radio properly.
#wifi detect >>/etc/config/wireless

### Clear preexisting wifi-iface sections to avoid conflicts or dups
#( for i in `seq 0 9` ; do echo "delete wireless.@wifi-iface[]" ; done ) | uci batch -q

### Create /etc/config/batman-adv if it's not there yet.
#uci import -m batman-adv </dev/null

echo " 
set system.@system[0].hostname=$HOSTNAME

set batman-adv.bat0=mesh
set batman-adv.bat0.interfaces='mesh0 mesh1 mesh2'

set network.lan.ipaddr=$IP
set network.lan.netmask=$NETMASK
set network.lan.dns='$DNS'
set network.lan.gateway=$GATEWAY
set network.lan.ifname='eth0 bat0'
set network.bat0=interface
set network.bat0.ifname=bat0
set network.bat0.proto=none
set network.bat0.mtu=1500
set network.mesh0=interface
set network.mesh0.proto=none
set network.mesh0.mtu=1528
set network.mesh1=interface
set network.mesh1.proto=none
set network.mesh1.mtu=1528
set network.mesh2=interface
set network.mesh2.proto=none
set network.mesh2.mtu=1528

set wireless.radio0=wifi-device
set wireless.radio0.channel=$MESH0_CHANNEL
set wireless.radio0.disabled=0
set wireless.radio0.phy=phy0
set wireless.radio0.macaddr=

set wireless.radio1=wifi-device
set wireless.radio1.channel=$MESH1_CHANNEL
set wireless.radio1.disabled=0
set wireless.radio1.phy=phy1
set wireless.radio1.macaddr=

set wireless.radio2=wifi-device
set wireless.radio2.channel=$MESH2_CHANNEL
set wireless.radio2.disabled=0
set wireless.radio2.phy=phy2
set wireless.radio2.macaddr=

add wireless wifi-iface
set wireless.@wifi-iface[-1].device=radio0
set wireless.@wifi-iface[-1].encryption=none
set wireless.@wifi-iface[-1].network=lan
set wireless.@wifi-iface[-1].mode=ap
set wireless.@wifi-iface[-1].ssid='$PUBLIC_ESSID'

add wireless wifi-iface
set wireless.@wifi-iface[-1].device=radio0
set wireless.@wifi-iface[-1].encryption=psk2
set wireless.@wifi-iface[-1].key='$WPA_KEY'
set wireless.@wifi-iface[-1].network=lan
set wireless.@wifi-iface[-1].mode=ap
set wireless.@wifi-iface[-1].ssid='$WPA_ESSID'

add wireless wifi-iface
set wireless.@wifi-iface[-1].device=radio0 
set wireless.@wifi-iface[-1].encryption=none
set wireless.@wifi-iface[-1].network=mesh0
set wireless.@wifi-iface[-1].mode=adhoc 
set wireless.@wifi-iface[-1].bssid=$MESH0_BSSID
set wireless.@wifi-iface[-1].ssid='$MESH0_ESSID'
set wireless.@wifi-iface[-1].mcast_rate=11000 

add wireless wifi-iface
set wireless.@wifi-iface[-1].device=radio1 
set wireless.@wifi-iface[-1].encryption=none
set wireless.@wifi-iface[-1].network=mesh1
set wireless.@wifi-iface[-1].mode=$MESH1_MODE
set wireless.@wifi-iface[-1].bssid=$MESH1_BSSID
set wireless.@wifi-iface[-1].ssid='$MESH1_ESSID'
set wireless.@wifi-iface[-1].mcast_rate=11000 

add wireless wifi-iface
set wireless.@wifi-iface[-1].device=radio2 
set wireless.@wifi-iface[-1].encryption=none
set wireless.@wifi-iface[-1].network=mesh2
set wireless.@wifi-iface[-1].mode=$MESH2_MODE
set wireless.@wifi-iface[-1].bssid=$MESH2_BSSID 
set wireless.@wifi-iface[-1].ssid='$MESH2_ESSID'
set wireless.@wifi-iface[-1].mcast_rate=11000 
commit" \
