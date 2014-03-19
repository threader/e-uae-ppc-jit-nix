ACLOCAL_AMFLAGS = -I m4

EXTRA_DIST = m4 amiga android debian docs CHANGES README uae_data

SUBDIRS = src

dist_noinst_SCRIPTS = bootstrap.sh

dist-hook:
	rm -rf `find $(distdir) -name CVS`
	rm -rf `find $(distdir) -name *.bak`
	rm -rf `find $(distdir) -name *~`
	rm -rf `find $(distdir) -name *.orig`
