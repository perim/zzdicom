#include <QtGui/QApplication>
#include "mainwindow.h"

#include "../zz_priv.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    MainWindow w;

    for (int i = zzutil(argc, argv, 2, "<filenames>", "DICOM image viewer"); i < argc; i++)
    {
	    w.addFile(argv[i]);
    }

    w.show();

    return a.exec();
}
