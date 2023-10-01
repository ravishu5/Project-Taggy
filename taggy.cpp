#include <QThread>
#include <QTime>
#include "taggy.h"
#include "ui_taggy.h"
#include <QDir>
#include <QFileDialog>
#include <QMessageBox>
#include <QDebug>
#include <QDragEnterEvent>
#include <QMimeData>
#include <QDesktopServices>
#ifdef _MSC_VER
DATA *shared_mem;
#endif
extern QString FILE_ARG;
float wordmatching(const QString &wordq1,const QString &wordq2)
{
    char word1[200];
    char word2[200];
    strcpy(word1,wordq1.toStdString().c_str());
    strcpy(word2,wordq2.toStdString().c_str());

    int len1 = strlen(word1);
    int len2 = strlen(word2);
    //tolower
    for(i=0;i<len1;i++){
        if(word1[i]>=65&&word1[i]<=90)
            word1[i]=word1[i]+32;
    }
    //tolower
    for(i=0;i<len2;i++){
        if(word2[i]>=65&&word2[i]<=90)
            word2[i]=word2[i]+32;
    }

    vector<vector<int>> dp(len1 + 1, vector<int>(len2 + 1, 0));

    for (int i = 0; i <= len1; i++) {
        for (int j = 0; j <= len2; j++) {
            if (i == 0) {
                dp[i][j] = j;
            } else if (j == 0) {
                dp[i][j] = i;
            } else if (word1[i - 1] == word2[j - 1]) {
                dp[i][j] = dp[i - 1][j - 1];
            } else {
                dp[i][j] = 1 + min({dp[i - 1][j], dp[i][j - 1], dp[i - 1][j - 1]});
            }
        }
    }
    return dp[len1][len2];
}

Taggy::Taggy(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::Taggy)
{
    Entertag_counter=0;
    Entertagfind_counter=0;
    setAcceptDrops(true);
    ui->setupUi(this);
    ui->tabWidget->setTabText(0, "CREATE NEW TAGS");
    ui->tabWidget->setTabText(1, "BROWSE TAGS");
    stoptagsorting=false;
    ui->REMOVETAG->setEnabled(false);
    setWindowTitle("Taggy");
    ui->FILELIST_LABEL->setText("FILES/FOLDERS/WEBSITES  (CLICK TO OPEN)");

    if( FILE_ARG.size()<1){
        ui->tabWidget->setCurrentIndex(1);
    }else{
        ui->tabWidget->setCurrentIndex(0);
    }

    ui->filename->setText(FILE_ARG);
    dbdir= QDir::homePath();
    dbdir=dbdir+"/.Taggy"; //as a distinguishing factor to locate tagged files.

    QDir dir(dbdir);
    if (!dir.exists()) {
        dir.mkpath(".");
    }
    //lockfile
    activityover=false;
    QFile file(dbdir+"/.lockfile");
    file.open(QIODevice::WriteOnly|QIODevice::Truncate|QIODevice::Text);    //open is used to override over files.
    QTextStream out(&file);

    out<<QDateTime::currentDateTime().toString();
    watcher = new QFileSystemWatcher();
    watcher->addPath(dbdir+"/.lockfile");
#ifdef _MSC_VER
    key_t key=SHMKEY;int shmid =-1;
    shmid = shmget(key, SHMSZ, 0666);
    //   shmget() creates/gets a block of shared memory associated with the key.
    //   int shmget(key_t key, size_t size, int shmflg)
    //   Shared memory is the fastest form of interprocess communication.
    //   The main advantage of shared memory is that the copying of message data is eliminated.

    if (shmid<0)
    {
        qDebug()<<"no shared memory- creating shared memory";
        shmid = shmget(key, SHMSZ, IPC_CREAT|0666);
        shared_mem = (DATA * )shmat(shmid, NULL, 0);
        //shmat uses the block id shared by shmget to block into this process's addrress space
        //and return us a pointer to use that memory
        memset(shared_mem,0,SHMSZ);
        OPENDATABASE();

    } else {
        shared_mem = (DATA *)shmat(shmid, NULL, 0);

        qDebug()<<"shared memory already present";
        bool loaded=READFROMSHARED_DB();

         if (!loaded){
            qDebug()<<"shared memory corrupt";
             OPENDATABASE();
        }
    }
#else
    OPENDATABASE();
#endif
    connect(ui->ADD_TAG,SIGNAL(clicked()), this, SLOT(SAVE_TAG_ACTION()));
    connect(ui->ENTER_TAG,SIGNAL(textChanged(QString)), this, SLOT(Entertag_counterupdate()));
    connect(ui->filename,SIGNAL(textChanged(QString)), this, SLOT(UPDATETAG()));
    connect(ui->ENTER_TAG_FIND,SIGNAL(textChanged(QString)), this, SLOT(Entertagfind_counterupdate()));
    connect(ui->PREVIOUS_TAGS, SIGNAL(itemClicked(QListWidgetItem*)),this, SLOT(AUTOCOMPLETETAG()));
    connect(ui->FILE_LIST, SIGNAL(itemClicked(QListWidgetItem*)),this, SLOT(OPEN_FILE()));
    connect(ui->ENTER_TAG,SIGNAL(returnPressed()), this, SLOT(AUTOCOMPLETETAG()));
    connect(ui->REMOVETAG ,SIGNAL(clicked()), this, SLOT(REMOVEFROMDATABASE()));
    connect(watcher,SIGNAL(fileChanged(QString)),SLOT(exitapp()));

    UPDATETAG();
    QTimer *timer = new QTimer(this);
    connect(timer, SIGNAL(timeout()), this, SLOT(timeexceeded()));
    timer->start(300);
    completer = new QCompleter();

    QFileSystemModel *fsModel = new QFileSystemModel(completer);
    completer->setModel(fsModel);
    fsModel->setRootPath("");
    ui->filename->setCompleter(completer);

}
#ifdef _MSC_VER
void Taggy::WRITETOSHARED_DB()
{

    QString data;
    qDebug()<<"writing to shared data";
    for (int i = 0; i < DATABASE.size(); ++i)
    {
        data=data+DATABASE.at(i)+"\n";
    }
    strcpy(shared_mem->filetaggerdb,data.toStdString().c_str());
    shared_mem->filetaggerdb_size=DATABASE.size();

}

