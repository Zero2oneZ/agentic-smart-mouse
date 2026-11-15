#include "backend.h"
#include "gitworkspace.h"
#include "sessionledger.h"
#include "theme.h"
#include "translator.h"
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
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
    void ledgerScopesFocusAndRedaction() {
        QTemporaryDir dir;
        int project, other, child;
        qint64 attached;
        {
            Backend b(dir.path());
            project = b.addCategory("Project", "/tmp/project", "Small project");
            attached = b.recordEvent(project, "command.finished", "Build finished",
                                     "password=example-secret\nCompiled successfully", "/tmp/project");
            QVERIFY(attached > 0);
            other = b.addCategory("Other", "/tmp/other", "Unrelated");
            b.select(project);
            QVERIFY(b.readEvent(attached)["content"].toString().contains("[REDACTED]"));
            QVERIFY(!b.readEvent(attached)["content"].toString().contains("example-secret"));
            auto outsider = b.recordEvent(other, "session.note", "Other secret", "Other content");
            QVERIFY(b.readEvent(outsider).contains("error"));
            QVERIFY(b.contextTool("workspace_overview", {{"category", other}}).contains("error"));
            child = b.focusWorkspace(project, "Menu rendering", "Fix radial layout");
            QVERIFY(child > other);
            QVERIFY(b.brief().contains("Fix radial layout"));
            QVERIFY(b.brief().contains(QString("#%1").arg(attached)));
            QVERIFY(!b.readEvent(attached).contains("error"));
            QVERIFY(b.readEvent(outsider).contains("error"));
            QCOMPARE(b.messages().size(), 0);
            b.select(1);
            auto events = b.timeline();
            QVERIFY(events.size() >= 5);
            for (int i = 1; i < events.size(); ++i)
                QVERIFY(events[i - 1].toMap()["id"].toLongLong() > events[i].toMap()["id"].toLongLong());
            QVERIFY(!events.first().toMap()["created"].toString().isEmpty());
        }
        Backend b(dir.path());
        b.select(1);
        QVERIFY(!b.readEvent(attached).contains("error"));
        b.select(child);
        QVERIFY(b.brief().contains("Fix radial layout"));
    }
    void attachmentsAndBoundedContext() {
        QTemporaryDir dir;
        Backend b(dir.filePath("data"));
        QFile file(dir.filePath("session.jsonl"));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("{\"command\":\"build\",\"result\":\"ok\"}\n");
        file.close();
        QVERIFY(b.attachSession(file.fileName()));
        QCOMPARE(b.timeline().first().toMap()["kind"].toString(), QString("session.attached"));
        QFile env(dir.filePath(".env"));
        QVERIFY(env.open(QIODevice::WriteOnly));
        env.write("TOKEN=private\n");
        env.close();
        QVERIFY(!b.attachSession(env.fileName()));
        for (int i = 0; i < 25; ++i)
            b.send(QString(10000, 'x'));
        auto context = b.requestContext();
        int chars = 0;
        for (const auto &v : context)
            chars += v.toObject()["content"].toString().size();
        QVERIFY(chars <= 30000);
        QVERIFY(context.size() <= 13);
        QCOMPARE(b.messages().size(), 25);
        auto all = b.eventPage(1);
        QVERIFY(all.size() > 20);
        auto older = b.eventPage(1, all.last().toMap()["id"].toLongLong());
        QVERIFY(older.isEmpty());
    }
    void managedGitIsolationCheckpointAndPush() {
        QTemporaryDir dir;
        QString source = dir.filePath("source"), remote = dir.filePath("remote.git"),
                work = dir.filePath("work");
        QDir().mkpath(source);
        auto git = [](const QString &cwd, const QStringList &args) {
            QProcess p;
            p.start("git", QStringList{"-C", cwd} + args);
            if (!p.waitForFinished(5000))
                return false;
            return p.exitCode() == 0;
        };
        auto write = [](const QString &path, const QByteArray &data) {
            QFile f(path);
            return f.open(QIODevice::WriteOnly) && f.write(data) == data.size();
        };
        QVERIFY(git(source, {"init"}));
        QVERIFY(git(source, {"config", "user.name", "Hotbox Test"}));
        QVERIFY(git(source, {"config", "user.email", "test@example.invalid"}));
        QVERIFY(write(source + "/app.txt", "initial\n"));
        QVERIFY(git(source, {"add", "app.txt"}));
        QVERIFY(git(source, {"commit", "-m", "Initial"}));
        QVERIFY(git(dir.path(), {"init", "--bare", remote}));
        QVERIFY(git(source, {"remote", "add", "origin", remote}));
        QVERIFY(write(source + "/app.txt", "uncommitted source edit\n"));
        auto fork = GitWorkspace::fork(source, work, "hotbox/test");
        QVERIFY2(fork["ok"].toBool(), qPrintable(fork["error"].toString()));
        QFile initial(work + "/app.txt");
        QVERIFY(initial.open(QIODevice::ReadOnly));
        QCOMPARE(initial.readAll(), QByteArray("initial\n"));
        initial.close();
        QVERIFY(write(work + "/app.txt", "focused edit\n"));
        QVERIFY(write(work + "/unselected.txt", "leave alone\n"));
        auto checkpoint = GitWorkspace::checkpoint(work, "hotbox/test", {"app.txt"}, "origin", true);
        QVERIFY2(checkpoint["ok"].toBool(), qPrintable(checkpoint["error"].toString()));
        QVERIFY(checkpoint["pushed"].toBool());
        QFile original(source + "/app.txt");
        QVERIFY(original.open(QIODevice::ReadOnly));
        QCOMPARE(original.readAll(), QByteArray("uncommitted source edit\n"));
        QVERIFY(git(remote, {"show-ref", "--verify", "refs/heads/hotbox/test"}));
        auto snapshot = GitWorkspace::snapshot(work);
        QVERIFY(snapshot["tree"].toString().contains("app.txt"));
        QVERIFY(!snapshot["tree"].toString().contains("unselected.txt"));
        QVERIFY(write(work + "/.env", "API_KEY=private\n"));
        QVERIFY(!GitWorkspace::checkpoint(work, "hotbox/test", {".env"}, "", false)["ok"].toBool());
        QVERIFY(
            !GitWorkspace::checkpoint(work, "hotbox/test", {"../source/app.txt"}, "", false)["ok"].toBool());
        QVERIFY(write(work + "/credentials.txt", "password=private\n"));
        QVERIFY(
            !GitWorkspace::checkpoint(work, "hotbox/test", {"credentials.txt"}, "", false)["ok"].toBool());
        QVERIFY(git(work, {"add", "unselected.txt"}));
        QVERIFY(!GitWorkspace::checkpoint(work, "hotbox/test", {"app.txt"}, "", false)["ok"].toBool());
    }
    void externalSessionUpdatesReachResidentApp() {
        QTemporaryDir dir;
        Backend resident(dir.path());
        Backend writer(dir.path());
        QSignalSpy activity(&resident, &Backend::activityChanged);
        QVERIFY(writer.recordEvent(1, "session.note", "External update", "Details") > 0);
        QTRY_VERIFY_WITH_TIMEOUT(activity.size() > 0, 3000);
        QCOMPARE(resident.timeline().first().toMap()["summary"].toString(), QString("External update"));
    }
    void backgroundGitResumesAndPausesOnFailure() {
        QTemporaryDir dir;
        QString source = dir.filePath("source");
        QDir().mkpath(source);
        auto git = [&](const QStringList &args) {
            QProcess p;
            p.start("git", QStringList{"-C", source} + args);
            return p.waitForFinished(5000) && p.exitCode() == 0;
        };
        auto write = [](const QString &path, const QByteArray &data) {
            QFile f(path);
            return f.open(QIODevice::WriteOnly) && f.write(data) == data.size();
        };
        QVERIFY(git({"init"}));
        QVERIFY(git({"config", "user.name", "Hotbox Test"}));
        QVERIFY(git({"config", "user.email", "test@example.invalid"}));
        QVERIFY(write(source + "/app.txt", "initial\n"));
        QVERIFY(git({"add", "app.txt"}));
        QVERIFY(git({"commit", "-m", "Initial"}));
        int id;
        QString work;
        {
            Backend b(dir.filePath("data"));
            id = b.addCategory("Project", source, "Test background work");
            b.forkGit();
            QTRY_VERIFY_WITH_TIMEOUT(!b.taskBusy(), 10000);
            QVERIFY(b.gitState()["managed"].toBool());
            work = b.folder();
            QVERIFY(work != source);
            b.configureGit("app.txt", "", true, false);
            QVERIFY(b.gitState()["automatic"].toBool());
        }
        {
            Backend b(dir.filePath("data"));
            b.select(id);
            QVERIFY(b.gitState()["automatic"].toBool());
            QVERIFY(write(work + "/app.txt", "background change\n"));
            b.processBackgroundGit();
            QTRY_VERIFY_WITH_TIMEOUT(!b.taskBusy(), 10000);
            QCOMPARE(b.timeline().first().toMap()["kind"].toString(), QString("git.checkpoint"));
            auto event = b.readEvent(b.timeline().first().toMap()["id"].toLongLong());
            QVERIFY(event["content"].toString().contains("history"));
            b.configureGit("app.txt", "missing-remote", true, true);
            b.processBackgroundGit();
            QTRY_VERIFY_WITH_TIMEOUT(!b.taskBusy(), 10000);
            QVERIFY(!b.gitState()["automatic"].toBool());
            QCOMPARE(b.timeline().first().toMap()["kind"].toString(), QString("git.error"));
        }
    }
    void modelToolLoopUsesOriginalWorkspace() {
        QTcpServer server;
        QVERIFY(server.listen(QHostAddress::LocalHost));
        int requests = 0;
        QJsonObject second;
        connect(&server, &QTcpServer::newConnection, this, [&] {
            auto socket = server.nextPendingConnection();
            auto buffer = new QByteArray;
            connect(socket, &QObject::destroyed, this, [buffer] { delete buffer; });
            connect(socket, &QTcpSocket::readyRead, this, [&, socket, buffer] {
                *buffer += socket->readAll();
                int end = buffer->indexOf("\r\n\r\n");
                if (end < 0)
                    return;
                int size = 0;
                for (auto line : buffer->left(end).split('\n'))
                    if (line.toLower().startsWith("content-length:"))
                        size = line.mid(15).trimmed().toInt();
                if (buffer->size() - end - 4 < size)
                    return;
                auto body = QJsonDocument::fromJson(buffer->mid(end + 4, size)).object();
                buffer->clear();
                ++requests;
                QJsonObject message;
                if (requests == 1) {
                    socket->write(
                        "HTTP/1.1 200 OK\r\nContent-Type: text/event-stream\r\nConnection: close\r\n\r\n");
                    QJsonObject first{{"index", 0},
                                      {"id", "call_test"},
                                      {"type", "function"},
                                      {"function", QJsonObject{{"name", "workspace_overview"},
                                                               {"arguments", "{\"category\":"}}}};
                    QJsonObject last{{"index", 0},
                                     {"function", QJsonObject{{"arguments", "2,\"before\":0}"}}}};
                    for (const auto &call : QJsonArray{first, last}) {
                        QJsonObject chunk{
                            {"choices", QJsonArray{QJsonObject{
                                            {"delta", QJsonObject{{"tool_calls", QJsonArray{call}}}}}}}};
                        socket->write("data: " + QJsonDocument(chunk).toJson(QJsonDocument::Compact) +
                                      "\n\n");
                    }
                    socket->write("data: [DONE]\n\n");
                    socket->disconnectFromHost();
                    return;
                }
                second = body;
                message = {{"role", "assistant"}, {"content", "Read the project events."}};
                QByteArray response =
                    QJsonDocument(QJsonObject{{"choices", QJsonArray{QJsonObject{{"message", message}}}}})
                        .toJson(QJsonDocument::Compact);
                socket->write("HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nConnection: "
                              "close\r\nContent-Length: " +
                              QByteArray::number(response.size()) + "\r\n\r\n" + response);
                socket->disconnectFromHost();
            });
        });
        QTemporaryDir dir;
        Backend b(dir.path());
        int origin = b.addCategory("Project", "/tmp/project", "Origin brief");
        QCOMPARE(origin, 2);
        b.saveSettings(QString("http://127.0.0.1:%1/v1/chat/completions").arg(server.serverPort()), "test",
                       "HOTBOX_TEST_NO_KEY");
        b.send("Read my history");
        b.select(1);
        QTRY_VERIFY_WITH_TIMEOUT(!b.busy(), 4000);
        QCOMPARE(requests, 2);
        auto messages = second["messages"].toArray();
        QCOMPARE(messages.last().toObject()["role"].toString(), QString("tool"));
        QVERIFY(messages.last().toObject()["content"].toString().contains("Origin brief"));
        QCOMPARE(b.messages().size(), 0);
        b.select(origin);
        QCOMPARE(b.messages().last().toMap()["content"].toString(), QString("Read the project events."));
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
