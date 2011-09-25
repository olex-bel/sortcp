#include "copydlg.h"
#include <QMessageBox>
#include <QtDebug>

CopyDlg::CopyDlg(CopyJob* job, QWidget* parent)
  :QDialog(parent), fileCopyJob(job)
{
  setupUi(this);
  
  connect(fileCopyJob, SIGNAL(starting(int)), this, SLOT(starting(int)));
  connect(fileCopyJob, SIGNAL(finished(int)), this, SLOT(finished(int)));
  connect(fileCopyJob, SIGNAL(startingCopy(int, int)), this, SLOT(startingCopy(int, int)));
  connect(fileCopyJob, SIGNAL(startCopyFile(int, const QString&, qint64)), 
          this, SLOT(startCopyFile(int, const QString&, qint64)));
  connect(fileCopyJob, SIGNAL(endingCopyFile(int,int)), this, SLOT(endingCopyFile(int, int)));
  connect(fileCopyJob, SIGNAL(error(int, int, const QString&)), 
          this, SLOT(error(int, int, const QString&)));
  connect(fileCopyJob, SIGNAL(errorWaitingForIteration(int, int, const QString&)), 
          this, SLOT(errorWaitingForIteration(int, int, const QString&)));
  connect(fileCopyJob, SIGNAL(copyProgress(int, int)), this, SLOT(copyProgress(int, int)));
  connect(pushButton, SIGNAL(clicked()), fileCopyJob, SLOT(stopCurrentWork()));
  connect(this, SIGNAL(rejected()), fileCopyJob, SLOT(stopCurrentWork()));
}

void CopyDlg::starting(int id)
{
  currentId = id;
  progressBar->reset();
  progressFile->reset();
  label->setText(tr("Prepare to copying ..."));
  show();
}

void CopyDlg::finished(int id)
{
  if ( id != currentId ) qDebug() <<  Q_FUNC_INFO << " wrong ID.";
  hide();
}

void CopyDlg::startingCopy(int id, int countFiles)
{
  if ( id != currentId ) qDebug() <<  Q_FUNC_INFO << " wrong ID.";
  progressBar->setRange(0, countFiles);
  progressFile->setRange(0, 100);
}

void CopyDlg::startCopyFile(int id, const QString& name, qint64 size)
{
  if ( id != currentId ) qDebug() <<  Q_FUNC_INFO << " wrong ID.";
  label->setText(name);
}

void CopyDlg::endingCopyFile(int id, int progressValue)
{
  if ( id != currentId ) qDebug() <<  Q_FUNC_INFO << " wrong ID.";
  progressBar->setValue(progressValue);
}

void CopyDlg::error(int id, int error, const QString& strError)
{
  if ( id != currentId ) qDebug() <<  Q_FUNC_INFO << " wrong ID.";
  QMessageBox::critical(this, tr("Error"), getStringError(error)+strError);
}

void CopyDlg::errorWaitingForIteration(int id, int error, const QString& strError)
{
  if ( id != currentId ) qDebug() <<  Q_FUNC_INFO << " wrong ID.";
  QMessageBox iterationMsgBox;
  iterationMsgBox.setText(getStringError(error));
  iterationMsgBox.setInformativeText(strError);
  iterationMsgBox.setStandardButtons(QMessageBox::Retry | QMessageBox::Ignore | QMessageBox::Cancel);
  iterationMsgBox.setDefaultButton(QMessageBox::Cancel);
  int ret = iterationMsgBox.exec();
  
  switch ( ret )
  {
    case QMessageBox::Retry:
      fileCopyJob->retry();
      break;
    case QMessageBox::Ignore:
      fileCopyJob->skip();
      break;
    default:
      fileCopyJob->cancel();
  }
}

QString CopyDlg::getStringError(int error)
{
  QString errorMessage(tr("Unknown error."));
  
  switch ( error )
  {
    case CopyJob::UnknownFileType:
      errorMessage = tr("Unknown file type.");
      break;
    case CopyJob::DestDirNotExists:
      errorMessage = tr("Directory of destination is not exists.");
      break;
    case CopyJob::SourceNotExists:
      errorMessage = tr("Source file is not exists.");
      break;
    case CopyJob::MakeDirError:
      errorMessage = tr("Error creating directory.");
      break;
    case CopyJob::CopyFileError:
      errorMessage = tr("Error creating file.");
      break;
  }
  
  return errorMessage;
}

void CopyDlg::copyProgress(int id, int progress)
{
  if ( id != currentId ) qDebug() <<  Q_FUNC_INFO << " wrong ID.";
  progressFile->setValue(progress);
}