bool Taggy::READFROMSHARED_DB()
{
    bool loaded=false;
      DATABASE.clear();
      QString data=QString(shared_mem->filetaggerdb);
      qDebug()<<"reading from shared memory data";

          QStringList items =data.split("\n");
          for (int i = 0; i < items.size(); ++i)
          {

                       QRegularExpression rxi("#tags-:");
                       QStringList queryi = items.at(i).split(rxi);
                       if(queryi.size()==2)
                       {
                           QRegularExpression rx("(\\ |\\t)"); //RegEx for ' ' OR '\t'
                           QStringList query = queryi.at(1).split(rx);
                           UPDATETAGDB(query);
                           DATABASE<<items.at(i).trimmed();
                       }

              }

      if ( DATABASE.size()==shared_mem->filetaggerdb_size)
        {  loaded=true;}
      else
      { DATABASE.clear();}

      UPDATE_BROWSELIST();
      return loaded;
}
#endif
void Taggy::Entertag_counterupdate()
{
    Entertag_counter=2;
}
void Taggy::Entertagfind_counterupdate()
{
    Entertagfind_counter=2;
}
void Taggy::timeexceeded()
{
    if (Entertag_counter>0)
    {
        Entertag_counter=Entertag_counter-1;
        if (Entertag_counter==0)
            UPDATETAGLIST();

    }
    if (Entertagfind_counter>0)
    {
        Entertagfind_counter=Entertagfind_counter-1;

        if (Entertagfind_counter==0)
            SORTFILELIST();
    }
}

void Taggy::exitapp()
{
    if (activityover)
        exit(1);

    activityover=true;
}

void Taggy::dropEvent(QDropEvent *ev)
{

    const QMimeData *mimeData = ev->mimeData();
    if (mimeData->hasUrls()) {
        QList<QUrl> urlList = mimeData->urls();
        QString text = urlList.at(0).path();
        if( text.at(text.length()-1) == '/' ) text.remove( text.length()-1, 1 );
#ifdef WINDOWS
        text.replace("/","\\");
        if( text.at(0) == '\\' ) text=text.right(text.size()-1);
        if( text.at(text.length()-1) == '\\' ) text.remove( text.length()-1, 1 );


#endif
        ui->filename->setText(text);

        ui->tabWidget->setCurrentIndex(0);
        qDebug()<<text;
    }
}

