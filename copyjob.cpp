#include "copyjob.h"
#include <QTemporaryFile>
#include <iostream>

#define BLOCK_SIZE 4096

class CopyExcepion
{ 
public:
  CopyExcepion(CopyJob::CopyError erroCode, const QString& strInfo)
  : error(erroCode), strError(strInfo)
  {
  }
  
  CopyJob::CopyError error;
  QString strError;
  
  static void throwIf(bool condition, CopyJob::CopyError error, const QString& strInfo)
  {
    if ( condition ) throw CopyExcepion(error, strInfo);
  }
};

CopyJob::CopyJob(QObject * parent)
  :QThread(parent), idCounter(0), waitingForInteraction(false), interactionResult(CancelOper),
   stopWorking(false)
{
  filesFilter = QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot | QDir::CaseSensitive;
  filesSort = QDir::Name | QDir::DirsFirst | QDir::IgnoreCase;
}

CopyJob::~CopyJob()
{
  stopJob();
  wait();
}

void CopyJob::stopJob()
{
  QMutexLocker locker(&mutex);
  stopWorking = true; // флаг завершения цикл обработки очереди
  breakCurrentWork = true; // флаг преривания текущего задания
  newCopyCondition.wakeOne(); 
  interactionCondition.wakeOne();
}

void CopyJob::stopCurrentWork()
{
  QMutexLocker locker(&mutex);
  breakCurrentWork = true; // флаг преривания текущего задания
  newCopyCondition.wakeOne();
  interactionCondition.wakeOne();
}

/*
Функция добавляет в очередь requestQueue новое задание копирования файлов. 
Запускает поток копирования, если он был остановлен. Возвращает идентификатор задания.
*/
int CopyJob::copyFiles(const QStringList& sourceFiles, const QDir& destDir)
{
  QMutexLocker locker(&mutex);
  ReqInfo r;
  
  r.sourceFiles = sourceFiles;
  r.destDir = destDir;
  requestQueue[idCounter] = r;
  
  start();
  newCopyCondition.wakeOne();
  
  return idCounter++;
}

/*
Функция получает индекс элемента, на котором произошла ошибка, определяет его тип
- если это файл, то просто увеличивает индекс на 1
- если это каталог, пропускает все файлв с files.at(i).index = files.at(index).index и 
files.at(i).level > files.at(index.).level (при переборе учитывается размер списка)
Возвращаетиндекс следующиго элемента для копирования. 
Если pos вне диапазона 0 <= pos < count возвращает files.count().
*/
int CopyJob::skipFiles(const CopyInfoList& files, int pos)
{
  if ( pos < 0 || pos >= files.count() ) return files.count();
  
  int nextPos(files.count());
  int index(files.at(pos).index);
  int level(files.at(pos).level);
  
  switch ( files.at(pos).type )
  {
    case CopyInfo::FILE:
      nextPos = pos + 1;
      break;
        
    case CopyInfo::DIR:
      while ( files.at(pos).index == index && files.at(pos).level == level && pos < files.count() )
        pos++;
      nextPos = pos;
      break;
  }
  
  return nextPos;
}

/*
Функция обрабатывающая текущее задание. Работает в два этапа: 1. формирует список файлов для копирования и 2. выполняет копирование со создание каталогов в каталоге destDir.
*/
void CopyJob::handle(int id, const QStringList& sourceFiles, const QDir& destDir)
{
  CopyInfoList files;
  int i, countFiles(0);
  
  // если каталог не существует, завершаем работу
  if ( !destDir.exists() ){ 
    emit error(id, DestDirNotExists, destDir.absolutePath());
    return;
  }
  
  emit starting(id);
  // если при подгатовке файлов к копированию произошла ошибка, завершает работу
  for ( i = 0; i < sourceFiles.count(); i++ )
    try{
      countFiles += prepareCopy(i, 0, sourceFiles.at(i), destDir.absolutePath(), files);
    }catch (const CopyExcepion& e){
      if ( e.error != UserTerminate ) emit error(id, e.error, e.strError);
      return;
    }
  
  emit startingCopy(id, countFiles); // сумарное колличество каталогов и файлов.
    
  i = 0;
  while ( i < files.count() ){
    try{
      files_copy(id, files.at(i), i+1);
      i++;
    }catch (const  CopyExcepion& e){
      if ( e.error == UserTerminate ) break;
      int uc = handleFilesCopyException(id, e.error, e.strError);
      if ( uc ==  CancelOper ) break; 
      if ( uc ==  SkipOper ) i = skipFiles(files, i);
    }
  } 
  
  emit finished(id);
}

/*
Функция вызывается при возникновении ошибки копирования файла или создания каталога.
Функция посылает сигнал errorWaitingForIteration и ожидает ответа.
Возвращает выбор пользователя.
*/
int CopyJob::handleFilesCopyException(int id, CopyError error, const QString strError)
{
  QMutexLocker locker(&mutex);
  
  // CancelOper по умолчанию, для возможности прервать работу цикла копирования в любой момент
  interactionResult = CancelOper; 
  waitingForInteraction = true;
  emit errorWaitingForIteration(id, error, strError);
  interactionCondition.wait(&mutex);
  waitingForInteraction = false;
  
  return interactionResult;
}

