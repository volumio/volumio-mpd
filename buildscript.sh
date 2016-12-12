#!/bin/bash

#MPD Volumio Custom recipe compiler script
echo "Installing dependencies"

apt-get update
apt-get install -y build-essential automake autoconf libtool pkg-config libcurl4-openssl-dev intltool libxml2-dev libgtk2.0-dev libnotify-dev libglib2.0-dev libevent-dev dh-make devscripts
apt-get -y remove libupnp6
apt-get -y install libupnp6
apt-get build-dep -y mpd

echo "Prepare"
./autogen.sh

echo "Configure"
./configure --disable-pulse --disable-roar --disable-oss --disable-openal --disable-wildmidi --disable-sidplay --disable-sndio --disable-haiku --disable-recorder-output --disable-gme 

echo "Compile"
make

echo "install"
sudo make install

echo "Done"

