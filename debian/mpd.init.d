#!/bin/sh

### BEGIN INIT INFO
# Provides:          mpd
# Required-Start:    $local_fs $remote_fs
# Required-Stop:     $local_fs $remote_fs
# Should-Start:      autofs $network $named alsa-utils pulseaudio avahi-daemon
# Should-Stop:       autofs $network $named alsa-utils pulseaudio avahi-daemon
# Default-Start:     2 3 4 5
# Default-Stop:      0 1 6
# Short-Description: Music Player Daemon
# Description:       Start the Music Player Daemon (MPD) service
#                    for network access to the local audio queue.
### END INIT INFO

. /lib/lsb/init-functions

PATH=/sbin:/bin:/usr/sbin:/usr/bin
NAME=mpd
DESC="Music Player Daemon"
DAEMON=/usr/bin/mpd
MPDCONF=/etc/mpd.conf

# Exit if the package is not installed
[ -x "$DAEMON" ] || exit 0

# Read configuration variable file if it is present
[ -r /etc/default/$NAME ] && . /etc/default/$NAME

if [ -n "$MPD_DEBUG" ]; then
    set -x
    MPD_OPTS=--verbose
fi

PIDFILE=$(sed -n 's/^[[:space:]]*pid_file[[:space:]]*"\?\([^"]*\)\"\?/\1/p' $MPDCONF)

mpd_start () {
    log_daemon_msg "Starting $DESC" "$NAME"

    if [ -z "$PIDFILE" ]; then
        log_failure_msg \
            "$MPDCONF must have pid_file set; cannot start daemon."
        exit 1
    fi

    PIDDIR=$(dirname "$PIDFILE")
    if [ ! -d "$PIDDIR" ]; then
        mkdir -m 0755 $PIDDIR
        if dpkg-statoverride --list --quiet /run/mpd > /dev/null; then
            # if dpkg-statoverride is used update it with permissions there
            dpkg-statoverride --force --quiet --update --add $( dpkg-statoverride --list --quiet /run/mpd ) 2> /dev/null
        else
            # use defaults
            chown mpd:audio $PIDDIR
        fi
    fi

    start-stop-daemon --start --quiet --oknodo --pidfile "$PIDFILE" \
        --exec "$DAEMON" -- $MPD_OPTS "$MPDCONF"
    log_end_msg $?
}

mpd_stop () {
    if [ -z "$PIDFILE" ]; then
        log_failure_msg \
            "$MPDCONF must have pid_file set; cannot stop daemon."
        exit 1
    fi

    log_daemon_msg "Stopping $DESC" "$NAME"
    start-stop-daemon --stop --quiet --oknodo --retry 5 --pidfile "$PIDFILE" \
        --exec $DAEMON
    log_end_msg $?
}

# note to self: don't call the non-standard args for this in
# {post,pre}{inst,rm} scripts since users are not forced to upgrade
# /etc/init.d/mpd when mpd is updated
case "$1" in
    start)
        mpd_start
        ;;
    stop)
        mpd_stop
        ;;
    status)
    	status_of_proc -p $PIDFILE $DAEMON $NAME
	;;
    restart|force-reload)
        mpd_stop
        mpd_start
        ;;
    force-start)
        mpd_start
        ;;
    force-restart)
        mpd_stop
        mpd_start
        ;;
    force-reload)
	mpd_stop
	mpd_start
	;;
    *)
        echo "Usage: $0 {start|stop|restart|force-reload}"
        exit 2
        ;;
esac
