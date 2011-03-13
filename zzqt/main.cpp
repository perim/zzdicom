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
	    w.addFile("../samples/spine.dcm");

    w.show();

    return a.exec();
}
