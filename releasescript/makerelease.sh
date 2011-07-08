#!/bin/bash

if [ -z "$1" ]; then
    echo "Usage: $0 version-tag"
    exit 1
fi

VERSION="$1"
REPO_URL="git://github.com/mpartel/bindfs.git"

umask 0022

# We work in a temporary dir to avoid interference
# of the autotools files in the parent dir.
OUTPUTDIR=`pwd`
TMPDIR="/tmp/bindfs-build"
mkdir $TMPDIR || exit 1
pushd "$TMPDIR"

# Download the release source
git clone "$REPO_URL" "bindfs-$VERSION" || exit 1

# Prepare the source tree:
# - check out the release tag
# - remove .git
# - run autotools
pushd "bindfs-$VERSION"
git checkout "$VERSION" || exit 1
rm -Rf .git
./autogen.sh || exit 1
rm -Rf autom4te.cache
popd

# Make the source package
tar cvzf "bindfs-${VERSION}.tar.gz" "bindfs-$VERSION" || exit 1

# Get the change log and man-page
cp "bindfs-$VERSION/ChangeLog" ./bindfs-ChangeLog.txt
cp "bindfs-$VERSION/src/bindfs.1" ./bindfs.1

# Create the HTML man page
rman -f HTML -r "" bindfs.1 > bindfs.1.html || exit 1

# Compile the source
pushd "bindfs-$VERSION"
./configure
make
popd

# Get the bindfs --help text
"bindfs-$VERSION/src/bindfs" --help > bindfs-help.txt

# Copy products to original dir
cp -r "bindfs-$VERSION.tar.gz" \
      bindfs-ChangeLog.txt \
      bindfs.1 \
      bindfs.1.html \
      bindfs-help.txt \
      "$OUTPUTDIR/"

# Clean up and we're done
popd
rm -Rf $TMPDIR

echo
echo "DONE!"
echo

