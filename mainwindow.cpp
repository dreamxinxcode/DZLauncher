#include "mainwindow.h"
#include "./ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    setupTable();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::setupTable() \
{
    ui->serverListTable->setRowCount(4); // Set 3 rows
    ui->serverListTable->setColumnCount(6); // Set 2 columns

    // Set column headers
    QStringList headers;
    headers << "Name" << "Map" << "Players" << "Country" << "IP" << "Ping"; // Replace with your desired column names
    ui->serverListTable->setHorizontalHeaderLabels(headers);
}

