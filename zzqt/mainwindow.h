#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QStandardItemModel>
#include <QtOpenGL/QGLWidget>

namespace Ui
{
	class MainWindow;
}

class ImageViewer : public QGLWidget
{
public:
	ImageViewer(QWidget *parent = NULL);
	~ImageViewer();

protected:
	void paintGL();
	void resizeGL(int width, int height);
	void initializeGL();
};

class MainWindow : public QMainWindow
{
	Q_OBJECT

public:
	explicit MainWindow(QWidget *parent = 0);
	~MainWindow();

public slots:
	void quit();
	void addFile(QString filename);
	void openFile(QString filename);

protected slots:
	void tagexpanded(const QModelIndex &idx);
	void tagclicked(const QModelIndex &idx);
	void fileclicked(const QModelIndex idx);

private:
	Ui::MainWindow *ui;
	QStandardItemModel *files, *tags;
	int numFiles;
	struct zzfile szz, *zz;
	struct zztexture szzt, *zzt;
};

#endif // MAINWINDOW_H
