#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QStandardItemModel>

namespace Ui {
    class MainWindow;
}

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

private:
    Ui::MainWindow *ui;
    QStandardItemModel *files, *tags;
    int numFiles;
};

#endif // MAINWINDOW_H