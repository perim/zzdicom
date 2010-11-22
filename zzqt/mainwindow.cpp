#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "../zz_priv.h"

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    numFiles = 0;

    files = new QStandardItemModel(1, 1, this);
    files->setHeaderData(0, Qt::Horizontal, QString("Files"));
    ui->listView->setModel(files);
    ui->listView->setSelectionBehavior(QAbstractItemView::SelectRows);

    tags = new QStandardItemModel(1, 3, this);
    tags->setHeaderData(0, Qt::Horizontal, QString("Tag"));
    tags->setHeaderData(1, Qt::Horizontal, QString("VR"));
    tags->setHeaderData(2, Qt::Horizontal, QString("Content"));
    ui->treeViewTags->setModel(tags);

    connect(ui->actionQuit, SIGNAL(triggered()), this, SLOT(quit()));
}

void MainWindow::openFile(QString filename)
{
	struct zzfile szz, *zz;
	uint16_t group, element;
	long len, pos;
	bool header = false;
	char hexfield[12];
	int numTags, nesting;
	QList<QModelIndex> hierarchy;

	nesting = 0;
	numTags = 0;
	zz = zzopen(filename.toAscii().constData(), "r", &szz);
	hierarchy.append(QModelIndex());
	while (zz && !feof(zz->fp) && !ferror(zz->fp))
	{
		zzread(zz, &group, &element, &len);
		qWarning("Reading (%04x,%04x)", group, element);
		pos = ftell(zz->fp);

		if (zz->ladderidx == 0 && !header)
		{
			header = true;
		}

		snprintf(hexfield, sizeof(hexfield) - 1, "%04x,%04x", group, element);

		numTags++;
		tags->setRowCount(numTags);
		QModelIndex idx(tags->index(numTags - 1, 0, hierarchy.last()));

		if (zz->currNesting > nesting)
		{
			hierarchy.append(idx);	// increase nesting
		}

		// reduce nesting
		while (zz->currNesting < nesting && !hierarchy.isEmpty())
		{
			hierarchy.removeLast();
			qWarning("dropping index (%d, %d)", zz->currNesting, nesting);
			nesting--;
		}

		nesting = zz->currNesting;
		tags->setData(/*idx*/tags->index(numTags - 1, 0, QModelIndex()), QString(hexfield));

		if (group == 2 && element == 0)
		{
			//  TEST!
			numTags++;
			tags->setRowCount(numTags);
			tags->setData(tags->index(numTags - 1, 0, tags->index(numTags - 2, 0, QModelIndex())), "TEST");
		}

		// Abort early, skip loading pixel data into memory if possible
		if (len != UNLIMITED && pos + len >= zz->fileSize)
		{
			break;
		}

		// Skip ahead
		if (!feof(zz->fp) && len != UNLIMITED && len > 0 && !(group == 0xfffe && element == 0xe000 && zz->pxstate != ZZ_PIXELITEM)
		    && zz->current.vr != SQ)
		{
			fseek(zz->fp, pos + len, SEEK_SET);
		}
	}
	zz = zzclose(zz);
}

void MainWindow::addFile(QString filename)
{
	numFiles++;
	files->setRowCount(numFiles);
	files->setData(files->index(numFiles - 1, 0, QModelIndex()), filename);
	qWarning("Added %s", filename.toAscii().constData());
	openFile(filename);
}

void MainWindow::quit()
{
	qApp->quit();
}

MainWindow::~MainWindow()
{
    delete ui;
}