/*
Функция подготавливает список файлов для копирования и подсчитивает их колличество. Содержимое
каталога строится в алфавитном порядке. (можно изменить с помощью переменной filesSort)
При работе функции возможно возбуждение исключения CopyExcepion. При обработки символической ссылки
в список копирования будет добавлен путь к файлу на который указывает эта ссылка
*/
int CopyJob::prepareCopy(int index, int level, const QString& sName, QString destDir, CopyInfoList& files)
{
  QFileInfo sourceInfo(sName);
  qint64 countFiles(0);
  
  CopyExcepion::throwIf(breakCurrentWork, UserTerminate, "User terminate");
  
  if ( !destDir.isEmpty() && !destDir.endsWith(QLatin1Char('/')) ) destDir += QLatin1Char('/');
  
  CopyExcepion::throwIf(!sourceInfo.exists(), SourceNotExists, sourceInfo.absoluteFilePath());
  
  if ( sourceInfo.isSymLink() )
    sourceInfo.setFile(sourceInfo.symLinkTarget());
  
  CopyInfo copyInfo;
  
  if ( sourceInfo.isFile() ){
    copyInfo.fill(sourceInfo.fileName(), sourceInfo.absoluteDir().absolutePath() + QLatin1Char('/'),
                  destDir, CopyInfo::FILE, sourceInfo.size(), index, level);
    files.append(copyInfo);
    countFiles++;
  }
  else 
    if ( sourceInfo.isDir() ){
      QDir sourceDir(sourceInfo.filePath());
          
      copyInfo.fill(sourceDir.dirName(), "", destDir, CopyInfo::DIR, 0, index, level);
      files.append(copyInfo);
      countFiles++;

      QFileInfoList filesInDir = sourceDir.entryInfoList(filesFilter, filesSort);
    
      for ( int i = 0; i < filesInDir.size(); i++ ){
        countFiles += prepareCopy(index, level + 1, filesInDir.at(i).absoluteFilePath(), 
                                  destDir + sourceDir.dirName(), files);
      }
    }
    else
      throw CopyExcepion(UnknownFileType, "Unknown file type");
    
  return countFiles;
}

/*
В зависимоти от типа элемента fileInfo, копирует файл или создает директорию.
Возможно возбуждение исключения CopyExcepion.
progressValue порядковый номер элемента для копирования
*/
void CopyJob::files_copy(int id, const CopyInfo& fileInfo, int progressValue)
{
  QDir destDir;
  QString sourcePathName = fileInfo.sSourcePath + fileInfo.sFileName;
  QString destPathName;
  
  emit startCopyFile(id, sourcePathName, fileInfo.size);
  switch ( fileInfo.type )
  {
    case CopyInfo::FILE:
      destPathName = fileInfo.sDestPath + fileInfo.sFileName;
      copy(id, sourcePathName, destPathName);
      break;
        
    case CopyInfo::DIR:
      destDir.setPath(fileInfo.sDestPath);
      CopyExcepion::throwIf(!destDir.exists(), DestDirNotExists, destDir.absolutePath());
      CopyExcepion::throwIf(!destDir.mkdir(fileInfo.sFileName), MakeDirError, fileInfo.sFileName);
      break;
      
    default:
        throw CopyExcepion(UnknownFileType, "Unknown file type");
   }
   emit endingCopyFile(id, progressValue);
}

/*
Копирует файл. Копирование производится черех временный файл, что позваляет удалить не полностью 
скопированный файл в случае ошибки. Возможно возбуждение исключения CopyExcepion.
*/
void CopyJob::copy(int id, const QString& fileName, const QString& newName)
{
  CopyExcepion::throwIf(fileName.isEmpty() || newName.isEmpty(), IncorectParam, "Internal error");

  bool bOk;
  QFile sourceFile(fileName);
  QString fileTemplate = QLatin1String("%1/cp_temp.XXXXXX");
  QTemporaryFile out(fileTemplate.arg(QFileInfo(newName).path()));
  
  if ( (bOk = sourceFile.exists()) )
  {
    qint64 fileSize = sourceFile.size();
    
    if ( (bOk = !QFile::exists(newName)) )
    {
      if ( (bOk = sourceFile.open(QIODevice::ReadOnly)) )
      {
        if ( (bOk = out.open()) )
        {
          char block[BLOCK_SIZE];
          qint64 totalRead = 0;
          
          while ( !sourceFile.atEnd() )
          {
            CopyExcepion::throwIf(breakCurrentWork, UserTerminate, "User terminate");
            qint64 in = sourceFile.read(block, BLOCK_SIZE);
            if ( in <= 0) break;
            if (in != out.write(block, in) ){
              bOk = false;
              break;
            }
            totalRead += in;
            emit copyProgress(id, 100*totalRead/fileSize);
          }
          
          bOk = (bOk && totalRead == sourceFile.size());
          bOk = (bOk && out.rename(newName));
        }
      }
    }
  }
  
  CopyExcepion::throwIf(!bOk, CopyFileError, fileName);
  out.setAutoRemove(false);
  QFile::setPermissions(newName, sourceFile.permissions());
}

// слот для взаимодействия с пользователем в случае ошибки
void CopyJob::setInteractionResult(UserChoice uc)
{
  QMutexLocker locker(&mutex);
  
  if ( waitingForInteraction ){
    interactionResult = uc;
    interactionCondition.wakeOne();
  }
}

void CopyJob::cancel()
{
  setInteractionResult(CancelOper);
}

void CopyJob::skip()
{
  setInteractionResult(SkipOper);
}

void CopyJob::retry()
{
  setInteractionResult(RetryOper);
}

void CopyJob::run()
{
  forever{
    mutex.lock();
    
    if ( stopWorking ){
      mutex.unlock();
      break;
    }
    
    if ( requestQueue.isEmpty() ){
      newCopyCondition.wait(&mutex);
      mutex.unlock();
    }
    else{
      breakCurrentWork = false;
      int id = requestQueue.constBegin().key();
      ReqInfo r = requestQueue[id];
      requestQueue.remove(id);
      mutex.unlock();
      
      handle(id, r.sourceFiles, r.destDir);
    }
  }
}
