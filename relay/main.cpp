#include <QCoreApplication>
#include <QCommandLineParser>
#include <QDebug>
#include "relay_server.h"

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    app.setApplicationName("DouDiZhuRelay");
    app.setApplicationVersion("1.0");

    QCommandLineParser parser;
    parser.setApplicationDescription("DouDiZhu Relay Server — room management and message forwarding");
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption portOpt(
        QStringList() << "p" << "port",
        "Listening port (default 9527)",
        "port", "9527");
    parser.addOption(portOpt);
    parser.process(app);

    quint16 port = parser.value(portOpt).toUShort();

    RelayServer server;
    if (!server.start(port))
    {
        qCritical() << "Failed to start relay server on port" << port;
        return 1;
    }

    qInfo() << "DouDiZhuRelay running. Press Ctrl+C to stop.";

    return app.exec();
}
