#!/bin/sh
# vim: set tw=78 noet fileencoding=utf-8:
# Build a Debian package from the git repository.

# Stop on any error
set -e

# Figure out where our sources are.
SOURCEDIR=$( realpath $( dirname "$0" )/../.. )

# Where do we keep the built packages?  By default, in ${SOURCEDIR}/dist
: ${OUTPUT:=${SOURCEDIR}/dist}

# What patch revision do we put on the package?
: ${PATCHREV:=1}

# Create a working directory in which to build the sources,
# unless we're given one via the environment.
if [ -z "${WORKDIR}" ]; then
	WORKDIR=$( mktemp -d )

	if [ -z "${KEEP_WORKDIR}" ]; then
		cleanup() {
			rm -fr "${WORKDIR}"
		}
		trap cleanup EXIT
	else
		cleanup() {
			:
		}
	fi
else
	cleanup() {
		:
	}
fi

# If we're not given a version, obtain one from git-describe
if [ -z "${VERSION}" ]; then
	# git may append -XX-gYYYYYYY where XX is the number of commits
	# since the last tag, and YYYYYYY is the current commit ID.  Useful
	# to have in the version string for development builds, *BUT* we must
	# tweak it a bit to keep Debian happy.  The following will transform
	#
	#	vW.X.Y.Z-AA-gBBBBBBBB
	# to
	#	vW.X.Y.Z+pAAgBBBBBBBB

	VERSION=$( ( cd ${SOURCEDIR} ; git describe ) | sed \
		-e 's/-\([0-9]\+\)-g\([0-9a-f]\+\)$/+p\1g\2/' )
fi

# Create the release sources
make -C ${SOURCEDIR} DESTDIR=${WORKDIR} VERSION=${VERSION} release

# Rename the upstream package to conform with Debian expectations
mv ${WORKDIR}/linbpq-${VERSION}.tar.xz \
	${WORKDIR}/linbpq_${VERSION#v}.orig.tar.xz

# Unpack the working tree
tar -C ${WORKDIR} -xJf ${WORKDIR}/linbpq_${VERSION#v}.orig.tar.xz

# Apply the Debian packaging overlay to it
tar -C ${SOURCEDIR}/packaging/debian -cf - debian \
	| tar -C ${WORKDIR}/linbpq-${VERSION} -xf -

# Update the Debian build version, stripping off the initial 'v'
# and adding a patch revision
sed -i -e "1 s/(.*)/(${VERSION#v}-${PATCHREV})/" \
	${WORKDIR}/linbpq-${VERSION}/debian/changelog

# Try a build
( cd ${WORKDIR}/linbpq-${VERSION} && dpkg-buildpackage $@ )

# If that succeeded, copy the outputs
if [ ! -d "${OUTPUT}" ] ; then
	mkdir "${OUTPUT}"
fi
cp ${WORKDIR}/linbpq_${VERSION#v}* "${OUTPUT}"
