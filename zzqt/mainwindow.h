#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QStandardItemModel>
#include <QtOpenGL/QGLWidget>
#include <QtOpenGL/QGLShaderProgram>

#include "../zztexture.h"

namespace Ui
{
	class MainWindow;
}

class ImageViewer : public QGLWidget
{
public:
	ImageViewer(QWidget *parent = NULL);
	~ImageViewer();
	void setVolume(struct zztexture *src);
	void setDepth(qreal value);

protected:
	void paintGL();
	void resizeGL(int width, int height);
	void initializeGL();

private:
	struct zztexture *zzt;
	QGLShaderProgram shader;
	GLfloat depth;
};

class ImageViewer3D : public QGLWidget
{
public:
	ImageViewer3D(QWidget *parent = NULL);
	~ImageViewer3D();
	void setVolume(struct zztexture *src);

protected:
	void paintGL();
	void resizeGL(int width, int height);
	void initializeGL();

private:
	struct zztexture *zzt;
	QGLShaderProgram shader;
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
	void setframe(int value);

private:
	Ui::MainWindow *ui;
	QStandardItemModel *files, *tags;
	int numFiles;
	struct zzfile szz, *zz;
	int frame;
	struct zztexture szzt, *zzt;
};

#endif // MAINWINDOW_H
