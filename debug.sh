#!/bin/bash

#only first time 
#apt-get update
#apt-get install -y gdb
#sudo apt-get source libc6

echo "Attaching to MPD process"
sudo gdb -p $(pidof mpd)

# or launching manually
# gdb --args mpd --stderr --no-daemon --verbose

# Once crashed, to see the backtrace
# bt
# SEE
# https://wiki.archlinux.org/title/Core_dump


