#include <QtGui/QApplication>
#include "puae_mainwindow.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    puae_MainWindow w;
    w.show();

    return a.exec();
}

