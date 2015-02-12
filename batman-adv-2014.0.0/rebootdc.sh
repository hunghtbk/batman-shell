sudo make clean
sudo make CONFIG_BATMAN_ADV_DEBUG=y
sudo make install
sudo rmmod batman_adv
sudo modprobe batman-adv
