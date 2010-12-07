/*
 * PUAE - The *nix Amiga Emulator
 *
 * QT GUI for PUAE
 *
 * Copyright 2010 Mustafa 'GnoStiC' TUFAN
 *
 */

#include <QtGui/QApplication>
#include <QThread>
#include "puae_bridge.h"
#include "puae_mainwindow.h"
//#include "ui_puae_mainwindow.h"

extern "C" {
#include "gui.h"
#include "options.h"
}

/*
 *
 *
 */
/*class MyThread : public QThread {
public:
	virtual void run();
};

void MyThread::run()
{
    QApplication a(NULL, NULL);
    puae_MainWindow w;
    w.show();
    a.exec();
}
*/

/* This function is called from od-macosx/main.m
 * WARNING: This gets called *before* real_main(...)!
 */
extern "C" void cocoa_gui_early_setup (void)
{
}

int gui_init (void)
{
//  MyThread *QT_GUI_Thread=new MyThread;
//	QT_GUI_Thread->start();

	return 0;
}

void gui_exit (void)
{

}

int gui_update (void)
{
}

void gui_display (int shortcut)
{
	int foo;

	if (shortcut == -1) {
	int argc;
	char *argv[0];
	argc = NULL;
	*argv = NULL;
		QApplication myApp(argc,argv);
		puae_MainWindow w;

		w.show();
		foo = myApp.exec();
	}
}

extern "C" void gui_message (const char *format,...)
{
}

void gui_fps (int fps, int idle)
{
}

void gui_handle_events (void)
{
}

void gui_led (int num, int on)
{
}

void gui_flicker_led (int led, int unitnum, int status)
{
}

void gui_gameport_axis_change (int port, int axis, int state, int max)
{
}

void gui_gameport_button_change (int port, int button, int onoff)
{
}

void gui_disk_image_change (int unitnum, const TCHAR *name, bool writeprotected)
{
}

void gui_filename (int num, const char *name)
{
}
