#ifndef MAINWND_H
#define MAINWND_H

#include <QtGui>
#include "ui_mainwnd.h"
#include "copyjob.h"
#include "copydlg.h"


class MainWnd: public QMainWindow, private Ui::MainWindow
{
  Q_OBJECT
     
  public:
    MainWnd(QWidget *parent = 0);
    ~MainWnd();
    
  private:
    QFileSystemModel model;
    CopyJob job;
    CopyDlg* dlg;
    QString strDestDir;
    QSettings settings;
    
  public slots:
    void copyFiles();
    void showContextMenu(const QPoint &position);
};

#endif