void Taggy::dragEnterEvent(QDragEnterEvent *ev)
{
    ev->acceptProposedAction();
}

void Taggy::OPEN_FILE()
{

    int LISTindex=ui->FILE_LIST->currentRow()/2;
    ui->REMOVETAG->setEnabled(false);
    if (ui->FILE_LIST->currentRow()%2==1)
        ui->REMOVETAG->setEnabled(true);
    if (ui->FILE_LIST->currentRow()%2>0)
        return;

    QRegularExpression rx("#tags-:");

    QStringList queryl = DATABASE.at(LISTindex).split(rx);
    QString file=queryl.at(0);
    if(file.size()>4){
        QString subString=file.left(3);
        if (subString=="htt"){
            QDesktopServices::openUrl(QUrl(file));

        } else{
            if (QDir(file).exists()||QFile(file).exists()) {
                QDesktopServices::openUrl(QUrl::fromLocalFile(file));
            } else {
                qDebug()<<"cannot open path";
                QMessageBox::StandardButton reply = QMessageBox::question(this, "File/folder missing ", file+" missing\n Is it a file ?",
                                                                          QMessageBox::Yes|QMessageBox::No);
                QString filenamenew;
                if (reply == QMessageBox::Yes) {
                    filenamenew =  QFileDialog::getOpenFileName(this,
                                                                tr("Choose File"), QDir::homePath());

                }else if (reply == QMessageBox::No){
                    filenamenew= QFileDialog::getExistingDirectory(0,"Choose Directory",QString(),QFileDialog::ShowDirsOnly);
                }else {
                    return ;
                }
#ifdef WINDOWS
                filenamenew.replace("/","\\");
                file.replace("/","\\");
#endif

                qDebug()<<filenamenew;
                if (filenamenew.size()<3)
                    return;
                /* reassigning file path */
                int sameupto=-1;bool same=true;
                for(int i=filenamenew.size()-1;i>=0;i--)
                {
                    if((file.size()-(filenamenew.size()-i))>=0) {
                        if ((filenamenew[i]==file[file.size()-(filenamenew.size()-i)])&&same)
                        {
                            sameupto=i;
                        }else{
                            same=false;
                        }
                    }
                }
                if(sameupto==-1)
                {  DATABASE<<filenamenew+"#tags-:"+queryl.at(1);
//                    ui->history->addItem("Added "+filenamenew+"#tags-:"+queryl.at(1));
//                    ui->history->scrollToBottom();
//                    HISTORY<<"Added "+filenamenew+"#tags-:"+queryl.at(1);
                    ui->tabWidget->setCurrentIndex(0);
                    DATABASE.removeAt(LISTindex);
                    SAVEDATABASE();
                    UPDATE_BROWSELIST();
                    return;
                }
                QString orginalpath=file.left(file.size()-(filenamenew.size()-sameupto)+1);
                qDebug()<<"orgp "+orginalpath;
                QString newpath=filenamenew.left(sameupto+1);
                qDebug()<<"newp "+newpath;
                QRegularExpression rx("#tags-:");
                QStringList NEWDATABASE;
                for (int i = 0; i < DATABASE.size(); ++i)
                {
                    QStringList queryi = DATABASE.at(i).split(rx);

                    ui->FILE_LIST->addItem(queryi.at(0) +" ["+queryi.at(1)+"]");
                    if(queryi.at(0).size()>4){
                        QString subString=queryi.at(0).left(3);
                        if (subString=="htt"){
                            qDebug()<<"is a http link";
                            NEWDATABASE<<DATABASE.at(i);
                        } else{
                            if (!QDir(queryi.at(0)).exists()&& !QFile(queryi.at(0)).exists()) {
                                QString oldfilename=queryi.at(0);
#ifdef WINDOWS
                                oldfilename.replace("/","\\");
#endif
                                oldfilename.replace(orginalpath,newpath);
                                qDebug()<<"new "+oldfilename;
                                if (QDir(oldfilename).exists()|| QFile(oldfilename).exists()) {
                                    qDebug()<<"Doesnot exist, so renaming "+queryi.at(0)+" to " +oldfilename;
                                    ui->tabWidget->setCurrentIndex(0);
                                    NEWDATABASE<<oldfilename+"#tags-:"+queryi.at(1);
                                } else {
                                    NEWDATABASE<<DATABASE.at(i);
                                }


                            } else {
                                NEWDATABASE<<DATABASE.at(i);
                            }
                        }
                    }

                }
                DATABASE.clear();
                DATABASE<<NEWDATABASE;
                SAVEDATABASE();
                UPDATE_BROWSELIST();
            }
        }
    }

}



