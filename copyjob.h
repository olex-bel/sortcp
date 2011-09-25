#ifndef COPYJOB_H
#define COPYJOB_H

#include <QString>
#include <QList>
#include <QDir>
#include <QThread>
#include <QWaitCondition>
#include <QMutex>
#include <QMap>

struct ReqInfo
{
  QStringList sourceFiles;
  QDir destDir;
};

struct CopyInfo {
  enum FileType {FILE, DIR};

  // служебные данные, для реализации пропуска файлов при возникновении ошибки создания каталога
  int index;
  int level;
  
  QString sFileName;
  QString sSourcePath;
  QString sDestPath;
  FileType type;
  qint64 size;
  
  void fill(const QString& fileName, const QString& sourcePath, const QString& destPath,
            FileType t, qint64 s, int i, int l)
  {
    sFileName = fileName;
    sSourcePath = sourcePath;
    sDestPath = destPath;
    type = t;
    size = s;
    index = i;
    level = l;
  }
};

typedef QList<CopyInfo> CopyInfoList;

class CopyJob: public QThread
{
  Q_OBJECT

  public:
    enum CopyError {NoError, UserTerminate, UnknownFileType, DestDirNotExists, SourceNotExists, MakeDirError, CopyFileError, IncorectParam};
    
    CopyJob(QObject * parent = 0);
    ~CopyJob();
    int copyFiles(const QStringList& sourceFiles, const QDir& destDir);
    
  protected:
    void run();
    
  private:
    enum UserChoice {RetryOper, SkipOper, CancelOper};
    
    QMap<int, ReqInfo> requestQueue;
    QWaitCondition newCopyCondition;
    QWaitCondition interactionCondition;
    int idCounter;
    QMutex mutex;
    bool waitingForInteraction;
    UserChoice interactionResult;
    QDir::Filters filesFilter;
    QDir::SortFlags filesSort;
    volatile bool stopWorking;
    volatile bool breakCurrentWork;
    
    void handle(int id, const QStringList& sourceFiles, const QDir& destDir);
    void files_copy(int id, const CopyInfo& fileInfo, int progressValue);
    void copy(int id, const QString& fileName, const QString& newName);
    int prepareCopy(int index, int level, const QString& sName, QString destDir, CopyInfoList& files);
    int skipFiles(const CopyInfoList& files, int pos);
    void setInteractionResult(UserChoice uc);
    void stopJob();
    int handleFilesCopyException(int id, CopyError error, const QString strError);
        
  signals:
    void starting(int id);
    void startingCopy(int id, int countFiles);
    void finished(int id);
    void error(int id, int error, const QString& strError);
    void errorWaitingForIteration(int id, int error, const QString& strError);
    void startCopyFile(int id, const QString& name, qint64 size);
    void copyProgress(int id, int progress);
    void endingCopyFile(int id, int progressValue);
    
  public slots:
    void cancel();
    void skip();
    void retry();
    void stopCurrentWork();
};

#endif
