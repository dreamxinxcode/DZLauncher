#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include <QVBoxLayout>
#include <QHeaderView>

// Network
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QJsonObject>

// Config
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>

// Other
#include <QDebug>
#include <QDir>
#include <QJsonArray>
#include <QPixmap>
#include <QProcess>
#include <QElapsedTimer>
#include <QScrollBar> // Added for infinite scroll
#include <QUrlQuery> // Added for pagination

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    fetchServers();

    // Connect double-click signal for rows
    connect(ui->serverListTable, &QTableWidget::cellClicked, this, &MainWindow::onRowClicked);
    connect(ui->serverListTable, &QTableWidget::cellDoubleClicked,
            this, &MainWindow::onRowDoubleClicked);

    // Infinite scroll: fetch more when scrolled to bottom
    connect(ui->serverListTable->verticalScrollBar(), &QScrollBar::valueChanged, this, [this](int value) {
        if (value == ui->serverListTable->verticalScrollBar()->maximum()) {
            fetchNextServers();
        }
    });

    this->setFixedSize(1500, 1000);

    setupTable();
}

MainWindow::~MainWindow()
{
    delete ui;
}

QString MainWindow::getConfig(const QString &key)
{
    // Relative path to the project root
    QString filePath = QDir::currentPath() + "/config.json";
    qDebug() << "Looking for config file at:" << filePath;

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "Could not open config file at:" << filePath;
        return QString();
    }

    QByteArray jsonData = file.readAll();
    QJsonDocument document = QJsonDocument::fromJson(jsonData);
    if (!document.isObject()) {
        qWarning() << "Invalid JSON format";
        return QString();
    }

    QJsonObject jsonObject = document.object();
    if (!jsonObject.contains(key)) {
        qWarning() << "Key not found in JSON:" << key;
        return QString();
    }

    return jsonObject.value(key).toString();
}

void MainWindow::checkConfig()
{
    QString battlemetricskey = getConfig("battlemetrics_token");

    if (battlemetricskey.isEmpty())
    {
        qDebug() << "Error: BattleMetrics key is missing";
    }
}

QString getPing(const QString &serverAddress) {
    QProcess process;

#ifdef Q_OS_WIN
    process.start("ping", QStringList() << "-n" << "1" << serverAddress);
#else
    process.start("ping", QStringList() << "-c" << "1" << serverAddress);
#endif

    if (!process.waitForFinished(5000)) {
        return "Timeout";
    }

    QString output = process.readAllStandardOutput();
    QRegularExpression regex(R"(time[=<]\s*(\d+\.?\d*)\s*ms)");
    QRegularExpressionMatch match = regex.match(output);
    if (match.hasMatch()) {
        return match.captured(1) + " ms";
    }
    return "N/A";
}

void MainWindow::fetchServers()
{
    serverList = QJsonArray();
    serverPageOffset = 0;
    hasMoreServers = true;
    fetchNextServers();
}

void MainWindow::fetchNextServers()
{
    if (isFetchingServers || !hasMoreServers) return;
    isFetchingServers = true;
    QString battlemetricsKey = getConfig("battlemetrics_token");
    if (battlemetricsKey.isEmpty()) {
        qWarning() << "[fetchServers] BattleMetrics token is missing in the config.";
        isFetchingServers = false;
        return;
    }
    auto manager = new QNetworkAccessManager(this);
    QUrl url("https://api.battlemetrics.com/servers");
    QUrlQuery query;
    query.addQueryItem("filter[game]", "dayz");
    query.addQueryItem("page[size]", QString::number(serverPageSize));
    query.addQueryItem("page[offset]", QString::number(serverPageOffset));
    url.setQuery(query);
    qDebug() << "[fetchServers] Requesting URL:" << url.toString();
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setRawHeader("Authorization", QString("Bearer %1").arg(battlemetricsKey).toUtf8());
    QNetworkReply *reply = manager->get(request);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        QByteArray response = reply->readAll();
        qDebug() << "[fetchServers] HTTP response:" << response.left(500);
        reply->deleteLater();
        isFetchingServers = false;
        QJsonDocument jsonDoc = QJsonDocument::fromJson(response);
        if (jsonDoc.isNull() || !jsonDoc.isObject()) {
            qWarning() << "[fetchServers] Invalid JSON format.";
            return;
        }
        QJsonArray serversArray = jsonDoc.object().value("data").toArray();
        qDebug() << "[fetchServers] Number of servers in response:" << serversArray.size();
        for (const QJsonValue &val : serversArray) {
            serverList.append(val);
        }
        if (serversArray.size() < serverPageSize) {
            hasMoreServers = false;
        } else {
            serverPageOffset += serverPageSize;
        }
        populateTable();
    });
}

