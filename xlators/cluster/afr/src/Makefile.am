xlator_LTLIBRARIES = afr.la pump.la
xlatordir = $(libdir)/glusterfs/$(PACKAGE_VERSION)/xlator/cluster

afr_common_source = afr-dir-read.c afr-dir-write.c afr-inode-read.c afr-inode-write.c afr-open.c afr-transaction.c afr-self-heal-data.c afr-self-heal-common.c afr-self-heal-metadata.c afr-self-heal-entry.c afr-self-heal-algorithm.c

afr_la_LDFLAGS = -module -avoidversion
afr_la_SOURCES = $(afr_common_source) afr.c
afr_la_LIBADD = $(top_builddir)/libglusterfs/src/libglusterfs.la

pump_la_LDFLAGS = -module -avoidversion
pump_la_SOURCES =  $(afr_common_source) pump.c
pump_la_LIBADD = $(top_builddir)/libglusterfs/src/libglusterfs.la

noinst_HEADERS = afr.h afr-transaction.h afr-inode-write.h afr-inode-read.h afr-dir-read.h afr-dir-write.h afr-self-heal.h afr-self-heal-common.h afr-self-heal-algorithm.h pump.h afr-mem-types.h afr-common.c

AM_CFLAGS = -fPIC -D_FILE_OFFSET_BITS=64 -D_GNU_SOURCE -Wall -D$(GF_HOST_OS) \
	    -I$(top_srcdir)/libglusterfs/src -I$(top_srcdir)/contrib/md5 -shared -nostartfiles $(GF_CFLAGS)

CLEANFILES =

uninstall-local:
	rm -f $(DESTDIR)$(xlatordir)/replicate.so
	rm -f $(DESTDIR)$(xlatordir)/pump.so

install-data-hook:
	ln -sf afr.so $(DESTDIR)$(xlatordir)/replicate.so
