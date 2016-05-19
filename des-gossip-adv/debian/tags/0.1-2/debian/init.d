#! /bin/sh
### BEGIN INIT INFO
# Provides:          des-gossip-adv
# Required-Start:    $remote_fs $syslog
# Required-Stop:     $remote_fs $syslog
# Default-Start:     
# Default-Stop:      0 1 6
# Short-Description: Start advanced gossip daemon
# Description:       Start a gossip routing daemon (advanced version)
### END INIT INFO

PATH=/usr/local/sbin:/usr/local/bin:/sbin:/bin:/usr/sbin:/usr/bin
DAEMON=/usr/sbin/des-gossip-adv
NAME=des-gossip-adv
DESC="Gossip Routing Daemon"

test -x $DAEMON || exit 0

LOGDIR=/var/log/des-gossip-adv
PIDFILE=/var/run/$NAME.pid	# Does not do anything, currently always /var/run/des-gossip-adv.pid
DODTIME=1                   # Time to wait for the server to die, in seconds
                            # If this value is set too low you might not
                            # let some servers to die gracefully and
                            # 'restart' will not work

# Include des-gossip-adv defaults if available
if [ -f /etc/default/des-gossip-adv ] ; then
    . /etc/default/des-gossip-adv
fi

set -e

running_pid()
{
    # Check if a given process pid's cmdline matches a given name
    pid=$1
    name=$2
    [ -z "$pid" ] && echo "null pid" && return 1
    [ ! -d /proc/$pid ] && echo "no proc enty" &&  return 1
    cmd=`cat /proc/$pid/cmdline | tr "\000" "\n" | head -n 1 | cut -d : -f 1`
    # Is this the expected child?
    [ "$cmd" != "$name" ] && echo "no match:" "$cmd" "$name" &&  return 1
    return 0
}

running()
{
# Check if the process is running looking at /proc
# (works for all users)

    # No pidfile, probably no daemon present
    [ ! -f "$PIDFILE" ] && echo "no pidfile" && return 1
    # Obtain the pid and check it against the binary name
    pid=`cat $PIDFILE`
    running_pid $pid $DAEMON || return 1
    return 0
}

force_stop() {
# Forcefully kill the process
    [ ! -f "$PIDFILE" ] && return
    if running ; then
        kill -15 $pid
        # Is it really dead?
        [ -n "$DODTIME" ] && sleep "$DODTIME"s
        if running ; then
            kill -9 $pid
            [ -n "$DODTIME" ] && sleep "$DODTIME"s
            if running ; then
                echo "Cannot kill $LABEL (pid=$pid)!"
                exit 1
            fi
        fi
    fi
    rm -f $PIDFILE
    return 0
}

configure_tap() {
	echo "configuring tap"
	dessert_up $TAP $PORT
}

case "$1" in
  start)
        echo -n "Starting $DESC: "
        start-stop-daemon --start --quiet --pidfile $PIDFILE \
            --exec $DAEMON -- $DAEMON_OPTS
        sleep 2
        if running ; then
            echo "$NAME."
            configure_tap
        else
            echo " ERROR."
        fi
        ;;
  stop)
        echo -n "Stopping $DESC: "
        start-stop-daemon --stop --quiet --pidfile $PIDFILE \
            --exec $DAEMON
        echo "$NAME."
        ;;
  force-stop)
        echo -n "Forcefully stopping $DESC: "
        force_stop
        if ! running ; then
            echo "$NAME."
        else
            echo " ERROR."
        fi
        ;;
  #reload)
        #
        # If the daemon can reload its config files on the fly
        # for example by sending it SIGHUP, do it here.
        #
        # If the daemon responds to changes in its config file
        # directly anyway, make this a do-nothing entry.
        #
        # echo "Reloading $DESC configuration files."
        # start-stop-daemon --stop --signal 1 --quiet --pidfile \
        #       /var/run/$NAME.pid --exec $DAEMON
  #;;
  force-reload)
        #
        # If the "reload" option is implemented, move the "force-reload"
        # option to the "reload" entry above. If not, "force-reload" is
        # just the same as "restart" except that it does nothing if the
        # daemon isn't already running.
        # check wether $DAEMON is running. If so, restart
        start-stop-daemon --stop --test --quiet --pidfile \
            /var/run/$NAME.pid --exec $DAEMON \
            && $0 restart \
            || exit 0
        ;;
  restart)
    echo -n "Restarting $DESC: "
        start-stop-daemon --stop --quiet --pidfile \
            /var/run/$NAME.pid --exec $DAEMON
        [ -n "$DODTIME" ] && sleep $DODTIME
        start-stop-daemon --start --quiet --pidfile \
            /var/run/$NAME.pid --exec $DAEMON -- $DAEMON_OPTS
        echo "$NAME."
		configure_tap
        ;;
  status)
    echo -n "$LABEL is "
    if running ;  then
        echo "running"
    else
        echo " not running."
        exit 1
    fi
    ;;
  *)
    N=/etc/init.d/$NAME
    # echo "Usage: $N {start|stop|restart|reload|force-reload}" >&2
    echo "Usage: $N {start|stop|restart|force-reload|status|force-stop}" >&2
    exit 1
    ;;
esac

exit 0
