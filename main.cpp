#include "taggy.h"
#include <QApplication>
#include <QDebug>
QString FILE_ARG;
int main(int argc, char *argv[])
{
    FILE_ARG=argv[1];
    qDebug()<<FILE_ARG;
    QApplication a(argc, argv);
    Taggy w;
    w.show();

    return a.exec();
}
