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

    // Connect double-click signal for rows
    connect(ui->serverListTable, &QTableWidget::cellClicked, this, &MainWindow::onRowClicked);
    connect(ui->serverListTable, &QTableWidget::cellDoubleClicked,
            this, &MainWindow::onRowDoubleClicked);

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
                    QString serverCountry = serverObject.value("attributes").toObject().value("country").toString().toLower();
                    QString serverIP = serverObject.value("attributes").toObject().value("ip").toString();
                    int players = serverObject.value("attributes").toObject().value("players").toInt();
                    int maxPlayers = serverObject.value("attributes").toObject().value("maxPlayers").toInt();
                    int portQuery = serverObject.value("attributes").toObject().value("portQuery").toInt();

                    // Add a new row to the table
                    ui->serverListTable->insertRow(row);

                    // Populate the table
                    ui->serverListTable->setItem(row, 0, new QTableWidgetItem(serverName));
                    ui->serverListTable->setItem(row, 1, new QTableWidgetItem(serverMap));
                    ui->serverListTable->setItem(row, 2, new QTableWidgetItem(QString("%1/%2").arg(players).arg(maxPlayers)));
                    ui->serverListTable->setItem(row, 4, new QTableWidgetItem(serverIP));
                    ui->serverListTable->setItem(row, 5, new QTableWidgetItem(QString::number(portQuery)));

                    // Render flag in the "Country" column
                    QLabel *flagLabel = new QLabel();
                    QString flagPath = QString(":/resources/flags/%1.svg").arg(serverCountry);
                    QPixmap flagPixmap(flagPath);

                    if (!flagPixmap.isNull()) {
                        flagLabel->setPixmap(flagPixmap.scaled(32, 32, Qt::KeepAspectRatio, Qt::SmoothTransformation));
                    } else {
                        qWarning() << "Flag not found for country:" << serverCountry;
                        flagLabel->setText(serverCountry); // Fallback: Show country name
                    }

                    ui->serverListTable->setCellWidget(row, 3, flagLabel);

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
    ui->serverListTable->setShowGrid(false);
    ui->serverListTable->horizontalHeader()->setVisible(true);

    // Adjust individual column widths
    ui->serverListTable->setColumnWidth(0, 350); // Name: Largest column, as it typically holds the most text
    ui->serverListTable->setColumnWidth(1, 200); // Map: Moderate width for map names
    ui->serverListTable->setColumnWidth(2, 100); // Players: Smaller width for player counts
    ui->serverListTable->setColumnWidth(3, 150); // Country: Moderate width for country names
    ui->serverListTable->setColumnWidth(4, 200); // IP: Moderate width for IP addresses
    ui->serverListTable->setColumnWidth(5, 100); // Port: Small width for port numbers
    ui->serverListTable->setColumnWidth(6, 50); // Ping: Small width for ping times

    // Ensure columns stretch to fill any remaining space
    ui->serverListTable->horizontalHeader()->setStretchLastSection(true);
}

void MainWindow::onRowClicked(int row, int column)
{
    Q_UNUSED(column); // Ignore the column for this use case

    QColor highlightColor("#318fca"); // Yellow (hex)
    QColor defaultColor("#22243a");   // White (hex)

    // Highlight the clicked row
    for (int i = 0; i < ui->serverListTable->rowCount(); ++i) {
        if (i == row) {
            for (int j = 0; j < ui->serverListTable->columnCount(); ++j) {
                QTableWidgetItem *item = ui->serverListTable->item(i, j);
                if (item) {
                    item->setBackground(highlightColor); // Highlight the row
                }
            }
        } else {
            for (int j = 0; j < ui->serverListTable->columnCount(); ++j) {
                QTableWidgetItem *item = ui->serverListTable->item(i, j);
                if (item) {
                    item->setBackground(defaultColor); // Reset the background color
                }
            }
        }
    }

    // Print the server details to the console
    QString serverName = ui->serverListTable->item(row, 0)->text();
    QString serverMap = ui->serverListTable->item(row, 1)->text();
    QString serverPlayers = ui->serverListTable->item(row, 2)->text();
    QString serverCountry = static_cast<QLabel*>(ui->serverListTable->cellWidget(row, 3))->text();
    QString serverIP = ui->serverListTable->item(row, 4)->text();
    QString serverPort = ui->serverListTable->item(row, 5)->text();

    qDebug() << "Server Details:"
             << "\nName: " << serverName
             << "\nMap: " << serverMap
             << "\nPlayers: " << serverPlayers
             << "\nCountry: " << serverCountry
             << "\nIP: " << serverIP
             << "\nPort: " << serverPort;

    ui->label_server_name->setText(serverName);
    ui->label_server_map->setText(serverMap);
    ui->label_server_players->setText(serverPlayers);
    ui->label_server_country->setText(serverCountry);
    ui->label_server_port->setText(serverPort);
    ui->label_server_ip->setText(serverIP);
}

void MainWindow::onRowDoubleClicked(int row, int column) {
    Q_UNUSED(column); // Ignore column if not needed

    QStringList rowData;
    for (int col = 0; col < ui->serverListTable->columnCount(); ++col) {
        QTableWidgetItem *item = ui->serverListTable->item(row, col);
        if (item) {
            rowData << item->text();
        }
    }
    qDebug() << "Row double-clicked:" << row << ", Data:" << rowData.join(", ");
}
