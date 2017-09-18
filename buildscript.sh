#!/bin/bash

#MPD Volumio Custom recipe compiler script
echo "Installing dependencies"

apt-get update
apt-get install -y build-essential automake autoconf libtool pkg-config libcurl4-openssl-dev intltool libxml2-dev libgtk2.0-dev libnotify-dev libglib2.0-dev libevent-dev dh-make devscripts
apt-get -y remove libupnp6
apt-get -y install libupnp6
apt-get build-dep -y mpd
apt-get -y install libsidplayfp-dev 

## Edit debian/rules by removing dh_strip override

echo "Creating Package"
debuild -i -us -uc -b

echo "Done"

