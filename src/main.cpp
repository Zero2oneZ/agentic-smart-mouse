#include "backend.h"
#include "theme.h"
#include "translator.h"
#include <QCursor>
#include <QDir>
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
    if (check || smoke)
        qputenv("QT_QPA_PLATFORM", "offscreen");
    if (smoke)
        qputenv("QT_QUICK_BACKEND", "software");
    QGuiApplication app(argc, argv);
    app.setApplicationName("hotbox");
    app.setApplicationVersion("0.1.0");
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
    QString serverPath = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation) + "/hotbox-ipc";
    QLocalSocket client;
    if (!smoke && !check) {
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
    Theme theme;
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
        backend.openChat();
        QTimer::singleShot(600, &app, [&, output] {
            mainWindow->grabWindow().save(output + "/chat.png");
            backend.preview();
        });
        QTimer::singleShot(1300, &app, [&, output] {
            menu->grabWindow().save(output + "/hotbox.png");
            backend.release();
        });
        QTimer::singleShot(1700, &app, [&, output] {
            QJsonObject result{{"ok", true},
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
