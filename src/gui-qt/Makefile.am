QT_INCPATH = $(shell qmake -query QT_INSTALL_HEADERS)
QT_INSPATH = $(shell qmake -query QT_INSTALL_DATA)
MOC = $(shell which moc)
UIC = $(shell which uic)
RCC = $(shell which rcc)

INCLUDES  = -I$(top_srcdir)/src/include -I$(top_builddir)/src -I$(top_srcdir)/src -I$(top_srcdir)
INCLUDES += -I$(QT_INCPATH) -I$(QT_INCPATH)/QtCore -I$(QT_INCPATH)/QtGui
INCLUDES += -I$(QT_INSPATH)/mkspecs/default
if BUILD_MACOSX_BUNDLE
INCLUDES += -I/usr/local/Qt4.7/mkspecs/macx-xcode 
INCLUDES += -I/System/Library/Frameworks/CarbonCore.framework/Headers
INCLUDES += -F/Library/Frameworks
INCLUDES += -I/Library/Frameworks/QtCore.framework/Versions/4/Headers
INCLUDES += -I/Library/Frameworks/QtGui.framework/Versions/4/Headers
endif
DEFINES = -DQT_NO_DEBUG -DQT_GUI_LIB -DQT_CORE_LIB -DQT_SHARED
LIBS = -L/usr/lib -lQtGui -lQtCore -lpthread 
##QMAKE_CFLAGS = $(QMAKE_CFLAGS) ) -m32
##QMAKE_CXXFLAGS = $(QMAKE_CXXFLAGS) -m32

AM_CPPFLAGS = @UAE_CPPFLAGS@
AM_CPPFLAGS += -I$(top_srcdir)/src/include -I$(top_builddir)/src -I$(top_srcdir)/src
AM_CFLAGS = @UAE_CFLAGS@
AM_CFLAGS += @QT_CFLAGS@
AR_FLAGS = $(LIBS)

noinst_LIBRARIES = libguidep.a

libguidep_a_SOURCES = \
		puae_mainwindow.cpp moc_puae_mainwindow.cpp \
		puae_bridge.cpp moc_puae_bridge.cpp \
		puae_misc.cpp moc_puae_misc.cpp \
		puae_registry.cpp moc_puae_registry.cpp

noinst_HEADERS = \
		ui_puae_mainwindow.h \
		puae_mainwindow.h \
		puae_bridge.h \
		puae_misc.h \
		puae_registry.h

BUILT_SOURCES = \
		moc_puae_mainwindow.cpp \
		moc_puae_bridge.cpp \
		moc_puae_misc.cpp \
		moc_puae_registry.cpp \
		ui_puae_mainwindow.h

CLEANFILES = *.o qrc*.* ui*.* moc*.*

moc_%.cpp: %.h
	$(MOC) $(DEFINES) $(INCLUDES) -o $@ $<

ui_%.h: %.ui
	$(UIC) $< -o $@
  
qrc_%.cpp: %.qrc
	$(RCC) $< -name Resources -o $@
