echo "************************************"
echo "____Lab411-phutvbk@gmail.com________"
echo "Install batman-adv and configure network interface"
echo "__________________________________________________"
#remove batman & del wlan3
sudo rmmod batman-adv
sudo batctl if del wlan3
sleep 2

echo "Insmod batman-adv"
sudo insmod ./batman-adv.ko
#stop Network manager
sudo stop network-manager
#disable firewall
sudo iptables -F
echo "Configure wlanx interface"
sudo ifconfig wlan3 down
sudo iwconfig wlan3 mode ad-hoc	
sudo iwconfig wlan3 essid NC
sudo iwconfig wlan3 key 1234567890
sudo ifconfig wlan3 10.42.43.55/8
sleep 1
sudo iwconfig wlan3 ap 02:CE:FA:DD:CA:00 
sudo iwconfig wlan3 channel 1
sudo ifconfig wlan3 up
sudo ifconfig wlan3 mtu 1532
sleep 1

echo "Configure bat0 interface"
sudo ifconfig wlan3 promisc
sudo modprobe libcrc32c
sudo batctl if add wlan3

sudo ifconfig bat0 up
sudo ifconfig wlan3 up

sudo ifconfig wlan3 0.0.0.0
sudo ifconfig bat0 192.168.123.105/24
echo "Configure is successful"
