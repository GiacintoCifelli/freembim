
AM_MAKEFLAGS = --no-print-directory

noinst_LTLIBRARIES =

sbin_PROGRAMS = freembim

sources =
sources += \
	at_lib.c \
	at_lib.h \
	at_procs.c \
	at_procs.h \
	common.c \
	common.h \
	freembim.h \
	main.c \
	mbim_lib.c \
	mbim_lib.h \
	mbim_procs.c \
	mbim_procs.h \
	thread_at.c \
	thread_at.h \
	thread.c \
	thread.h \
	thread_mbim.c \
	thread_mbim.h \
	thread_tty.c \
	thread_tty.h \
	thread_udev.h \
	freembim.ver

if UDEV
sources += thread_udev.c
else
sources += thread_noudev.c
endif

freembim_SOURCES = $(sources)

if MAINTAINER_MODE
build_plugindir = $(abs_top_srcdir)/plugins/.libs
else
build_plugindir = $(plugindir)
endif

MAINTAINERCLEANFILES = Makefile.in \
	aclocal.m4 configure config.h.in config.sub config.guess \
	ltmain.sh depcomp compile missing install-sh mkinstalldirs

AM_CFLAGS = @UDEV_CFLAGS@ -fplan9-extensions -DPLUGINDIR=\""$(build_plugindir)"\"

freembim_LDFLAGS = -Wl,--export-dynamic -lpthread @UDEV_LIBS@
				-Wl,--version-script=$(srcdir)/freembim.ver

freembim_DEPENDENCIES =

BUILT_SOURCES = version.h

CLEANFILES = $(BUILT_SOURCES) $(rules_DATA)
