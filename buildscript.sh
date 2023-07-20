#!/bin/bash

#MPD Volumio Custom recipe compiler script
echo "Remember to uncomment sources line in /etc/apt/sources.list"
echo "For PI remove /etc/apt/preferences.d/raspberrypi-kernel"

echo "Installing dependencies"

sudo apt-get update
sudo apt-get install -y build-essential devscripts debhelper dh-make cmake libfmt-dev libboost-dev libpcre2-dev
sudo apt-get build-dep -y mpd

sudo apt-get install -y python3-pip
sudo pip3 install meson

echo "Creating Package"
sudo dh binary-arch

echo "Done"
