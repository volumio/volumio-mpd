#!/bin/bash

#MPD Volumio Custom recipe compiler script
echo "Installing dependencies"

sudo apt-get update
sudo apt-get install -y build-essential automake autoconf libtool pkg-config libcurl4-openssl-dev intltool libxml2-dev libgtk2.0-dev libnotify-dev libglib2.0-dev libevent-dev dh-make devscripts
sudo apt-get -y remove libupnp6
sudo apt-get -y install libupnp6
sudo apt-get build-dep -y mpd
sudo apt-get -y install libsidplayfp-dev

## Edit debian/rules by removing dh_strip override
rm -rf debian/source

echo "Fixing libraries"
autoreconf -f -i

echo "Creating Package"
debuild -i -us -uc -b -d

#or debuild -us -uc

echo "Done"
