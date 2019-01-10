#!/bin/bash

#MPD Volumio Custom recipe compiler script
echo "Removing upnp dep"
sudo apt-get remove -y libupnp6

echo "Installing dependencies"

sudo apt-get update
sudo apt-get install -y build-essential automake autoconf libtool pkg-config libcurl4-openssl-dev intltool libxml2-dev libgtk2.0-dev libnotify-dev libglib2.0-dev libevent-dev dh-make devscripts
sudo apt-get build-dep -y mpd

echo "Creating Package"
debuild -i -us -uc -b -d

echo "Done"
