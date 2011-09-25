#include "mainwnd.h"

const char pathToDestDirParam[] = "/Settings/defaultDestDir";

MainWnd::MainWnd(QWidget *parent)
  :QMainWindow(parent), settings("BANTech", "srtcp")
{
  setupUi(this);
  dlg = new CopyDlg(&job, this);
  
  treeView->setContextMenuPolicy(Qt::CustomContextMenu);
  treeView->setModel(&model);
  treeView->setColumnWidth(0, 400);
  model.setRootPath(QDir::homePath());
  model.setReadOnly(true);
  strDestDir = settings.value(pathToDestDirParam, QDir::homePath()).toString();
  
  connect(actionCopy, SIGNAL(triggered()), this, SLOT(copyFiles()));
  connect(actionExit, SIGNAL(triggered()), this, SLOT(close()));
  connect(treeView, SIGNAL(customContextMenuRequested(const QPoint&)), 
          this, SLOT(showContextMenu(const QPoint &)));
}

MainWnd::~MainWnd()
{
  settings.setValue(pathToDestDirParam, strDestDir);
}

void MainWnd::copyFiles()
{
  QFileInfo fi;
  QModelIndexList list = treeView->selectionModel()->selectedRows();
  int countItems = list.count();
  
  if ( countItems > 0 ){
    QString dir = QFileDialog::getExistingDirectory(this, tr("Copy to..."),
                                                    strDestDir,
                                                    QFileDialog::ShowDirsOnly | 
                                                    QFileDialog::DontResolveSymlinks| 
                                                    QFileDialog::DontUseNativeDialog);
    
    if ( dir.isEmpty() ) return;
    
    strDestDir = dir;
    QStringList files;
    for ( int i = 0; i < countItems; i++ ){
      fi = model.fileInfo(list.at(i));
      if ( fi.isFile() || fi.isDir() ) files << fi.absoluteFilePath();
    }
    
    if ( files.count() > 0 )
      job.copyFiles(files, QDir(dir));
  }
}

void MainWnd::showContextMenu(const QPoint &position)
{
  QList<QAction *> actions;
  
  actions << actionCopy;
  QMenu::exec(actions, treeView->mapToGlobal(position));
}
