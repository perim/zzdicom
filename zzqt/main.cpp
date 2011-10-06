#include <QtGui/QApplication>
#include "mainwindow.h"

#include "../zz_priv.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    MainWindow w;

    w.show();	// we must show first to ensure GL context is created before uploading data

    for (int i = zzutil(argc, argv, 1, "<filenames>", "DICOM image viewer", NULL); i < argc; i++)
    {
	    w.addFile(argv[i]);
    }
    if (argc == 1)
	    w.addFile("../samples/spine.dcm");

    return a.exec();
}