void Taggy::SAVEDATABASE()
{

    ui->ENTER_TAG_FIND->setText("");
    QFile file(dbdir+"/Taggy_db");
    file.open(QIODevice::WriteOnly|QIODevice::Truncate|QIODevice::Text);
    QTextStream out(&file);
    for (int i = 0; i < DATABASE.size(); ++i)
    {
        out<<DATABASE.at(i)+"\n";
    }

#ifdef _MSC_VER
    WRITETOSHARED_DB();
#endif

}
void Taggy::OPENDATABASE()
{
    DATABASE.clear();
    QFile file(dbdir+"/Taggy_db");
    qDebug()<<dbdir;
    QString data;
    if (file.open(QFile::ReadOnly))
    {
        data = file.readAll();
        file.close();
        QStringList items =data.split("\n");
        for (int i = 0; i < items.size(); ++i)
        {
            if (items.at(i).size()>0)
            {
                bool present=false;
                for(int j=0;j<DATABASE.size();j++)
                {
                    if (DATABASE.at(j)==items.at(i).trimmed())
                        present=true;
                }
                if(!present)
                {
                    QRegularExpression rxi("-:");
                    QStringList queryi = items.at(i).split(rxi);
                    if(queryi.size()==2)
                    {
                        QRegularExpression rx("(\\ |\\t)"); //RegEx for ' ' OR '\t'
                        QStringList query = queryi.at(1).split(rx);
                        UPDATETAGDB(query);
                        DATABASE<<items.at(i).trimmed();
                    }
                }
            }
        }

    }
    UPDATE_BROWSELIST();
#ifdef _MSC_VER
    WRITETOSHARED_DB();
#endif
}


void Taggy::UPDATE_BROWSELIST()
{
    ui->FILE_LIST->clear();
    ui->REMOVETAG->setEnabled(false);
    QRegularExpression rx("#tags-:");

    for (int i = 0; i < DATABASE.size(); ++i)
    {
        QStringList queryi = DATABASE.at(i).split(rx);
        ui->FILE_LIST->addItem(queryi.at(0));
        ui->FILE_LIST->addItem("Tags-: "+queryi.at(1)+"\n");
        ui->FILE_LIST->item(ui->FILE_LIST->count()-1)->setForeground(*(new QBrush(Qt::darkGreen)));
    }
    ui->FILE_LIST->scrollToTop();
}

void Taggy::AUTOCOMPLETETAG()
{
    if(ui->PREVIOUS_TAGS->count()<=0.0)
        return;
    QRegularExpression rx("(\\ |\\t)");
    QString tag= ui->ENTER_TAG->text().trimmed();
    QStringList query = tag.split(rx);
    QString finaltag;
    for(int j=0;j<query.size()-1;j++)
    {
        finaltag=finaltag+query.at(j)+" ";
    }
    int LISTindex=ui->PREVIOUS_TAGS->currentRow();
    if(LISTindex>=0 && LISTindex<TAGS.size() )
    {
        finaltag=finaltag+TAGS.at(LISTindex);
    }else if(TAGS.size() >0) {
        finaltag=finaltag+TAGS.at(0)+" ";
    }
    stoptagsorting=true;
    ui->ENTER_TAG->setText(finaltag);
    stoptagsorting=false;
}

void  Taggy::UPDATETAG()
{
    QString tag;
    QString filename=ui->filename->text().trimmed();
    if (filename.size()<1){
        return;
    }

    filename.replace("file:///","/");

    if( filename.at(filename.length()-1) == '/' ) filename.remove( filename.length()-1, 1 );
#ifdef WINDOWS
    if( filename.at(filename.length()-1) == '\\' ) filename.remove( filename.length()-1, 1 );
    bool ishttp=false;
    if(filename.size()>4){
        QString subString=filename.left(3);
        if (subString=="htt")
            ishttp=true;
    }
    if (!ishttp)
        filename.replace("/","\\");
#endif
    QRegularExpression rx("#tags-:");
    for(int j=0;j<DATABASE.size();j++)
    {
        QString Line=DATABASE.at(j);

        QStringList query = Line.split(rx);
        if (query.size()==2){
            if(query.at(0).trimmed()==filename.trimmed())
            {
                tag=query.at(1);
            }
        }

    }
    ui->ENTER_TAG->setText(tag);
}

