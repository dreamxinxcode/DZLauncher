#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QJsonArray>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private:
    Ui::MainWindow *ui;

    void setupTable();
    void populateTable();
    void onRowClicked(int row, int column);
    void onRowDoubleClicked(int row, int column);
    QString getConfig(const QString &key);
    void checkConfig();
    void fetchServers();
    QJsonArray serverList;
    int serverPageOffset = 0;
    int serverPageSize = 100;
    bool isFetchingServers = false;
    bool hasMoreServers = true;
    void fetchNextServers();
};
#endif // MAINWINDOW_H
