#include "backend.h"
#include "theme.h"
#include "translator.h"
#include <QCursor>
#include <QDir>
#include <QElapsedTimer>
#include <QEventLoop>
#include <QFile>
#include <QGuiApplication>
#include <QIcon>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalServer>
#include <QLocalSocket>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QRegularExpression>
#include <QScreen>
#include <QStandardPaths>
#include <QTextStream>
#include <QTimer>
#include <Window>

static QJsonDocument hypr(const QString &command) {
    QProcess p;
    p.start("hyprctl", {"-j", command});
    if (!p.waitForFinished(800)) {
        p.kill();
        p.waitForFinished(100);
        return {};
    }
    return QJsonDocument::fromJson(p.readAllStandardOutput());
}
int main(int argc, char **argv) {
    const QStringList raw = [&] {
        QStringList a;
        for (int i = 1; i < argc; ++i)
            a << QString::fromLocal8Bit(argv[i]);
        return a;
    }();
    const bool check = raw.contains("check"), smoke = raw.contains("--smoke");
    const QString commandName = raw.value(0, "open");
    const bool headless = QStringList{"events",   "read-event", "attach", "record",     "focus",
                                      "snapshot", "branch",     "sync",   "git-config", "run"}
                              .contains(commandName) ||
                          commandName == "segment" || commandName == "architecture";
    if (check || smoke || headless) {
        qputenv("QT_QPA_PLATFORM", "offscreen");
        qputenv("QT_QPA_PLATFORMTHEME", "");
    }
    if (smoke)
        qputenv("QT_QUICK_BACKEND", "software");
    QGuiApplication app(argc, argv);
    app.setApplicationName("hotbox");
    app.setApplicationVersion("0.4.0");
    app.setDesktopFileName("hotbox");
    app.setQuitOnLastWindowClosed(false);
    app.setWindowIcon(QIcon::fromTheme("hotbox"));
    QString command = raw.isEmpty() ? "open" : raw.first();
    QString dataDir;
    if (smoke) {
        int n = raw.indexOf("--smoke");
        if (n + 1 >= raw.size())
            return 2;
        dataDir = raw[n + 1] + "/data";
    }
    const int separatorIndex = raw.indexOf("--");
    const QStringList options = separatorIndex >= 0 ? raw.mid(0, separatorIndex) : raw;
    auto option = [&](const QString &name, const QString &fallback = QString()) {
        int i = options.indexOf(name);
        return i >= 0 && i + 1 < options.size() ? options[i + 1] : fallback;
    };
    if (options.contains("--data-dir"))
        dataDir = option("--data-dir");
    QString serverPath = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation) + "/hotbox-ipc";
    QLocalSocket client;
    if (!smoke && !check && !headless) {
        client.connectToServer(serverPath);
        if (client.waitForConnected(150)) {
            client.write(command.toUtf8() + "\n");
            client.waitForBytesWritten(500);
            return 0;
        }
        if (command == "hold" || command == "release" || command == "quit") {
            QTextStream(stderr) << "Hotbox is not running. Start hotbox daemon first.\n";
            return 1;
        }
    }
    Backend backend(dataDir);
    if (headless) {
        bool valid = false;
        int category = option("--category", "1").toInt(&valid);
        if (!valid || category <= 0) {
            QTextStream(stderr) << "Invalid category ID\n";
            return 2;
        }
        backend.select(category);
        if (backend.currentId() != category) {
            QTextStream(stderr) << "Workspace does not exist\n";
            return 2;
        }
        QJsonObject result;
        int exitCode = 0;
        if (command == "architecture") {
            QString action = "catalog";
            for (int i = 1; i < options.size(); ++i) {
                if (options[i].startsWith("--")) {
                    if (options[i] != "--json")
                        ++i;
                    continue;
                }
                action = options[i];
                break;
            }
            QVariantMap arguments{{"toolRoot", option("--tool-root")},
                                  {"id", option("--id")},
                                  {"path", option("--relative-path")},
                                  {"description", option("--description")},
                                  {"runtimeRoot", option("--runtime-root")},
                                  {"capabilities", option("--capabilities").split(',', Qt::SkipEmptyParts)},
                                  {"artist", option("--artist")},
                                  {"album", option("--album")},
                                  {"genre", option("--genre")}};
            result = QJsonObject::fromVariantMap(backend.architectureAction(action, arguments));
        } else if (command == "segment") {
            QString action = "list";
            for (int i = 1; i < options.size(); ++i) {
                if (options[i].startsWith("--")) {
                    if (options[i] != "--json")
                        ++i;
                    continue;
                }
                action = options[i];
                break;
            }
            if (action == "list")
                result = {{"segments", QJsonArray::fromVariantList(backend.segmentPage(
                                           option("--state", QString("")), option("--query", QString("")),
                                           option("--before", "0").toLongLong()))}};
            else {
                QVariantMap arguments;
                for (const auto &key :
                     QStringList{"id", "kind", "title", "description", "summary", "source", "content",
                                 "worker", "lease", "target", "relation", "error"})
                    if (options.contains("--" + key))
                        arguments[key] = option("--" + key);
                for (const auto &key : QStringList{"offset", "seconds", "event"})
                    if (options.contains("--" + key)) {
                        bool ok;
                        auto number = option("--" + key).toLongLong(&ok);
                        if (!ok) {
                            QTextStream(stderr) << "Invalid numeric option: " << key << '\n';
                            return 2;
                        }
                        arguments[key] = number;
                    }
                if (options.contains("--ids"))
                    arguments["ids"] = option("--ids").split(',', Qt::SkipEmptyParts);
                if (options.contains("--content-file")) {
                    QString path = option("--content-file");
                    if (path.startsWith("~/"))
                        path = QDir::homePath() + path.mid(1);
                    QFileInfo info(path);
                    QFile file(path);
                    if (!info.isFile() || SessionLedger::privatePath(info.canonicalFilePath()) ||
                        info.size() > 1024 * 1024 || !file.open(QIODevice::ReadOnly)) {
                        QTextStream(stderr)
                            << "Choose a text artifact up to 1 MiB outside credential locations\n";
                        return 2;
                    }
                    QByteArray data = file.readAll();
                    if (data.contains('\0')) {
                        QTextStream(stderr) << "Artifact must be text\n";
                        return 2;
                    }
                    arguments["content"] = QString::fromUtf8(data);
                    arguments["source"] = info.canonicalFilePath();
                }
                result = QJsonObject::fromVariantMap(backend.segmentAction(action, arguments));
            }
        } else if (command == "events")
            result = {{"events", QJsonArray::fromVariantList(
                                     backend.eventPage(category, option("--before", "0").toLongLong()))}};
        else if (command == "read-event")
            result = QJsonObject::fromVariantMap(
                backend.readEvent(option("--id").toLongLong(), option("--offset", "0").toInt()));
        else if (command == "attach")
            result = {{"ok", backend.attachSession(option("--path"))}};
        else if (command == "record")
            result = {{"event", backend.recordEvent(category, option("--kind", "session.note"),
                                                    option("--summary", "Session note"),
                                                    option("--content", QString("")),
                                                    option("--source", QString("")))}};
        else if (command == "focus") {
            int id = backend.focusWorkspace(category, option("--name"), option("--brief"));
            if (id > 0 && options.contains("--folder"))
                backend.updateCategory(id, backend.title(), option("--folder"), backend.brief());
            result = {{"category", id}};
        } else if (command == "git-config") {
            backend.configureGit(option("--paths", QString("")), option("--remote", QString("")),
                                 options.contains("--automatic"), options.contains("--push"));
            result = QJsonObject::fromVariantMap(backend.gitState());
            result["ok"] =
                backend.gitState().value("managed").toBool() &&
                backend.gitState().value("paths").toString() == option("--paths", QString("")) &&
                backend.gitState().value("remote").toString() == option("--remote", QString("")) &&
                backend.gitState().value("automatic").toBool() == options.contains("--automatic") &&
                backend.gitState().value("push").toBool() == options.contains("--push");
        } else if (command == "run") {
            int separator = raw.indexOf("--");
            QStringList args = separator >= 0 ? raw.mid(separator + 1) : QStringList{};
            if (args.isEmpty()) {
                QTextStream(stderr) << "Usage: hotbox run --category ID -- PROGRAM ARGUMENTS\n";
                return 2;
            }
            QStringList safe = args;
            bool secretNext = false;
            for (QString &arg : safe) {
                if (secretNext) {
                    arg = "[REDACTED]";
                    secretNext = false;
                } else if (QRegularExpression("(?i)^--?(password|token|api-key|secret|authorization)$")
                               .match(arg)
                               .hasMatch())
                    secretNext = true;
            }
            QString description = SessionLedger::redact(safe.join(' '));
            QString path = backend.folder();
            if (path.startsWith("~/"))
                path = QDir::homePath() + path.mid(1);
            if (!QFileInfo(path).isDir()) {
                QTextStream(stderr) << "Link an existing workspace directory first\n";
                return 2;
            }
            qint64 started = backend.recordEvent(category, "command.started", description, description, path);
            QProcess process;
            process.setWorkingDirectory(path);
            process.setProcessChannelMode(QProcess::MergedChannels);
            QString program = args.takeFirst();
            process.start(program, args);
            QByteArray output;
            QElapsedTimer elapsed;
            elapsed.start();
            bool timeout = false;
            if (process.waitForStarted(3000)) {
                while (process.state() != QProcess::NotRunning) {
                    process.waitForFinished(100);
                    output += process.readAll();
                    if (output.size() > 240000)
                        output = output.left(240000);
                    if (elapsed.elapsed() > 120000) {
                        timeout = true;
                        process.kill();
                        process.waitForFinished(1000);
                        break;
                    }
                }
                exitCode =
                    timeout ? 124 : (process.exitStatus() == QProcess::NormalExit ? process.exitCode() : 1);
            } else {
                exitCode = 1;
                output = process.errorString().toUtf8();
            }
            output += process.readAll();
            output = output.left(240000);
            QJsonObject detail{{"command", description},
                               {"cwd", path},
                               {"output", SessionLedger::redact(QString::fromUtf8(output))},
                               {"exitCode", exitCode},
                               {"startedEvent", double(started)},
                               {"durationMs", double(elapsed.elapsed())},
                               {"timedOut", timeout}};
            while (QJsonDocument(detail).toJson(QJsonDocument::Compact).size() > 240000) {
                detail["output"] = detail["output"].toString().left(detail["output"].toString().size() / 2);
                detail["outputTruncated"] = true;
            }
            auto event = backend.recordEvent(category, "command.finished",
                                             description + QString(" · exit %1").arg(exitCode),
                                             QString::fromUtf8(QJsonDocument(detail).toJson()), path);
            result = {{"event", double(event)}, {"exitCode", exitCode}};
        } else {
            if (command == "snapshot")
                backend.captureGit();
            else if (command == "branch")
                backend.forkGit();
            else
                backend.checkpointGit();
            if (backend.taskBusy()) {
                QEventLoop loop;
                QObject::connect(&backend, &Backend::activityChanged, &loop, [&] {
                    if (!backend.taskBusy())
                        loop.quit();
                });
                loop.exec();
            }
            result = {{"ok", backend.lastGitResult().value("ok").toBool()},
                      {"result", QJsonObject::fromVariantMap(backend.lastGitResult())},
                      {"git", QJsonObject::fromVariantMap(backend.gitState())}};
        }
        if (!backend.status().isEmpty())
            result["status"] = backend.status();
        if (result.contains("error") || (result.contains("ok") && !result["ok"].toBool()) ||
            (result.contains("category") && result["category"].toInt() < 1) ||
            (result.contains("event") && result["event"].toDouble() < 1))
            exitCode = 1;
        QTextStream(stdout) << QJsonDocument(result).toJson(QJsonDocument::Compact) << '\n';
        return exitCode;
    }
    if (check) {
        QJsonObject result{{"ok", backend.status().isEmpty()},
                           {"categories", int(backend.categories().size())},
                           {"modelConfigured", !backend.model().isEmpty()},
                           {"version", app.applicationVersion()}};
        QTextStream(stdout) << QJsonDocument(result).toJson(QJsonDocument::Compact) << '\n';
        return result["ok"].toBool() ? 0 : 1;
    }
    QLocalServer server;
    if (!smoke) {
        QLocalServer::removeServer(serverPath);
        server.setSocketOptions(QLocalServer::UserAccessOption);
        if (!server.listen(serverPath)) {
            QTextStream(stderr) << server.errorString() << '\n';
            return 1;
        }
    }
    QQuickStyle::setStyle("Material");
    Theme theme(smoke ? dataDir + "/preview-theme" : QString());
    Translator translator;
    app.installTranslator(&translator);
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("backend", &backend);
    engine.rootContext()->setContextProperty("theme", &theme);
    engine.rootContext()->setContextProperty("translator", &translator);
    QObject::connect(&translator, &Translator::changed, &engine, &QQmlEngine::retranslate);
    engine.load(QUrl("qrc:/Main.qml"));
    if (engine.rootObjects().isEmpty())
        return 1;
    auto mainWindow = qobject_cast<QQuickWindow *>(engine.rootObjects().first());
    auto menu = mainWindow->findChild<QQuickWindow *>("hotboxOverlay");
    if (!menu)
        return 2;
    LayerShellQt::Window *layer = nullptr;
    if (app.platformName() == "wayland") {
        layer = LayerShellQt::Window::get(menu);
        layer->setScope("hotbox-overlay");
        layer->setLayer(LayerShellQt::Window::LayerOverlay);
        layer->setAnchors(LayerShellQt::Window::Anchors(LayerShellQt::Window::AnchorTop) |
                          LayerShellQt::Window::AnchorBottom | LayerShellQt::Window::AnchorLeft |
                          LayerShellQt::Window::AnchorRight);
        layer->setExclusiveZone(-1);
        layer->setKeyboardInteractivity(LayerShellQt::Window::KeyboardInteractivityNone);
    }
    QObject::connect(&backend, &Backend::showMenu, &app, [&] {
        QPoint pos = smoke ? QPoint(400, 350) : QCursor::pos();
        QScreen *screen = QGuiApplication::screenAt(pos);
        if (app.platformName() == "wayland") {
            auto point = hypr("cursorpos").object();
            if (point.contains("x"))
                pos = QPoint(point["x"].toInt(), point["y"].toInt());
            // Match the compositor's logical monitor geometry, including scale and transform.
            const auto monitors = hypr("monitors").array();
            for (const auto &v : monitors) {
                auto m = v.toObject();
                double scale = m["scale"].toDouble(1);
                int w = qRound(m["width"].toInt() / scale), h = qRound(m["height"].toInt() / scale);
                if (m["transform"].toInt() % 2)
                    qSwap(w, h);
                QRect rect(m["x"].toInt(), m["y"].toInt(), w, h);
                if (rect.contains(pos)) {
                    for (auto *s : app.screens())
                        if (s->name() == m["name"].toString())
                            screen = s;
                    break;
                }
            }
        }
        if (!screen)
            screen = app.primaryScreen();
        if (!screen)
            return;
        if (layer)
            layer->setScreen(screen);
        menu->setScreen(screen);
        QRect geometry = screen->geometry();
        menu->setGeometry(geometry);
        menu->setProperty("anchorX", qBound(12, pos.x() - geometry.x(), geometry.width() - 12));
        menu->setProperty("anchorY", qBound(12, pos.y() - geometry.y(), geometry.height() - 12));
        QMetaObject::invokeMethod(menu, "resetMenu");
        menu->show();
    });
    auto handle = [&](const QString &cmd) {
        if (cmd == "hold")
            backend.hold();
        else if (cmd == "release")
            backend.release();
        else if (cmd == "menu")
            backend.preview();
        else if (cmd == "quit") {
            mainWindow->setProperty("allowClose", true);
            app.quit();
        } else if (cmd == "keybindings")
            backend.launch("keybindings");
        else if (cmd == "open")
            backend.openChat();
    };
    QObject::connect(&server, &QLocalServer::newConnection, &app, [&] {
        while (server.hasPendingConnections()) {
            auto *socket = server.nextPendingConnection();
            socket->setParent(&server);
            QObject::connect(socket, &QLocalSocket::readyRead, socket, [&, socket] {
                if (socket->canReadLine()) {
                    QString cmd = QString::fromUtf8(socket->readLine()).trimmed();
                    handle(cmd);
                    socket->disconnectFromServer();
                }
            });
            QObject::connect(socket, &QLocalSocket::disconnected, socket, &QObject::deleteLater);
            QTimer::singleShot(1000, socket, &QObject::deleteLater);
        }
    });
    if (smoke) {
        const QString output = raw[raw.indexOf("--smoke") + 1];
        QDir().mkpath(output);
        backend.addCategory("Weather app", "/tmp/weather", "Build an hourly weather display.");
        backend.recordEvent(
            backend.currentId(), "command.finished", "Sample build passed",
            "Command: make -j4\nWorking directory: /tmp/weather\nExit code: 0\nAll checks passed.",
            "sample session");
        auto sample = backend.segmentAction(
            "create", {{"kind", "web_search"},
                       {"title", "Desktop rendering research"},
                       {"description", "Sample research capsule for the interface preview"}});
        QString sampleId = sample["id"].toString();
        backend.segmentAction("add", {{"id", sampleId},
                                      {"kind", "input"},
                                      {"content", "Compare native and web-based desktop interfaces."}});
        backend.segmentAction(
            "add",
            {{"id", sampleId},
             {"kind", "output"},
             {"source", "https://example.invalid/sample-notes"},
             {"content", "Sample finding: compare native rendering and web views for desktop integration."}});
        backend.segmentAction(
            "add",
            {{"id", sampleId}, {"kind", "script"}, {"content", "print('summarize saved findings')\n"}});
        backend.segmentAction("ready", {{"id", sampleId}});
        backend.segmentAction("process");
        backend.openChat();
        QTimer::singleShot(600, &app, [&, output] {
            mainWindow->grabWindow().save(output + "/chat.png");
            mainWindow->setProperty("page", "timeline");
        });
        QTimer::singleShot(1000, &app, [&, output, sampleId] {
            mainWindow->grabWindow().save(output + "/timeline.png");
            mainWindow->setProperty("page", "segments");
            auto view = mainWindow->findChild<QObject *>("segmentsView");
            if (view)
                view->setProperty("selected", backend.segmentAction("read", {{"id", sampleId}}));
        });
        QTimer::singleShot(1600, &app, [&, output] {
            mainWindow->grabWindow().save(output + "/segments.png");
            backend.preview();
        });
        QTimer::singleShot(2200, &app, [&, output] {
            menu->grabWindow().save(output + "/hotbox.png");
            backend.release();
            backend.select(1);
            backend.architectureAction("init");
            backend.addCategory("Media workspace", "", "Sample reusable libraries", 1);
            backend.architectureAction("ensure", {{"capabilities", QStringList{"music", "album-covers"}},
                                                  {"artist", "Sample Band"},
                                                  {"album", "Sample Album"},
                                                  {"genre", "Rock"}});
            backend.clearStatus();
            mainWindow->setProperty("page", "architecture");
        });
        QTimer::singleShot(2600, &app, [&, output] {
            mainWindow->grabWindow().save(output + "/architecture.png");
            QJsonObject result{{"ok", true},
                               {"architecture", QFile::exists(output + "/architecture.png")},
                               {"chat", QFile::exists(output + "/chat.png")},
                               {"hotbox", QFile::exists(output + "/hotbox.png")}};
            QTextStream(stdout) << QJsonDocument(result).toJson(QJsonDocument::Compact) << '\n';
            mainWindow->setProperty("allowClose", true);
            app.quit();
        });
    } else if (command != "daemon")
        QTimer::singleShot(0, &app, [&] { handle(command); });
    return app.exec();
}
