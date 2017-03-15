#!/bin/bash

echo "This script is designed to install the lasershark_hostapp suite on a Raspberry Pi running Raspbian."
echo "It may work on other distros as well, but buyer beware!"

sudo apt-get update
sudo apt-get upgrade
sudo apt-get install gcc build-essential libusb-1.0-0-dev -y

git submodule init
git submodule update

make lasershark_stdin
make lasershark_stdin_displayimage
make lasershark_stdin_circlemaker
make lasershark_stdin_gridmaker

echo "Installation is complete!"
