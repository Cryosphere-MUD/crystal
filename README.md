Crystal is a simple mud client for people who terminals.

# Build instructions

Crystal works on Linux, Mac OS, and possibly other unixes. Install cmake,
and then build like so

git submodule init
git submodule update
mkdir -p build && cd build && cmake .. && make

# Features

Crystal aims to keep an unobtrusive interface, with similar commands and
bindings to Unix telnet.  We support

* an isolated input line line with readline()-style commands and command history
* paging up and down, with a split screen scrollback
* lua scripting - see crystal.lua(crystal.lua) for examples
* MCCP2/MCCPX - compression protocols
* TLS (if openssl is available)
* Cryosphere's telnet <a href="https://musicmud.org/crystal/telopt-mplex.txt">overlay/mplex extension</a>