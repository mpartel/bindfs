#!/bin/bash -e

autoreconf -fi

if [ "$1" == "-d" ]; then
	shift;
	echo "Running ./configure"
	echo
	sleep 1s
	./configure "$@"
elif [ -n "$1" ]; then
	echo
	echo "./configure $@"
	./configure "$@"
else
	echo
	echo "autogen.sh completed successfully."
	echo "Now run ./configure with the appropriate flags and then make."
fi

