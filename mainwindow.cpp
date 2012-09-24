#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QThread>
#include <QDesktopServices>
#include <QTcpSocket>
#include <QUrl>
#include <list>
#include <algorithm>
#include <map>

#ifdef WIN32
#include <windows.h>
#endif

#define BOMB_WITH_WARNING(x) {\
emit updateMessage(QString("Warning: ") + QString(x));\
emit updateMessage("worker complete");\
return;\
}

using std::cout;
using std::cerr;
using std::list;

using std::vector;
using std::count;
using std::map;
using std::make_pair;
using std::sort;
using std::string;
using std::find;
using std::not1;
using std::ptr_fun;

namespace {

inline bool is_hex(const char c)
{
    return ((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f')) ? true : false;
}

vector<string> tokenize_query_string(const string& input)
{
    vector<string> rv;

    string::const_iterator b = input.begin() + 6;
    string::const_iterator e = find(b, input.end(), ' ');
    while (b != input.end())
    {
        string hex(b, e);
        hex.erase(remove_if(hex.begin(), hex.end(), not1(ptr_fun(is_hex))), hex.end());
        if (32 == hex.size())
            rv.push_back(hex);

        if (e != input.end())
            b = e + 1;
        else
            b = e;

        if (b != input.end())
        {
            e = find(b, input.end(), ' ');
        }
    }
    return rv;
}
}

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow),
    hasUnsavedData(false),
    saveAsFilename(),
    worker(0)
{
    ui->setupUi(this);
    connect(ui->actionAbout_Duffy, SIGNAL(triggered()), this, SLOT(doActionAbout()));
    connect(ui->actionClose, SIGNAL(triggered()), this, SLOT(doActionClose()));
    connect(ui->actionHelp_index, SIGNAL(triggered()), this, SLOT(doActionHelpIndex()));
    connect(ui->actionNew, SIGNAL(triggered()), this, SLOT(doActionNew()));
    connect(ui->actionOpen, SIGNAL(triggered()), this, SLOT(doActionOpen()));
    connect(ui->actionQuit, SIGNAL(triggered()), this, SLOT(doActionQuit()));
    connect(ui->actionSave, SIGNAL(triggered()), this, SLOT(doActionSave()));
    connect(ui->actionSave_as, SIGNAL(triggered()), this, SLOT(doActionSaveAs()));
    connect(ui->knownToggle, SIGNAL(activated(int)), SLOT(toggleClicked(int)));
    connect(ui->fileType, SIGNAL(activated(int)), SLOT(toggleClicked(int)));
    connect(ui->dateEdit, SIGNAL(dateTimeChanged(QDateTime)), SLOT(dateEditChanged(QDateTime)));
    connect(ui->treeWidget, SIGNAL(itemDoubleClicked(QTreeWidgetItem*,int)),
            SLOT(itemDoubleClicked(QTreeWidgetItem*, int)));
    updateUI();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::updateMessage(QString message)
{
    if (message == "worker complete")
    {
        worker->wait();
        delete worker;
        worker = 0;

        ui->actionNew->setEnabled(true);
        ui->actionClose->setEnabled((files.size() > 0) ? true : false);
        ui->actionOpen->setEnabled(true);
        ui->actionSave->setEnabled((files.size() > 0) ? true : false);
        ui->actionSave_as->setEnabled((files.size() > 0) ? true : false);

        hasUnsavedData = (files.size() > 0) ? true : false;
    }
    else
    {
        statusBar()->showMessage(message);
    }
}

/** Callback for double clicking on a row in the QTreeWidget.
  *
  * Side effects: attempts to open a remote URL, updates the UI.
  *
  * Error handling: none
  *
  * Postcondition: none
  *
  * @pre item must not be null
  * @return void
  */
void MainWindow::itemDoubleClicked(QTreeWidgetItem *item, int)
{
    if (0 == item)
    {
        updateUI();
        return;
    }
    if (item->text(2).isNull() || item->text(2) == "")
    {
        return;
    }
    QUrl url("https://www.google.com/search?as_q=" + item->text(2));
    QDesktopServices::openUrl(url);
}

/** Callback for changing the date within the QDateEdit widget.
  *
  * Side effects: updates the UI.
  *
  * Error handling: none
  *
  * Postcondition: none
  *
  * @pre None
  * @return void
  */
void MainWindow::dateEditChanged(QDateTime)
{
    this->updateUI();
}

/** Callback for clicking on either of the two combo boxes.
  *
  * Side effects: updates the UI.
  *
  * Error handling: none
  *
  * Postcondition: none
  *
  * @pre None
  * @return void
  */
void MainWindow::toggleClicked(int)
{
    this->updateUI();
}

/** Updates the user interface to reflect the current state
  * of the application.
  *
  * Side effects: updates the UI
  *
  * Error handling: none
  *
  * Postcondition: none
  *
  * @pre None
  * @return void
  */
void MainWindow::updateUI()
{
    if (0 != files.size())
    {
        ui->actionClose->setEnabled(true);
        ui->actionSave->setEnabled(true);
        ui->actionSave_as->setEnabled(true);
    } else {
        ui->actionClose->setEnabled(false);
        ui->actionSave->setEnabled(false);
        ui->actionSave_as->setEnabled(false);
    }
    updateTreeWidget();
}

/** Displays information about the application.
  *
  * Side effects: displays a window
  *
  * Error handling: none
  *
  * Postcondition: none
  *
  * @pre None
  * @return void
  */
void MainWindow::doActionAbout()
{
    QMessageBox::about(this, "About Sgt. Duffy",
                       "Sgt. Duffy 0.9.9 is (c) 2012, Robert J. Hansen <rjh@sixdemonbag.org>.\n\n"
                       "You are free to use, share and modify this program in accordance with "
                       "the ISC License.\n\n"
                       "Sgt. Duffy is named after the intrepid analyst from Infocom's series "
                       "of classic mystery games.\n\n"
                       "Good luck, and good hunting!");
    return;
}

void MainWindow::doActionClose()
{
    if (0 == ui->treeWidget->topLevelItemCount())
        return;
    if (hasUnsavedData)
    {
        QMessageBox::StandardButton btn;

        btn = QMessageBox::question(this,
                                    "",
                                    "Would you like to save this analysis?",
                                    QMessageBox::Yes|QMessageBox::No|QMessageBox::Cancel,
                                    QMessageBox::Yes);
        if (btn == QMessageBox::Yes)
        {
            doActionSave();
        }
        if (btn == QMessageBox::Cancel)
        {
            return;
        }
        files.clear();
        ui->treeWidget->clear();
        saveAsFilename = QString();
        hasUnsavedData = false;
        updateUI();
    }
}

void MainWindow::doActionHelpIndex()
{
    QUrl url("http://keyservers.org/~rjh/duffy");
    QDesktopServices::openUrl(url);
    return;
}

void MainWindow::doActionNew()
{
    if (hasUnsavedData)
    {
        {
            QMessageBox::StandardButton btn;

            btn = QMessageBox::question(this,
                                        "",
                                        "Would you like to save this analysis?",
                                        QMessageBox::Yes|QMessageBox::No|QMessageBox::Cancel,
                                        QMessageBox::Yes);
            if (btn == QMessageBox::Yes)
            {
                doActionSave();
            }
            if (btn == QMessageBox::Cancel)
            {
                return;
            }
        }
    }
    QString path = QFileDialog::getExistingDirectory(this, "Choose a directory", QDir::homePath());

    if (path.isNull() || path == "")
    {
        return;
    }

    files.clear();
    ui->treeWidget->clear();
    hasUnsavedData = false;
    updateUI();

    ui->actionNew->setEnabled(false);
    ui->actionClose->setEnabled(false);
    ui->actionOpen->setEnabled(false);
    ui->actionSave->setEnabled(false);
    ui->actionSave_as->setEnabled(false);

    worker = new Worker(path);
    worker->start();
    connect(worker, SIGNAL(updateMessage(QString)), this, SLOT(updateMessage(QString)));
    connect(worker, SIGNAL(updateFileMetaInformation(std::vector<FileMetaInformation>*)), this, SLOT(updateFileMetaInformation(std::vector<FileMetaInformation>*)));

    return;
}

void MainWindow::updateFileMetaInformation(std::vector<FileMetaInformation>* ptr)
{
    files = *ptr;
    delete ptr;
    updateTreeWidget();
}

void MainWindow::doActionOpen()
{
    if (hasUnsavedData)
    {
        QMessageBox::StandardButton btn;

        btn = QMessageBox::question(this,
                                    "",
                                    "Would you like to save this analysis?",
                                    QMessageBox::Yes|QMessageBox::No|QMessageBox::Cancel,
                                    QMessageBox::Yes);
        if (btn == QMessageBox::Yes)
        {
            doActionSave();
        }
        if (btn == QMessageBox::Cancel)
        {
            return;
        }
    }
    files.clear();
    ui->treeWidget->clear();
    hasUnsavedData = false;
    updateUI();
    QString filename = QFileDialog::getOpenFileName(this,
                                                    "Open a Duffy file",
                                                    QString(),
                                                    "Duffy files (*.dfy)");
    if (filename.isNull())
    {
        return;
    }
    saveAsFilename = filename;
    QFile infile(saveAsFilename);
    if (! infile.open(QFile::ReadOnly|QFile::Text))
    {
        QMessageBox::warning(this, "Error", "A disk error occurred.", QMessageBox::Ok, QMessageBox::Ok);
        return;
    }
    files.clear();
    while (! infile.atEnd())
    {
        QString fn(QString::fromAscii(infile.readLine()));
        QString hash(QString::fromAscii(infile.readLine()));
        QString date(QString::fromAscii(infile.readLine()));
        QString inNSRL(QString::fromAscii(infile.readLine()));

        fn = fn.left(fn.size() - 1);
        hash = hash.left(hash.size() - 1);
        date = date.left(date.size() - 1);
        inNSRL = inNSRL.left(1);

        files.push_back(FileMetaInformation(fn, hash, date, inNSRL));
    }
    updateTreeWidget();
    updateUI();
}

void MainWindow::doActionQuit()
{
    qApp->quit();
    return;
}

void MainWindow::doActionSave()
{
    if (files.size() == 0)
    {
        return;
    }

    if (saveAsFilename.isNull() || saveAsFilename == "")
    {
        doActionSaveAs();
        return;
    }
    QFile file(saveAsFilename);
    if (! file.open(QIODevice::WriteOnly|QIODevice::Text))
    {
        QMessageBox::warning(this,
                             "Couldn't save file",
                             "A disk error of some sort occurred.",
                             QMessageBox::Ok,
                             QMessageBox::Ok);
        saveAsFilename = "";
    }
    for (size_t i = 0 ; i < files.size() ; i++) {
        const FileMetaInformation& fmi = files[i];
        file.write(fmi.filename().toAscii());
        file.write("\n");
        file.write(fmi.hash().toAscii());
        file.write("\n");
        file.write(fmi.modified().toString("MM dd yyyy").toAscii());
        file.write("\n");
        file.write(fmi.inNSRL() ? "true" : "false");
        file.write("\n");
    }
    hasUnsavedData = false;
}

void MainWindow::doActionSaveAs()
{
    saveAsFilename = QFileDialog::getSaveFileName(this, "Save as", QString(), "Duffy files (*.dfy)");
    if (!saveAsFilename.isNull()) doActionSave();
}

void MainWindow::updateTreeWidget()
{
    vector<FileMetaInformation>::iterator viter = files.begin();
    itemMap.clear();
    ui->treeWidget->clear();

    bool show_unknowns = (ui->knownToggle->currentIndex()) ? true : false;
    bool showAllFiles = (ui->fileType->currentIndex()) ? false : true;
    QDateTime since = (ui->dateEdit->dateTime());

    for ( ; viter != files.end() ; ++viter)
    {
        const FileMetaInformation& fmi = *viter;
        const QDateTime& modified = fmi.modified();
        const QString& fn = fmi.filename();
        const QString& hash = fmi.hash();
        bool is_unknown = ! fmi.inNSRL();

        if ((is_unknown != show_unknowns) || (modified < since))
        {
            continue;
        }

        QStringList pathParts = fn.split(QDir::separator());
        QString bareName = pathParts.last();
        QString extension = bareName.split(".").last();

        bool isExecutable = (extension == "EXE") ||
                (extension == "exe") ||
                (extension == "DLL") ||
                (extension == "dll") ||
                (extension == "COM") ||
                (extension == "com");
        if (!bareName.isNull() && bareName != "")
        {
            if (! showAllFiles && !isExecutable)
            {
                continue;
            }
        }

        pathParts.removeLast();
        QString justThePath = pathParts.join(QDir::separator()) + QDir::separator();
        QTreeWidgetItem* pathItem = getItemForPath(justThePath);
        QTreeWidgetItem* item = new QTreeWidgetItem(pathItem);
        item->setText(0, bareName);
        item->setText(1, modified.toString("MMM d, yyyy"));
        item->setText(2, hash);
    }
    ui->treeWidget->expandAll();
    ui->treeWidget->resizeColumnToContents(0);
    ui->treeWidget->resizeColumnToContents(1);
    ui->treeWidget->resizeColumnToContents(2);
}

QTreeWidgetItem* MainWindow::getItemForPath(QString path)
{
    if (path.right(1) != QString(QDir::separator()))
    {
        path += QDir::separator();
    }
    QStringList pathParts = path.split(QDir::separator());
    // drop the null element on the end
    pathParts.removeLast();

    if (itemMap.end() != itemMap.find(path))
    {
        return itemMap[path];
    }

    if (pathParts.size() == 1)
    {
        QTreeWidgetItem* qti = new QTreeWidgetItem(ui->treeWidget);
        qti->setText(0, path);
        ui->treeWidget->addTopLevelItem(qti);
        itemMap.insert(make_pair(path, qti));
        return qti;
    }

    QString parentPath = "";
    for (int i = 0 ; i < pathParts.size() - 1 ; i += 1)
    {
        parentPath += pathParts[i] + QDir::separator();
    }
    QTreeWidgetItem* parentItem = getItemForPath(parentPath);
    QTreeWidgetItem* thisItem = new QTreeWidgetItem(parentItem);
    thisItem->setText(0, pathParts.last() + QDir::separator());
    itemMap.insert(make_pair(pathParts.join(QDir::separator()) + QDir::separator(), thisItem));
    return thisItem;
}

QStringList Worker::getFilenames()
{
    QStringList rv;
    list<QString> scanlist;
    QStringList dirlist;
    list<QString>::iterator iter;
    QStringList::iterator iter2;
    scanlist.push_back(root);

    for (iter = scanlist.begin() ; iter != scanlist.end() ; ++iter)
    {
        QDir currentDir(*iter);
        currentDir.setFilter(QDir::Dirs | QDir::NoSymLinks | QDir::NoDotAndDotDot | QDir::Readable | QDir::Hidden);
        dirlist = currentDir.entryList();

        for (iter2 = dirlist.begin() ; iter2 != dirlist.end() ; ++iter2)
        {
            // Qt guarantees that it will interpolate forward slashes to an OS-appropriate
            // path separator.  I don't personally believe them.  :)
            QString path = *iter + QDir::separator() + *iter2;
            scanlist.push_back(path);
        }
    }

    scanlist.sort();

    for (iter = scanlist.begin() ; iter != scanlist.end() ; ++iter)
    {
        QDir currentDir(*iter);
        currentDir.setFilter(QDir::Files | QDir::NoSymLinks | QDir::NoDotAndDotDot | QDir::Readable | QDir::Hidden);
        dirlist = currentDir.entryList();
        for (iter2 = dirlist.begin() ; iter2 != dirlist.end() ; ++iter2)
        {
            QString path = *iter + QDir::separator() + *iter2;
            rv.push_back(path);
        }
    }

    return rv;
}


void Worker::run()
{
    // Part 1: the easy stuff.  Recursively walk the directory and compute MD5 hashes.
    emit updateMessage(QString("Please wait ... counting files"));
    QStringList files = getFilenames();

    vector<FileMetaInformation>* fmi = new vector<FileMetaInformation>();
    emit updateMessage("0 of " + QString::number(files.size()) + " files processed");
    for (QStringList::iterator iter = files.begin() ; iter != files.end() ; ++iter)
    {
        FileMetaInformation thingy(*iter);
        fmi->push_back(thingy);
        if (0 == (fmi->size() % 10))
            emit updateMessage(QString::number(fmi->size()) + " of " + QString::number(files.size()) + " files processed");
    }


    // Part 2: pain begins here.
    map<QString, bool> hashmap;
    for (vector<FileMetaInformation>::iterator iter = fmi->begin() ;
         iter != fmi->end() ; ++iter)
    {
        hashmap[iter->hash()] = false;
    }

    vector<std::string> hashes;
    for (map<QString, bool>::iterator iter = hashmap.begin() ; iter != hashmap.end() ; ++iter)
    {
        if (iter->first.size() != 32 && iter->first.size() != 40) BOMB_WITH_WARNING("internal consistency error #1");
        hashes.push_back(iter->first.toStdString());
    }
    sort(hashes.begin(), hashes.end());

    vector<std::string> query_strings;
    std::string query_string = "query";
    for (size_t idx = 0 ; idx < hashes.size() ; ++idx)
    {
        query_string = query_string + " " + hashes.at(idx);
        if ((idx > 0) && (0 == (idx % 1000)))
        {
            query_strings.push_back(query_string + "\r\n");
            query_string = "query";
        }
    }
    if ("query" != query_string)
    {
        query_strings.push_back(query_string + "\r\n");
    }


    emit updateMessage("Contacting NSRL server at nsrl.kyr.us...");

    QTcpSocket sock;
    sock.connectToHost("nsrl.kyr.us", 9120);
    if (false == sock.waitForConnected(5000)) BOMB_WITH_WARNING("could not connect to NSRL server");

    sock.write("Version: 2.0\r\n");
    if (false == sock.waitForReadyRead(5000)) BOMB_WITH_WARNING("NSRL server timed out");
    QString qline = QString::fromLatin1(sock.readLine());
    if ("OK" != qline.left(2)) BOMB_WITH_WARNING("NSRL server is out of date");

    for (size_t idx = 0 ; idx < query_strings.size() ; ++idx)
    {
        hashes = tokenize_query_string(query_strings.at(idx));
        sock.write(query_strings.at(idx).c_str());

        if (false == sock.waitForReadyRead(5000)) BOMB_WITH_WARNING("NSRL server timed out");
        string tmp = QString::fromLatin1(sock.readLine()).toStdString();
        string status(tmp.begin(), tmp.begin() + 2);
        if ("OK" != status) BOMB_WITH_WARNING("NSRL server gave an unexpected response");

        string result(tmp.begin() + 3, tmp.end() - 2); // chop off the \r\n
        if (result.size() != hashes.size()) BOMB_WITH_WARNING("NSRL server gave incomplete data");

        for (size_t idx2 = 0 ; idx2 < hashes.size() ; ++idx2)
        {
            string hash = hashes.at(idx2);
            bool present = (result.at(idx2) == '0') ? false : true;
            map<QString, bool>::iterator mapiter = hashmap.find(QString::fromStdString(hash));
            if (hashmap.end() == mapiter) BOMB_WITH_WARNING("internal consistency check failed");
            mapiter->second = present;
        }
    }
    sock.write("BYE\r\n");
    sock.close();
    for (vector<FileMetaInformation>::iterator iter = fmi->begin() ; iter != fmi->end() ; ++iter)
    {
        map<QString, bool>::iterator mapiter = hashmap.find(iter->hash());
        if (hashmap.end() == mapiter) BOMB_WITH_WARNING("internal consistency check failed");
        iter->presentInNSRL = mapiter->second;

        if (iter->inNSRL())
            std::cerr << iter->hash().toStdString() << "  " << iter->filename().toStdString() << "\n";
    }
    emit updateMessage("Query complete");
    emit updateFileMetaInformation(fmi);
    emit updateMessage("worker complete");
}