void MainWindow::populateTable()
{
    qDebug() << "[populateTable] Populating table with" << serverList.size() << "servers.";
    ui->serverListTable->setRowCount(0); // Clear existing rows

    for (int i = 0; i < serverList.size(); ++i) {
        QJsonValue value = serverList.at(i);
        if (!value.isObject()) continue;

        QJsonObject serverObject = value.toObject();
        QJsonObject attributes = serverObject.value("attributes").toObject();

        QString serverName = attributes.value("name").toString("Unknown Name");
        QString serverMap = attributes.value("details").toObject().value("map").toString("Unknown Map");
        QString serverPlayers = QString("%1/%2")
                                    .arg(attributes.value("players").toInt())
                                    .arg(attributes.value("maxPlayers").toInt());
        QString serverCountry = attributes.value("country").toString("Unknown Country");
        QString serverIP = attributes.value("ip").toString("0.0.0.0");
        QString serverPort = QString::number(attributes.value("port").toInt());
        QString ping = "N/A"; // Optional: Add logic to calculate ping if needed

        int row = ui->serverListTable->rowCount();
        ui->serverListTable->insertRow(row);

        ui->serverListTable->setItem(row, 0, new QTableWidgetItem(serverName));
        ui->serverListTable->setItem(row, 1, new QTableWidgetItem(serverMap));
        ui->serverListTable->setItem(row, 2, new QTableWidgetItem(serverPlayers));
        ui->serverListTable->setItem(row, 3, new QTableWidgetItem(serverCountry));
        ui->serverListTable->setItem(row, 4, new QTableWidgetItem(serverIP));
        ui->serverListTable->setItem(row, 5, new QTableWidgetItem(serverPort));
        ui->serverListTable->setItem(row, 6, new QTableWidgetItem(ping));
    }
}

void MainWindow::setupTable()
{
    ui->serverListTable->clear(); // Clear headers and contents
    QStringList headers{"Name", "Map", "Players", "Country", "IP", "Port", "Ping"};
    ui->serverListTable->setColumnCount(headers.size());
    ui->serverListTable->setHorizontalHeaderLabels(headers);

    ui->serverListTable->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft);
    ui->serverListTable->setSortingEnabled(true);
    ui->serverListTable->setShowGrid(false);
    ui->serverListTable->horizontalHeader()->setVisible(true);

    // Adjust individual column widths
    ui->serverListTable->setColumnWidth(0, 350);
    ui->serverListTable->setColumnWidth(1, 200);
    ui->serverListTable->setColumnWidth(2, 100);
    ui->serverListTable->setColumnWidth(3, 150);
    ui->serverListTable->setColumnWidth(4, 200);
    ui->serverListTable->setColumnWidth(5, 100);
    ui->serverListTable->setColumnWidth(6, 50);

    // Ensure columns stretch to fill remaining space
    ui->serverListTable->horizontalHeader()->setStretchLastSection(true);

    // Populate data
    populateTable();
}

void MainWindow::onRowClicked(int row, int column)
{
    Q_UNUSED(column);

    if (row < 0 || row >= serverList.size()) {
        qDebug() << "Invalid row index";
        return;
    }

    QJsonObject serverObject = serverList.at(row).toObject();
    QJsonObject attributes = serverObject.value("attributes").toObject();

    QString serverName = attributes.value("name").toString("Unknown Name");
    QString serverMap = attributes.value("details").toObject().value("map").toString("Unknown Map");
    QString serverPlayers = QString("%1/%2")
                                .arg(attributes.value("players").toInt())
                                .arg(attributes.value("maxPlayers").toInt());
    QString serverCountry = attributes.value("country").toString("Unknown Country");
    QString serverIP = attributes.value("ip").toString("0.0.0.0");
    QString serverPort = QString::number(attributes.value("port").toInt());

    qDebug() << "Server Details:"
             << "\nName: " << serverName
             << "\nMap: " << serverMap
             << "\nPlayers: " << serverPlayers
             << "\nCountry: " << serverCountry
             << "\nIP: " << serverIP
             << "\nPort: " << serverPort;

    // Update UI labels with the details
    ui->label_server_name->setText(serverName);
    ui->label_server_map->setText(serverMap);
    ui->label_server_players->setText(serverPlayers);
    ui->label_server_country->setText(serverCountry);
    ui->label_server_port->setText(serverPort);
    ui->label_server_ip->setText(serverIP);
}

void MainWindow::onRowDoubleClicked(int row, int column)
{
    Q_UNUSED(column);

    if (row < 0 || row >= ui->serverListTable->rowCount()) {
        qWarning() << "Invalid row index for double-click.";
        return;
    }

    QTableWidgetItem *ipItem = ui->serverListTable->item(row, 4);
    QTableWidgetItem *portItem = ui->serverListTable->item(row, 5);

    if (!ipItem || !portItem) {
        qDebug() << "Invalid IP or Port item";
        return;
    }

    QString serverIP = ipItem->text();
    QString serverPort = portItem->text();
    QString joinCommand = QString("steam steam://connect/%1:%2").arg(serverIP, serverPort);

    qDebug() << "Joining server with command:" << joinCommand;

    QProcess *process = new QProcess(this);
    process->start(joinCommand);

    if (!process->waitForStarted()) {
        qWarning() << "Failed to start Steam process for joining server.";
    }
}
