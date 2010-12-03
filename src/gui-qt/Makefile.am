QT_INCPATH = $(shell qmake -query QT_INSTALL_HEADERS)
MOC = $(shell which moc)
UIC = $(shell which uic)
RCC = $(shell which rcc)
INCLUDES = -I$(top_srcdir)/src/include -I$(top_builddir)/src -I$(top_srcdir)/src -I$(top_srcdir) -I$(QT_INCPATH) -I$(QT_INCPATH)/QtCore -I$(QT_INCPATH)/QtGui
DEFINES = -DQT_NO_DEBUG -DQT_GUI_LIB -DQT_CORE_LIB -DQT_SHARED
LIBS = -L/usr/lib -lQtGui -lQtCore -lpthread 

AM_CPPFLAGS  = @UAE_CPPFLAGS@
AM_CPPFLAGS += -I$(top_srcdir)/src/include -I$(top_builddir)/src -I$(top_srcdir)/src
AM_CFLAGS    = @UAE_CFLAGS@
AM_CFLAGS   += @QT_CFLAGS@
AR_FLAGS	= $(LIBS)

noinst_LIBRARIES = libguidep.a

libguidep_a_SOURCES = puae_mainwindow.cpp moc_puae_mainwindow.cpp

noinst_HEADERS = puae_mainwindow.h ui_puae_mainwindow.h

BUILT_SOURCES = moc_puae_mainwindow.cpp ui_puae_mainwindow.h

CLEANFILES = *.o qrc*.* ui*.* moc*.*

moc_%.cpp: %.h
	$(MOC) $(DEFINES) $(INCLUDES) -o $@ $<

ui_%.h: %.ui
	$(UIC) $< -o $@
  
qrc_%.cpp: %.qrc
	$(RCC) $< -name Resources -o $@
