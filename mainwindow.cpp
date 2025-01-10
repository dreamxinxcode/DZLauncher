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

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    // Set up a layout for the central widget
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->addWidget(ui->serverListTable);
    centralWidget()->setLayout(layout);

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
            qDebug() << "Response: " << response;

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

                    // Add a new row to the table
                    ui->serverListTable->insertRow(row);

                    // Populate the cells
                    ui->serverListTable->setItem(row, 0, new QTableWidgetItem(serverName));
                    ui->serverListTable->setItem(row, 1, new QTableWidgetItem(serverMap));
                    ui->serverListTable->setItem(row, 2, new QTableWidgetItem(QString("%1/%2").arg(players).arg(maxPlayers)));
                    ui->serverListTable->setItem(row, 3, new QTableWidgetItem(serverCountry));
                    ui->serverListTable->setItem(row, 4, new QTableWidgetItem(serverIP));
                    ui->serverListTable->setItem(row, 5, new QTableWidgetItem(QString::number(portQuery)));

                    row++; // Move to the next row
                }
            }
        } else {
            // Handle errors
            qDebug() << "API Error: " << reply->errorString();
        }

        reply->deleteLater(); // Clean up
    });
}


void MainWindow::setupTable() \
{
    fetchServers();

    ui->serverListTable->setRowCount(100);
    ui->serverListTable->setColumnCount(7);

    // Set column headers
    QStringList headers;
    headers << "Name" << "Map" << "Players" << "Country" << "IP" << "Port" << "Ping";
    ui->serverListTable->setHorizontalHeaderLabels(headers);


    // Stretch the columns to fill the table's width
    ui->serverListTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);

    // Optional: Make the table fill its parent widget
    ui->serverListTable->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

