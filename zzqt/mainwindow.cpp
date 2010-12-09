#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "../zz_priv.h"

#define MAX_LEN_VALUE 200

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
	ui->setupUi(this);
	connect(ui->treeViewTags, SIGNAL(expanded(const QModelIndex&)), this, SLOT(expanded(const QModelIndex&)));
	connect(ui->treeViewTags, SIGNAL(collapsed(const QModelIndex&)), this, SLOT(expanded(const QModelIndex&)));
	numFiles = 0;

	files = new QStandardItemModel(0, 1, this);
	files->setHeaderData(0, Qt::Horizontal, QString("Files"));
	ui->listView->setModel(files);

	tags = new QStandardItemModel(0, 4, this);
	tags->setHeaderData(0, Qt::Horizontal, QString("Tag"));
	tags->setHeaderData(1, Qt::Horizontal, QString("VR"));
	tags->setHeaderData(2, Qt::Horizontal, QString("Content"));
	tags->setHeaderData(3, Qt::Horizontal, QString("Tag name"));
	ui->treeViewTags->setModel(tags);
	ui->treeViewTags->resizeColumnToContents(1);

	connect(ui->actionQuit, SIGNAL(triggered()), this, SLOT(quit()));
}

void MainWindow::openFile(QString filename)
{
	struct zzfile szz, *zz;
	uint16_t group, element;
	long len, pos;
	char hexfield[20], vrfield[MAX_LEN_VR], contentfield[MAX_LEN_VALUE];
	int nesting;
	QList<QStandardItem *> hierarchy;
	const struct part6 *tag;

	nesting = 0;

	zz = zzopen(filename.toAscii().constData(), "r", &szz);
	zziterinit(zz);
	while (zziternext(zz, &group, &element, &len))
	{
		QStandardItem *last = NULL;

		pos = ftell(zz->fp);
		tag = zztag(group, element);

		// reduce nesting
		while (zz->currNesting < nesting && !hierarchy.isEmpty())
		{
			hierarchy.removeLast();
			nesting--;
		}

		if (!hierarchy.isEmpty())
		{
			last = hierarchy.last();
		}

		QStandardItem *item = NULL;
		QStandardItem *item2 = NULL, *item3 = NULL, *item4 = NULL;
		if (zz->current.vr != NO)
		{
			snprintf(hexfield, sizeof(hexfield) - 1, "%04x,%04x", group, element);
			item = new QStandardItem(hexfield);
			item->setData(QVariant(1));	// for enumerating sequence items
			item2 = new QStandardItem(zzvr2str(zz->current.vr, vrfield));
			if (tag)
			{
				item3 = new QStandardItem(tag->description);
			}
			else
			{
				item3 = new QStandardItem("(Unknown tag)");
			}
			item4 = new QStandardItem(zztostring(zz, contentfield, sizeof(contentfield)));
		}
		else if (ZZ_KEY(zz->current.group, zz->current.element) == DCM_Item && !hierarchy.isEmpty())
		{
			int count = last->data().toInt();
			snprintf(hexfield, sizeof(hexfield) - 1, "%04x,%04x (%d)", group, element, count);
			item = new QStandardItem(hexfield);
			item->setData(QVariant(1));	// for enumerating sequence items
			item2 = new QStandardItem("-");
			item3 = new QStandardItem("");
			item4 = new QStandardItem("");
			last->setData(QVariant(count + 1));
		}
		else	// ??
		{
			snprintf(hexfield, sizeof(hexfield) - 1, "%04x,%04x", group, element);
			item = new QStandardItem(hexfield);
			item->setData(QVariant(1));	// for enumerating sequence items
			item2 = new QStandardItem("-");
			item3 = new QStandardItem("");
			item4 = new QStandardItem("");
		}

		if (zz->nextNesting > nesting)
		{
			hierarchy.append(item);	// increase nesting
		}
		else if (zz->nextNesting < zz->currNesting && !hierarchy.isEmpty())
		{
			hierarchy.removeLast();
			nesting--;
		}

		nesting = zz->nextNesting;
		if (last)
		{
			int nextRow = last->rowCount();
			last->setRowCount(nextRow);
			last->setChild(nextRow, 0, item);
			last->setChild(nextRow, 1, item2);
			last->setChild(nextRow, 2, item3);
			last->setChild(nextRow, 3, item4);
		}
		else
		{
			int nextRow = tags->rowCount();
			tags->setRowCount(nextRow);
			tags->setItem(nextRow, 0, item);
			tags->setItem(nextRow, 1, item2);
			tags->setItem(nextRow, 2, item3);
			tags->setItem(nextRow, 3, item4);
		}
	}
	zz = zzclose(zz);
	ui->treeViewTags->resizeColumnToContents(0);
	ui->treeViewTags->resizeColumnToContents(2);
}

void MainWindow::addFile(QString filename)
{
	numFiles++;
	files->setRowCount(numFiles);
	files->setData(files->index(numFiles - 1, 0, QModelIndex()), filename);
	qWarning("Added %s", filename.toAscii().constData());
	openFile(filename);
}

void MainWindow::expanded(const QModelIndex &idx)
{
	Q_UNUSED(idx);
	ui->treeViewTags->resizeColumnToContents(0);
}

void MainWindow::quit()
{
	qApp->quit();
}

MainWindow::~MainWindow()
{
	delete ui;
}
