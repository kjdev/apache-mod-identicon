
ACLOCAL_AMFLAGS = -I m4

moddir = @APACHE_MODULEDIR@
mod_LTLIBRARIES = mod_identicon.la

mod_identicon_la_SOURCES = mod_identicon.c
mod_identicon_la_CFLAGS = @APACHE_CFLAGS@ @APACHE_INCLUDES@ @GD_CFLAGS@ @LIBMEMCACHED_CFLAGS@
mod_identicon_la_CPPFLAGS = @APACHE_CPPFLAGS@ @APACHE_INCLUDES@ @GD_CFLAGS@ @LIBMEMCACHED_CPPFLAGS@
mod_identicon_la_LDFLAGS = -avoid-version -module @APACHE_LDFLAGS@ @GD_LDFLAGS@ @LIBMEMCACHED_LDFLAGS@
mod_identicon_la_LIBS = @APACHE_LIBS@ @GD_LIBS@ @LIBMEMCACHED_LIBS@