void  Taggy::SORTFILELIST()
{
    ui->PREVIOUS_TAGS->clear();
    QString find= ui->ENTER_TAG_FIND->text();
    float *score=new float[DATABASE.size()];

    QRegularExpression rxdb("#tags-:");
    QRegularExpression rx("(\\ |\\t)");
    QStringList tags = find.split(rx);

    for (int i = 0; i < DATABASE.size();i++)
    {
        QStringList dbtaglist = DATABASE.at(i).split(rxdb);
        QStringList dbtags=dbtaglist.at(1).split(rx);
        score[i]=0.0;
        for (int j = 0; j < tags.size(); j++)
        {
            for (int k = 0; k < dbtags.size(); k++)
            {
                if (tags.at(j).trimmed().size()>0)
                {
                    float scoring=wordmatching(dbtags.at(k),tags.at(j))/tags.at(j).size();
                    ///taking weight  of highly matching tag
                    if (score[i]<scoring)
                        score[i]=scoring;
                }
            }
        }
    }

    if (find.length()>0){

        QString replacest;
        float scoretmp;

        for (int i = 0; i < DATABASE.size()-1; i++)
        {
            for (int j = i+1; j < DATABASE.size(); j++)
            {
                if (score[j] > score[i])
                {
                    replacest=DATABASE.at(j);
                    DATABASE.replace(j,DATABASE.at(i) );
                    DATABASE.replace(i,replacest );
                    scoretmp=score[j];
                    score[j]=score[i];
                    score[i]=scoretmp;
                }
            }
        }
        //update BROWSE_LIST IN SECOND TAB
        UPDATE_BROWSELIST();
    }
}
void  Taggy::UPDATETAGLIST()
{

    if (stoptagsorting)
        return;
    QString tag= ui->ENTER_TAG->text();
    QString find;
    ui->PREVIOUS_TAGS->clear();

    if (tag.length()>0){
        QRegularExpression rx("(\\ |\\t)");
        QStringList query = tag.split(rx);
        int i=query.size();
        find=query.at(i-1);
        float *score=new float[TAGS.size()];
        for (int j = 0; j < TAGS.size(); ++j)
        {
            score[j]=wordmatching(TAGS.at(j),find);
        }
        float scoretmp;
        QString replacest;
        for (int i = 0; i < TAGS.size()-1; ++i)
        {
            for (int j = i+1; j < TAGS.size(); ++j)
            {
                if (score[j] > score[i])
                {
                    replacest=TAGS.at(j);
                    TAGS.replace(j,TAGS.at(i) );
                    TAGS.replace(i,replacest );
                    scoretmp=score[j];
                    score[j]=score[i];
                    score[i]=scoretmp;
                }

            }
        }

        for (int i = 0; i < TAGS.size(); ++i)
        {
            if( score[i]>0)
            {
                QListWidgetItem *item = new QListWidgetItem();
                item->setData(Qt::DisplayRole, TAGS.at(i));
                item->setTextAlignment(Qt::AlignRight);
                ui->PREVIOUS_TAGS->addItem(item);
                ui->PREVIOUS_TAGS->item(ui->PREVIOUS_TAGS->count()-1)->setForeground(*(new QBrush(Qt::darkGreen)));

            }

        }
        ui->PREVIOUS_TAGS->scrollToTop();
    }
}
void  Taggy::UPDATETAGDB(QStringList tags)
{

    for(int j=0;j<tags.size();j++)
    {
        bool found=false;
        for(int i=0;i<TAGS.size();i++)
        {
            if(TAGS.at(i).trimmed()==tags.at(j).trimmed()){
                found=true;
            }
        }
        if (!found && tags.at(j).trimmed().size()>1 ) {
            TAGS<<tags.at(j).trimmed();
        }
    }
}
void Taggy::REMOVEFROMDATABASE()
{
    QRegularExpression rx("#tags-:");
      int LISTindex=ui->FILE_LIST->currentRow()/2;

      if( LISTindex>=0 && LISTindex<DATABASE.size())
      {
          QMessageBox::StandardButton reply;
          QStringList queryd = DATABASE.at(LISTindex).split(rx);
          reply = QMessageBox::question(this, "Confirmation", queryd.at(0)+"\n Do you want to Remove Tags?",
                                        QMessageBox::Yes|QMessageBox::No);
          if (reply == QMessageBox::Yes) {
              DATABASE.removeAt(LISTindex);
              ui->tabWidget->setCurrentIndex(0);
              SAVEDATABASE();
              //update BROWSE_LIST IN SECOND TAB
              UPDATE_BROWSELIST();
          } else {
              qDebug() << "Yes was *not* clicked";
          }
      }
}

