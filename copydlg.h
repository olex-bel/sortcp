#ifndef COPYDLG_H
#define COPYDLG_H

#include <QDialog>
#include "ui_copydialog.h"
#include "copyjob.h"

class CopyDlg: public QDialog, private Ui::CopyDialog
{
  Q_OBJECT
  
  public:
    CopyDlg(CopyJob* job, QWidget* parent=0);
    
  private:
    QString getStringError(int error);
    CopyJob* fileCopyJob;
    int currentId;
        
  private slots:
    void starting(int id);
    void startingCopy(int id, int countFiles);
    void finished(int id);
    void error(int id, int error, const QString& strError);
    void errorWaitingForIteration(int id, int error, const QString& strError);
    void startCopyFile(int id, const QString& name, qint64 size);
    void endingCopyFile(int id, int progressValue);
    void copyProgress(int id, int progress);
};

#endif
