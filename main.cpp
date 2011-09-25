#include <QApplication>
#include "mainwnd.h"

using namespace std;

int main(int ac, char** av)
{
  QApplication app(ac, av);
  MainWnd window;
  window.show();
  
  return app.exec();
}
