#!/bin/bash

# Enable environment variables to override tool commands.
: ${AUTOCONF=autoconf}
: ${AUTOHEADER=autoheader}
: ${AUTOMAKE=automake}
: ${ACLOCAL=aclocal}
: ${LIBTOOLIZE=libtoolize}

# Apple calls the GNU libtoolize "glibtoolize"
if [[ ! -x `which "$LIBTOOLIZE"` ]]; then
	LIBTOOLIZE=glibtoolize
fi
if [[ ! -x `which "$LIBTOOLIZE"` ]]; then
	echo "Cannot find libtoolize"
	exit 1
fi

# Add /usr/local/share/aclocal to aclocal's search path
if [[ -d /usr/local/share/aclocal ]]; then
	ACLOCAL="$ACLOCAL -I /usr/local/share/aclocal"
fi

rm -rf autom4te.cache
rm -f aclocal.m4
rm -f missing mkinstalldirs depcomp install-sh libtool

echo "Running $ACLOCAL..."
$ACLOCAL || exit 1

echo "Running $AUTOHEADER..."
$AUTOHEADER || exit 1

echo "Running $AUTOCONF..."
$AUTOCONF || exit 1

echo "Running $LIBTOOLIZE..."
$LIBTOOLIZE --automake --copy --force || exit 1

echo "Running $AUTOMAKE..."
$AUTOMAKE -a -c || exit 1

if [ "$1" == "-d" ]; then
	echo "Running configure --enable-debug"
	echo
	sleep 1s
	./configure --enable-debug
elif [ -n "$1" ]; then
	echo
	echo "./configure $@"
	./configure $@
else
	echo
	echo "autogen.sh completed successfully."
	echo "Now run ./configure with the appropriate flags and then make."
fi

