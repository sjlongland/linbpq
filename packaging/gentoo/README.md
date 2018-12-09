# `linbpq` repository

This is a work-in-progress Gentoo overlay for `linbpq`.  Right now, it compiles
on `amd64`, and it might have a chance of working, but more work is being done
to beat `linbpq` into some sort of sensible shape.

`linbpq` will be installed as `/usr/bin/linbpq`, whilst its data files will
live in `/var/lib/linbpq` and its logs in `/var/log/linbpq`.  The plan is to
have an OpenRC init script that can launch `linbpq` automatically at boot.

The `ebuild` script already sets up a `linbpq` user in the `uucp` group so that
it can access the serial ports (for communicating with TNCs) and sets up
`CONFIG_PROTECT` so your settings in `/var/lib/linbpq` don't get obliterated on
the next update.
