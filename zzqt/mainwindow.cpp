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
	char hexfield[12];
	int nesting;
	QList<QStandardItem *> hierarchy;

	nesting = 0;

	zz = zzopen(filename.toAscii().constData(), "r", &szz);
	zziterinit(zz);
	while (zziternext(zz, &group, &element, &len))
	{
		pos = ftell(zz->fp);

		snprintf(hexfield, sizeof(hexfield) - 1, "%04x,%04x", group, element);
		QStandardItem *item = new QStandardItem(hexfield);
		QStandardItem *last = NULL;

		if (!hierarchy.isEmpty())
		{
			last = hierarchy.last();
		}

		if (zz->nextNesting > nesting)
		{
			hierarchy.append(item);	// increase nesting
		}

		// reduce nesting
		while (zz->currNesting < nesting && !hierarchy.isEmpty())
		{
			hierarchy.removeLast();
			nesting--;
		}

		nesting = zz->nextNesting;
		if (last)
		{
			last->appendRow(item);
		}
		else
		{
			tags->appendRow(item);
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
