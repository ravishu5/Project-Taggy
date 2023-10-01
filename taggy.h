#ifndef TAGGY_H
#define TAGGY_H

#define WINDOWS

#include <QUrl>
#include <QDebug>
#include <QMainWindow>
#include <QFileSystemWatcher>
#include <QFileSystemModel>
#include <QCompleter>
#include <QListWidgetItem>
#include <QInputDialog>
#include <QTimer>

#ifdef _MSC_VER
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#define SHMSZ 90000000
#define SHMKEY 1367
#endif //!_MSC_VER

QT_BEGIN_NAMESPACE
class QMimeData;

QT_END_NAMESPACE

#ifdef _MSC_VER
typedef struct
{

    char filetaggerdb[10000000];
    int filetaggerdb_size;
    char history[10000];
    int history_size;

}DATA ;
#endif
namespace Ui {
class Taggy;
}

class Taggy : public QMainWindow
{
    Q_OBJECT

public:
    explicit Taggy(QWidget *parent = 0);
    ~Taggy();

    void SAVEDATABASE();
    void OPENDATABASE();
    void UPDATE_BROWSELIST();
    void UPDATETAGLIST();
    void SORTFILELIST();
#ifdef _MSC_VER
    void WRITETOSHARED_DB();
    bool READFROMSHARED_DB();
#endif

private slots:
    void dropEvent(QDropEvent *ev) Q_DECL_OVERRIDE;
    void dragEnterEvent(QDragEnterEvent *ev) Q_DECL_OVERRIDE;
    void SAVE_TAG_ACTION();
    void ADDTODATABASE(QString data );
    void UPDATETAGDB(QStringList tags);
    void UPDATETAG();
    void AUTOCOMPLETETAG();
    void REMOVEFROMDATABASE();
    void OPEN_FILE();
    void exitapp();
    void Entertagfind_counterupdate();
    void Entertag_counterupdate();
    void timeexceeded();

    void on_filename_returnPressed();

    void on_ENTER_TAG_editingFinished();

    void on_ADD_TAG_clicked();

    void on_ENTER_TAG_FIND_returnPressed();

    void on_ENTER_TAG_FIND_textChanged(const QString &arg1);

    void on_REMOVETAG_clicked();

private:
    Ui::Taggy *ui;
    QFileSystemWatcher *watcher;
    QCompleter *completer;
    bool stoptagsorting;
    QString dbdir;
    bool activityover;
    int Entertag_counter;
    int Entertagfind_counter;
    QStringList DATABASE;
    QStringList PRESENTDATABASE;
    QStringList TAGS;

};

#endif // TAGGY_H
