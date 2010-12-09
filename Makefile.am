bin_PROGRAMS = crystal

crystal_SOURCES = crystal.cc Socket.cc Socket.h mccpDecompress.h \
	          mccpDecompress.c io.cc grid.cc telnet.cc \
		common.h crystal.h grid.h io.h telnet.h wcwidth.c \
	scripting.cc scripting.h commands.cc commands.h common.cc \
	commandeditor.cc commandeditor.h keybindings.cc display.cc

man_MANS = crystal.6

DEBDIST = README.Debian debian/changelog.real debian/compat debian/control debian/copyright debian/crystal-default.ex debian/crystal.substvars debian/docs debian/rules debian/watch.ex

EXTRA_DIST = crystal.lua $(man_MANS) $(DEBDIST)

DEFS = @DEFS@ -DLOCALEDIR=\"${localedir}\" -DPACKAGE=\"${PACKAGE}\"

makedeb:
	fakeroot debian/rules binary

fakechange:
	printf "$(PACKAGE) ($(VERSION)-0.$(shell date +%y%m%d%H%M%S)) unstable; urgency=low\n\n  * snapshot\n\n -- Makefile Pixies <fake@email.invalid>  $(shell date +'%a, %d %b %Y %T %z')\n\n" >debian/changelog
	cat debian/changelog.real >>debian/changelog

realchange:
	cp debian/changelog.real debian/changelog

debsnap: fakechange makedeb

debrelease: realchange makedeb

debclean:
	fakeroot debian/rules clean
