#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "../zz_priv.h"
#include "../part6.h"

#define MAX_LEN_VALUE 200

ImageViewer::ImageViewer(QWidget *parent) : QGLWidget(parent)
{

}

ImageViewer::~ImageViewer()
{

}

void ImageViewer::resizeGL(int width, int height)
{
	if (height == 0)
	{
		height = 1;
	}
	glViewport(0, 0, width, height);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glOrtho(-1.0, 1.0, -1.0, 1.0, 0.0, 1.0);
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
}

void ImageViewer::initializeGL()
{
	glShadeModel(GL_SMOOTH);
	glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
	glClearDepth(1.0f);
	glEnable(GL_DEPTH_TEST);
	glDepthFunc(GL_LEQUAL);
	glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
}

void ImageViewer::paintGL()
{
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glLoadIdentity();
	glBegin(GL_QUADS);
	glColor3f(1.0f, 0.0f, 0.0f);
	glVertex3f(-1.0f, 1.0f, 0.0f);
	glColor3f(0.0f, 1.0f, 0.0f);
	glVertex3f( 1.0f, 1.0f, 0.0f);
	glColor3f(0.0f, 0.0f, 1.0f);
	glVertex3f( 1.0f,-1.0f, 0.0f);
	glColor3f(1.0f, 1.0f, 0.0f);
	glVertex3f(-1.0f, -1.0f, 0.0f);
	glEnd();
}

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
	ui->setupUi(this);
	connect(ui->treeViewTags, SIGNAL(expanded(const QModelIndex &)), this, SLOT(tagexpanded(const QModelIndex &)));
	connect(ui->treeViewTags, SIGNAL(collapsed(const QModelIndex &)), this, SLOT(tagexpanded(const QModelIndex &)));
	connect(ui->treeViewTags, SIGNAL(clicked(const QModelIndex &)), this, SLOT(tagclicked(const QModelIndex &)));
	connect(ui->listView, SIGNAL(clicked(QModelIndex)), this, SLOT(fileclicked(QModelIndex)));
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
	uint16_t group, element;
	long len, pos;
	char hexfield[20], vrfield[MAX_LEN_VR], contentfield[MAX_LEN_VALUE];
	int nesting;
	QList<QStandardItem *> hierarchy;
	const struct part6 *tag;

	nesting = 0;

	zz = zzopen(filename.toAscii().constData(), "r", &szz);
	zzt = zzcopytotexture(zz, &szzt);
	zziterinit(zz);
	tags->clear();
	while (zziternext(zz, &group, &element, &len))
	{
		QStandardItem *last = NULL;
		QString infoText;

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
			snprintf(hexfield, sizeof(hexfield) - 1, "%04x,%04x (%d)", group, element, zz->ladder[zz->ladderidx].item + 1);
			item = new QStandardItem(hexfield);
			item2 = new QStandardItem("-");
			item3 = new QStandardItem("");
			item4 = new QStandardItem("");
		}
		else	// ??
		{
			snprintf(hexfield, sizeof(hexfield) - 1, "%04x,%04x", group, element);
			item = new QStandardItem(hexfield);
			item2 = new QStandardItem("-");
			item3 = new QStandardItem("");
			item4 = new QStandardItem("");
		}
		infoText.append("Tag starts at byte " + QString::number(zz->current.pos) + " and is of size " + QString::number(zz->current.length) + ".");
		if (!zz->current.valid)
		{
			item->setForeground(QBrush(QColor(Qt::red)));
			item2->setForeground(QBrush(QColor(Qt::red)));
			item3->setForeground(QBrush(QColor(Qt::red)));
			item4->setForeground(QBrush(QColor(Qt::red)));
			infoText.append("\n\n" + QString(zz->current.warning));
		}
		item->setData(infoText);
		item2->setData(infoText);
		item3->setData(infoText);
		item4->setData(infoText);

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
	QModelIndex idx = files->index(numFiles - 1, 0);
	files->setData(idx, filename, Qt::UserRole);
	files->setData(idx, filename, Qt::DisplayRole);
	if (numFiles == 1)	// select first file
	{
		ui->listView->setCurrentIndex(idx);
		fileclicked(idx);
	}
}

void MainWindow::tagexpanded(const QModelIndex &idx)
{
	Q_UNUSED(idx);
	ui->treeViewTags->resizeColumnToContents(0);
}

void MainWindow::tagclicked(const QModelIndex &idx)
{
	QStandardItem *item = tags->itemFromIndex(idx);
	ui->textBrowserTagInfo->setText(item->data().toString());
}

void MainWindow::fileclicked(const QModelIndex idx)
{
	QStandardItem *item = files->itemFromIndex(idx);
	QString filename = item->data(Qt::UserRole).toString();
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
