#!/bin/sh

BIN_NAME=/usr/bin/aesdsocket
BIN_PID=/var/tmp/${BIN_NAME}.pid
BIN_OPTS="-d"

case "$1" in
	start)
		echo "starting ${BIN_NAME}";
		start-stop-daemon --start --startas ${BIN_NAME} -- ${BIN_OPTS}
		;;
	stop)
		echo "stopping daemon ${BIN_NAME}";
		start-stop-daemon --stop --pidfile ${BIN_PID} -o 
        ;;
	restart)
		echo "stopping daemon ${BIN_NAME}";
		start-stop-daemon --stop --pidfile ${BIN_PID} -o
		echo "starting ${BIN_NAME}";
		start-stop-daemon --start --startas ${BIN_NAME} -- ${BIN_OPTS}
		;;
	*)
		echo "Usage ${0} {start|stop|restart}";
		;;
esac

exit 0;
