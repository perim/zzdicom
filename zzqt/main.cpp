#include <QtGui/QApplication>
#include "mainwindow.h"

#include "../zz_priv.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    MainWindow w;

    for (int i = zzutil(argc, argv, 1, "<filenames>", "DICOM image viewer"); i < argc; i++)
    {
	    w.addFile(argv[i]);
    }
    if (argc == 1)
	    w.addFile("/home/per/zzdicom/EC0A7F5B");

    w.show();

    return a.exec();
}
