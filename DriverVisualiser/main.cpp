#include "mainwindow.h"

#include <QApplication>

#include <io.h>
#include <fcntl.h>

#include "mainwindow.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    MainWindow w;
    w.show();
    return a.exec(); // The program stays here until the window is closed
}
