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

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

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
    QString filePath = QDir::currentPath() + "/../../config.json";
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
    qDebug() << "Pinging.." << serverAddress;

    // Start timer
    QElapsedTimer timer;
    timer.start();

    #ifdef Q_OS_WIN
        process.start("ping", QStringList() << "-n" << "1" << serverAddress); // Windows
    #else
        process.start("ping", QStringList() << "-c" << "1" << serverAddress); // Linux/macOS
    #endif

    // Wait for the process to finish
    if (!process.waitForFinished(5000)) { // 5-second timeout
        qDebug() << "Ping timed out!";
        return "Timeout";
    }

    // Stop timer
    qint64 elapsed = timer.elapsed();

    // Get output and debug the result
    QString output = process.readAllStandardOutput();
    qDebug() << "Ping Output:" << output;

    // Return the elapsed time as a string
    return QString::number(elapsed) + " ms";
}

void MainWindow::fetchServers()
{
    // Get the token
    QString battlemetricsKey = getConfig("battlemetrics_token");

    if (battlemetricsKey.isEmpty()) {
        qWarning() << "BattleMetrics token is missing in the config.";
        return;
    }

    // Create the network manager
    QNetworkAccessManager *manager = new QNetworkAccessManager(this);

    // API URL
    QUrl url("https://api.battlemetrics.com/servers");

    // Create the request
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");

    request.setRawHeader("Authorization", QString("Bearer %1").arg(battlemetricsKey).toUtf8());

    // Send GET request
    QNetworkReply *reply = manager->get(request);

    // Handle the response asynchronously
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        if (reply->error() == QNetworkReply::NoError) {
            // Read the response
            QByteArray response = reply->readAll();

            // Parse JSON
            QJsonDocument jsonDoc = QJsonDocument::fromJson(response);
            if (!jsonDoc.isNull()) {
                QJsonObject jsonObj = jsonDoc.object();

                // Access the "data" array
                QJsonArray serversArray = jsonObj.value("data").toArray();
                if (serversArray.isEmpty()) {
                    qWarning() << "No servers found in the response!";
                    return;
                }

                // Clear the table
                ui->serverListTable->setRowCount(0);

                // Iterate over the servers and add to the table
                int row = 0;
                for (const QJsonValue &serverValue : serversArray) {
                    if (!serverValue.isObject()) continue;

                    QJsonObject serverObject = serverValue.toObject();
                    QJsonObject serverDetails = serverObject.value("attributes").toObject().value("details").toObject();

                    // Extract server details
                    QString serverName = serverObject.value("attributes").toObject().value("name").toString();
                    QString serverMap = serverDetails.value("map").toString();
                    QString serverCountry = serverObject.value("attributes").toObject().value("country").toString();
                    QString serverIP = serverObject.value("attributes").toObject().value("ip").toString();
                    int serverPort = serverObject.value("attributes").toObject().value("port").toInt();
                    int players = serverObject.value("attributes").toObject().value("players").toInt();
                    int maxPlayers = serverObject.value("attributes").toObject().value("maxPlayers").toInt();
                    int port = serverObject.value("attributes").toObject().value("port").toInt();
                    int portQuery = serverObject.value("attributes").toObject().value("portQuery").toInt();

                    // QString serverPing = getPing(serverIP);

                    // Add a new row to the table
                    ui->serverListTable->insertRow(row);

                    // Populate the table
                    ui->serverListTable->setItem(row, 0, new QTableWidgetItem(serverName));
                    ui->serverListTable->setItem(row, 1, new QTableWidgetItem(serverMap));
                    ui->serverListTable->setItem(row, 2, new QTableWidgetItem(QString("%1/%2").arg(players).arg(maxPlayers)));
                    ui->serverListTable->setItem(row, 3, new QTableWidgetItem(serverCountry));
                    ui->serverListTable->setItem(row, 4, new QTableWidgetItem(serverIP));
                    ui->serverListTable->setItem(row, 5, new QTableWidgetItem(QString::number(portQuery)));

                    row++;
                }
            }
        } else {
            // Handle errors
            qDebug() << "API Error: " << reply->errorString();
        }

        reply->deleteLater(); // Clean up
    });
}

void MainWindow::setupTable()
{
    fetchServers();

    ui->serverListTable->setColumnCount(7);

    // Set column headers
    QStringList headers;
    headers << "Name" << "Map" << "Players" << "Country" << "IP" << "Port" << "Ping";
    ui->serverListTable->setHorizontalHeaderLabels(headers);
    ui->serverListTable->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft);
    ui->serverListTable->setSortingEnabled(true);
    ui->serverListTable->setAlternatingRowColors(true);
    ui->serverListTable->setShowGrid(false);
    ui->serverListTable->verticalHeader()->setVisible(false);
    ui->serverListTable->horizontalHeader()->setVisible(true);

    // Adjust individual column widths
    ui->serverListTable->setColumnWidth(0, 250); // Name
    ui->serverListTable->setColumnWidth(1, 200); // Map
    ui->serverListTable->setColumnWidth(2, 100); // Players
    ui->serverListTable->setColumnWidth(3, 150); // Country
    ui->serverListTable->setColumnWidth(4, 150); // IP
    ui->serverListTable->setColumnWidth(5, 100); // Port
    ui->serverListTable->setColumnWidth(6, 100); // Ping

    // Ensure columns stretch to fill any remaining space
    ui->serverListTable->horizontalHeader()->setStretchLastSection(true);
}
