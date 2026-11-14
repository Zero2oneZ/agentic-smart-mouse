#include "backend.h"
#include "theme.h"
#include "translator.h"
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QtTest>

class HotboxTests : public QObject {
    Q_OBJECT
  private slots:
    void persistenceAndArchives() {
        QTemporaryDir dir;
        int parent, child;
        {
            Backend b(dir.path());
            QVERIFY(b.status().isEmpty());
            parent = b.addCategory("Weather", "/tmp/weather", "Hourly weather");
            QVERIFY(parent > 1);
            child = b.addCategory("UI", "/tmp/weather", "Theme support", parent);
            QVERIFY(child > parent);
            b.send("Remember the blue accent");
            QCOMPARE(b.messages().size(), 1);
            b.updateCategory(child, "UI", "/tmp/weather", "Blue accent");
        }
        {
            Backend b(dir.path());
            b.select(child);
            QCOMPARE(b.brief(), QString("Blue accent"));
            QCOMPARE(b.messages().first().toMap()["content"].toString(), QString("Remember the blue accent"));
            b.archive(parent);
            QCOMPARE(b.categories().size(), 1);
            QCOMPARE(b.archivedCategories().size(), 2);
            b.restore(child);
            QCOMPARE(b.categories().size(), 3);
            QCOMPARE(b.children(parent).size(), 1);
            b.archive(1);
            QCOMPARE(b.categories().size(), 3);
        }
    }
    void holdThresholdAndRelease() {
        QTemporaryDir dir;
        Backend b(dir.path());
        QSignalSpy shown(&b, &Backend::showMenu);
        b.hold();
        QTest::qWait(200);
        b.release();
        QTest::qWait(360);
        QCOMPARE(shown.size(), 0);
        QVERIFY(!b.expanded());
        b.hold();
        QTest::qWait(300);
        b.hold();
        QTRY_VERIFY_WITH_TIMEOUT(b.expanded(), 400);
        QCOMPARE(shown.size(), 1);
        b.release();
        QVERIFY(!b.expanded());
        QTest::qWait(150);
        QCOMPARE(shown.size(), 1);
    }
    void invalidSettingsDoNotReplaceConnection() {
        QTemporaryDir dir;
        Backend b(dir.path());
        QString before = b.endpoint();
        b.saveSettings("http://remote.invalid/v1/chat/completions", "model", "HOTBOX_API_KEY");
        QCOMPARE(b.endpoint(), before);
        QVERIFY(!b.status().isEmpty());
        b.saveSettings("http://127.0.0.1:11434/v1/chat/completions", "local-model", "HOTBOX_API_KEY");
        QCOMPARE(b.model(), QString("local-model"));
    }
    void streamSavesToOriginAndIncludesBrief() {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        QByteArray requestBody;
        bool replied = false;
        connect(&server, &QTcpServer::newConnection, this, [&] {
            auto *socket = server.nextPendingConnection();
            auto buffer = new QByteArray;
            connect(socket, &QObject::destroyed, this, [buffer] { delete buffer; });
            connect(socket, &QTcpSocket::readyRead, this, [&, socket, buffer] {
                *buffer += socket->readAll();
                int end = buffer->indexOf("\r\n\r\n");
                if (end < 0 || replied)
                    return;
                auto header = buffer->left(end);
                int size = 0;
                for (auto line : header.split('\n'))
                    if (line.toLower().startsWith("content-length:"))
                        size = line.mid(15).trimmed().toInt();
                if (buffer->size() - end - 4 < size)
                    return;
                replied = true;
                requestBody = buffer->mid(end + 4, size);
                socket->write(
                    "HTTP/1.1 200 OK\r\nContent-Type: text/event-stream\r\nConnection: close\r\n\r\n");
                socket->write("data: {\"choices\":[{\"delta\":{\"content\":\"Hello \"}}]}\n\n");
                QTimer::singleShot(60, socket, [socket] {
                    socket->write("data: {\"choices\":[{\"delta\":{\"content\":\"");
                    socket->write(QByteArray::fromHex("e2"));
                });
                QTimer::singleShot(120, socket, [socket] {
                    socket->write(QByteArray::fromHex("9c93"));
                    socket->write("\"}}]}\n\ndata: [DONE]\n\n");
                    socket->disconnectFromHost();
                });
            });
        });
        QTemporaryDir dir;
        Backend b(dir.path());
        int origin = b.addCategory("Project", "/tmp/project", "Use the blue accent");
        b.saveSettings(QString("http://127.0.0.1:%1/v1/chat/completions").arg(server.serverPort()),
                       "test-model", "HOTBOX_TEST_NO_KEY");
        b.send("Build a menu");
        QVERIFY(b.busy());
        b.select(1);
        QTRY_VERIFY_WITH_TIMEOUT(!b.busy(), 3000);
        QVERIFY2(b.status().isEmpty(), qPrintable(b.status()));
        QCOMPARE(b.messages().size(), 0);
        b.select(origin);
        QCOMPARE(b.messages().size(), 2);
        QCOMPARE(b.messages().last().toMap()["content"].toString(), QString::fromUtf8("Hello ✓"));
        auto request = QJsonDocument::fromJson(requestBody).object();
        QVERIFY(request["stream"].toBool());
        QVERIFY(request["messages"].toArray()[0].toObject()["content"].toString().contains(
            "Use the blue accent"));
    }
    void failedConnectionRetainsUserMessage() {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        auto port = server.serverPort();
        server.close();
        QTemporaryDir dir;
        Backend b(dir.path());
        b.saveSettings(QString("http://127.0.0.1:%1/v1/chat/completions").arg(port), "test-model",
                       "HOTBOX_TEST_NO_KEY");
        b.send("Keep this message");
        QTRY_VERIFY_WITH_TIMEOUT(!b.busy(), 3000);
        QCOMPARE(b.messages().size(), 1);
        QVERIFY(!b.status().isEmpty());
    }
    void qmlLoadsAndCenterOpensMaster() {
        QTemporaryDir dir;
        Backend b(dir.path());
        Theme theme(dir.path());
        Translator translator(dir.filePath("language"), dir.path());
        QQmlApplicationEngine engine;
        QStringList warnings;
        connect(&engine, &QQmlEngine::warnings, this, [&](const QList<QQmlError> &errors) {
            for (auto e : errors)
                warnings << e.toString();
        });
        engine.rootContext()->setContextProperty("backend", &b);
        engine.rootContext()->setContextProperty("theme", &theme);
        engine.rootContext()->setContextProperty("translator", &translator);
        engine.load(QUrl("qrc:/Main.qml"));
        QVERIFY2(!engine.rootObjects().isEmpty(), qPrintable(warnings.join('\n')));
        auto *win = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
        auto *overlay = win->findChild<QQuickWindow *>("hotboxOverlay");
        QVERIFY(overlay);
        overlay->setProperty("anchorX", 500);
        overlay->setProperty("anchorY", 350);
        b.preview();
        overlay->show();
        QTest::qWait(200);
        QSignalSpy chat(&b, &Backend::showChat);
        QTest::mouseDClick(overlay, Qt::LeftButton, Qt::NoModifier, QPoint(500, 350));
        QCOMPARE(chat.size(), 1);
        QCOMPARE(b.currentId(), 1);
        QVERIFY(!b.expanded());
        QVERIFY2(warnings.isEmpty(), qPrintable(warnings.join('\n')));
    }
};
int main(int argc, char **argv) {
    qputenv("QT_QUICK_BACKEND", "software");
    QGuiApplication app(argc, argv);
    QQuickStyle::setStyle("Material");
    HotboxTests tests;
    return QTest::qExec(&tests, argc, argv);
}
#include "hotbox_tests.moc"