void Taggy::ADDTODATABASE(QString data)
{
    QRegularExpression rx("#tags-:");
    QStringList queryd = data.split(rx);
    int remove=-1;
    for(int j=0;j<DATABASE.size();j++)
    {
        QString Line=DATABASE.at(j);

        QStringList query = Line.split(rx);
        if (query.size()==2){
            if(query.at(0).trimmed()==queryd.at(0).trimmed())
            {
                remove=j;
            }
        }

    }
    QString prefix="Added ";
    if (remove>=0)
    {    DATABASE.removeAt(remove);
        prefix="Modified ";
    }

    DATABASE<<data;
    ui->tabWidget->setCurrentIndex(0);
    //update BROWSE_LIST IN SECOND TAB
    SAVEDATABASE();
    UPDATE_BROWSELIST();
}
void Taggy::SAVE_TAG_ACTION()
{
    //OPENDATABASE();
    QString NEW_TAG= ui->ENTER_TAG->text().trimmed();
    QString file=ui->filename->text().trimmed();
    bool ishttp=false;
    if(file.size()<2)
    {
        QMessageBox messageBox;
        messageBox.critical(0,"Error","Give File/Folder name");
        messageBox.setFixedSize(500,200);
        return;
    }
    file.replace("file:///","/");
    if( file.at(file.length()-1) == '/' ) file.remove( file.length()-1, 1 );
    if(file.size()>4){
        QString subString=file.left(3);
        if (subString=="htt")
            ishttp=true;
    }
#ifdef WINDOWS
    if( file.at(file.length()-1) == '\\' ) file.remove( file.length()-1, 1 );
    if(!ishttp)
        file.replace("/","\\");
#endif

    if ((!QDir(file).exists()&&!QFile(file).exists())&&!ishttp)
    {
        QMessageBox messageBox;
        messageBox.critical(0,"Error","File/Folder does not exists");
        messageBox.setFixedSize(500,200);
        return;
    }
    if ( NEW_TAG.count(QRegularExpression("#tags-:"))>0)
    {
        QMessageBox messageBox;
        messageBox.critical(0,"Error","Tag contains string #tags-:");
        messageBox.setFixedSize(500,200);
        return;
    }
    QRegularExpression rx("(\\ |\\t)"); //RegEx for ' ' OR '\t'
    QStringList query = NEW_TAG.split(rx);
    int i=query.size();
    for(int j=0;j<i;j++)
    {

        QString STRING= query.at(j);
        if (NEW_TAG.size()<2) {
            QMessageBox messageBox;
            messageBox.critical(0,"Error","Tag is too small");
            messageBox.setFixedSize(500,200);

            return;
        }
        if ( STRING.count(QRegularExpression("[a-zA-Z0-9]"))==0)
        {
            if (STRING.size()>1){
                QMessageBox messageBox;
                messageBox.critical(0,"Error","Tags should contain atleast one alphabet or number");
                messageBox.setFixedSize(500,200);

                return;
            }
        }
    }

    UPDATETAGDB(query);


    if (QDir(file).exists()||QFile(file).exists()||ishttp) {
        ADDTODATABASE(file+"#tags-:"+NEW_TAG);
    }
}

Taggy::~Taggy()
{
    delete ui;
}

void Taggy::on_filename_returnPressed()
{

}

void Taggy::on_ENTER_TAG_editingFinished()
{

}

void Taggy::on_ADD_TAG_clicked()
{

}

void Taggy::on_ENTER_TAG_FIND_returnPressed()
{
    SORTFILELIST();
}
void Taggy::on_ENTER_TAG_FIND_textChanged(const QString &arg1)
{

}


void Taggy::on_REMOVETAG_clicked()
{

}
