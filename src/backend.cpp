#include "backend.h"
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QUrl>
#include <QUuid>

Backend::Backend(const QString &directory, QObject *parent) : QObject(parent) {
    m_directory =
        directory.isEmpty() ? QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) : directory;
    QDir().mkpath(m_directory);
    QFile::setPermissions(m_directory, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
    m_connection = QUuid::createUuid().toString();
    m_db = QSqlDatabase::addDatabase("QSQLITE", m_connection);
    m_db.setDatabaseName(m_directory + "/hotbox.sqlite3");
    if (!m_db.open()) {
        setStatus(tr("Cannot open memory database: %1").arg(m_db.lastError().text()));
        return;
    }
    QFile::setPermissions(m_db.databaseName(), QFile::ReadOwner | QFile::WriteOwner);
    execute("PRAGMA foreign_keys=ON");
    execute("PRAGMA journal_mode=WAL");
    execute("CREATE TABLE IF NOT EXISTS categories(id INTEGER PRIMARY KEY, parent INTEGER REFERENCES "
            "categories(id), name TEXT NOT NULL, folder TEXT NOT NULL DEFAULT '', brief TEXT NOT NULL "
            "DEFAULT '', archived INTEGER NOT NULL DEFAULT 0)");
    execute(
        "CREATE TABLE IF NOT EXISTS messages(id INTEGER PRIMARY KEY, category INTEGER NOT NULL REFERENCES "
        "categories(id), role TEXT NOT NULL, content TEXT NOT NULL, created TEXT DEFAULT CURRENT_TIMESTAMP)");
    execute("CREATE TABLE IF NOT EXISTS settings(name TEXT PRIMARY KEY,value TEXT NOT NULL)");
    execute("INSERT OR IGNORE INTO categories(id,parent,name) VALUES(1,NULL,'Master chat')");
    m_hold.setSingleShot(true);
    m_hold.setInterval(500);
    connect(&m_hold, &QTimer::timeout, this, [this] {
        if (m_held) {
            m_expanded = true;
            emit expandedChanged();
            emit showMenu();
        }
    });
    connect(&m_bindingProcess, &QProcess::errorOccurred, this, [this] {
        setStatus(tr("Cannot read Omarchy keybindings: %1").arg(m_bindingProcess.errorString()));
    });
    connect(&m_bindingProcess, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
            [this](int code, QProcess::ExitStatus) {
                if (code != 0) {
                    setStatus(tr("Cannot read Omarchy keybindings: %1")
                                  .arg(QString::fromUtf8(m_bindingProcess.readAllStandardError()).trimmed()));
                    return;
                }
                const QString output = QString::fromUtf8(m_bindingProcess.readAllStandardOutput());
                m_bindings.clear();
                for (const QString &line : output.split('\n', Qt::SkipEmptyParts))
                    m_bindings.append(line.trimmed());
                emit bindingsChanged();
            });
}
Backend::~Backend() {
    if (m_reply) {
        m_reply->disconnect(this);
        m_reply->abort();
    }
    if (m_bindingProcess.state() != QProcess::NotRunning) {
        m_bindingProcess.kill();
        m_bindingProcess.waitForFinished(1000);
    }
    m_db.close();
    m_db = QSqlDatabase();
    QSqlDatabase::removeDatabase(m_connection);
}
bool Backend::execute(const QString &sql, const QVariantList &args) const {
    QSqlQuery q(m_db);
    q.prepare(sql);
    for (const auto &arg : args)
        q.addBindValue(arg);
    if (!q.exec()) {
        const_cast<Backend *>(this)->setStatus(tr("Memory database error: %1").arg(q.lastError().text()));
        return false;
    }
    return true;
}
QString Backend::field(int id, const QString &name) const {
    if (name != "name" && name != "folder" && name != "brief")
        return {};
    QSqlQuery q(m_db);
    q.prepare("SELECT " + name + " FROM categories WHERE id=?");
    q.addBindValue(id);
    q.exec();
    return q.next() ? q.value(0).toString() : QString();
}
QString Backend::title() const { return field(m_current, "name"); }
QString Backend::folder() const { return field(m_current, "folder"); }
QString Backend::brief() const { return field(m_current, "brief"); }
QVariantList Backend::categories() const {
    QVariantList list;
    QSqlQuery q("SELECT id,COALESCE(parent,0),name,folder,brief FROM categories WHERE archived=0 ORDER BY id",
                m_db);
    while (q.next())
        list.append(QVariantMap{{"id", q.value(0)},
                                {"parent", q.value(1)},
                                {"name", q.value(2)},
                                {"folder", q.value(3)},
                                {"brief", q.value(4)}});
    return list;
}
QVariantList Backend::archivedCategories() const {
    QVariantList list;
    QSqlQuery q("SELECT id,name FROM categories WHERE archived=1 ORDER BY id", m_db);
    while (q.next())
        list.append(QVariantMap{{"id", q.value(0)}, {"name", q.value(1)}});
    return list;
}
QVariantList Backend::children(int parent) const {
    QVariantList list;
    for (const auto &v : categories()) {
        auto m = v.toMap();
        if (m["parent"].toInt() == parent && m["id"].toInt() != 1)
            list.append(m);
    }
    return list;
}
QVariantList Backend::messages() const {
    QVariantList list;
    QSqlQuery q(m_db);
    q.prepare("SELECT id,role,content,created FROM messages WHERE category=? ORDER BY id");
    q.addBindValue(m_current);
    q.exec();
    while (q.next())
        list.append(QVariantMap{
            {"id", q.value(0)}, {"role", q.value(1)}, {"content", q.value(2)}, {"created", q.value(3)}});
    return list;
}
void Backend::select(int id) {
    if (field(id, "name").isEmpty())
        return;
    m_current = id;
    emit changed();
}
int Backend::addCategory(const QString &name, const QString &folder, const QString &brief, int parent) {
    if (name.trimmed().isEmpty()) {
        setStatus(tr("Give the category a name."));
        return -1;
    }
    if (parent && field(parent, "name").isEmpty()) {
        setStatus(tr("Parent category does not exist."));
        return -1;
    }
    QSqlQuery q(m_db);
    q.prepare("INSERT INTO categories(parent,name,folder,brief) VALUES(?,?,?,?)");
    q.addBindValue(parent ? QVariant(parent) : QVariant());
    q.addBindValue(name.trimmed());
    q.addBindValue(folder.trimmed());
    q.addBindValue(brief);
    if (!q.exec()) {
        setStatus(q.lastError().text());
        return -1;
    }
    m_current = q.lastInsertId().toInt();
    emit changed();
    return m_current;
}
void Backend::updateCategory(int id, const QString &name, const QString &folder, const QString &brief) {
    if (name.trimmed().isEmpty())
        return;
    if (execute("UPDATE categories SET name=?,folder=?,brief=? WHERE id=?",
                {name.trimmed(), folder.trimmed(), brief, id}))
        emit changed();
}
void Backend::archive(int id) {
    if (id == 1)
        return;
    // Archive descendants with their parent so no visible menu entries become orphaned.
    if (execute("WITH RECURSIVE branch(id) AS (SELECT id FROM categories WHERE id=? UNION ALL SELECT c.id "
                "FROM categories c JOIN branch b ON c.parent=b.id) UPDATE categories SET archived=1 WHERE id "
                "IN (SELECT id FROM branch)",
                {id})) {
        m_current = 1;
        emit changed();
    }
}
void Backend::restore(int id) {
    // Restore ancestors and descendants together, retaining the original hierarchy.
    if (execute("WITH RECURSIVE branch(id) AS (SELECT id FROM categories WHERE id=? UNION ALL SELECT c.id "
                "FROM categories c JOIN branch b ON c.parent=b.id), ancestors(id,parent) AS (SELECT "
                "id,parent FROM categories WHERE id=? UNION ALL SELECT c.id,c.parent FROM categories c JOIN "
                "ancestors a ON c.id=a.parent) UPDATE categories SET archived=0 WHERE id IN (SELECT id FROM "
                "branch UNION SELECT id FROM ancestors)",
                {id, id}))
        emit changed();
}
QString Backend::setting(const QString &name, const QString &fallback) const {
    QSqlQuery q(m_db);
    q.prepare("SELECT value FROM settings WHERE name=?");
    q.addBindValue(name);
    q.exec();
    return q.next() ? q.value(0).toString() : fallback;
}
void Backend::saveSettings(const QString &endpoint, const QString &model, const QString &keyVariable) {
    QUrl url(endpoint);
    if (!url.isValid() || url.host().isEmpty() || (url.scheme() != "https" && url.scheme() != "http")) {
        setStatus(tr("Enter a valid HTTP or HTTPS chat endpoint."));
        return;
    }
    if (url.scheme() == "http" && url.host() != "127.0.0.1" && url.host() != "localhost" &&
        url.host() != "::1") {
        setStatus(tr("Use HTTPS for a remote model endpoint."));
        return;
    }
    if (!QRegularExpression("^[A-Za-z_][A-Za-z0-9_]*$").match(keyVariable).hasMatch()) {
        setStatus(tr("Enter a valid environment variable name for the API key."));
        return;
    }
    for (const auto &pair : QList<QPair<QString, QString>>{{"endpoint", endpoint.trimmed()},
                                                           {"model", model.trimmed()},
                                                           {"key_variable", keyVariable.trimmed()}})
        execute("INSERT INTO settings(name,value) VALUES(?,?) ON CONFLICT(name) DO UPDATE SET "
                "value=excluded.value",
                {pair.first, pair.second});
    emit settingsChanged();
    setStatus(tr("Model settings saved."));
}
int Backend::append(int category, const QString &role, const QString &content) {
    QSqlQuery q(m_db);
    q.prepare("INSERT INTO messages(category,role,content) VALUES(?,?,?)");
    q.addBindValue(category);
    q.addBindValue(role);
    q.addBindValue(content);
    if (!q.exec()) {
        setStatus(q.lastError().text());
        return 0;
    }
    emit changed();
    return q.lastInsertId().toInt();
}
void Backend::updateMessage(int id, const QString &content) {
    execute("UPDATE messages SET content=? WHERE id=?", {content, id});
    emit changed();
}
void Backend::setStatus(const QString &text) {
    m_status = text;
    emit statusChanged();
}
void Backend::clearStatus() { setStatus({}); }
void Backend::send(const QString &text) {
    if (text.trimmed().isEmpty() || busy())
        return;
    if (!append(m_current, "user", text.trimmed()))
        return;
    if (model().isEmpty()) {
        setStatus(tr("Message saved. Open Model settings to connect a model for replies."));
        return;
    }
    QJsonArray context;
    QString system = tr("You are the assistant for this local workspace. Project briefs are user-maintained "
                        "context. You can discuss and propose changes, but this chat connection cannot "
                        "execute commands or edit files. Never claim to have done so.");
    system +=
        "\nWorkspace: " + title() + "\nFolder: " + folder() + "\nBrief and saved decisions:\n" + brief();
    if (m_current == 1) {
        system += "\nWorkspace directory (summaries only; other conversations are not included):";
        for (const auto &v : categories()) {
            const auto c = v.toMap();
            system +=
                "\n" + c["name"].toString() + " | " + c["folder"].toString() + " | " + c["brief"].toString();
        }
    }
    context.append(QJsonObject{{"role", "system"}, {"content", system}});
    // Include a bounded recent window; the project brief is always present.
    const auto history = messages();
    int start = qMax(0, int(history.size()) - 40);
    for (int i = start; i < history.size(); ++i) {
        const auto m = history[i].toMap();
        if (!m["content"].toString().isEmpty())
            context.append(QJsonObject{{"role", m["role"].toString()}, {"content", m["content"].toString()}});
    }
    QNetworkRequest request{QUrl(endpoint())};
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    request.setTransferTimeout(120000);
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    const QByteArray token = qgetenv(keyVariable().toUtf8().constData());
    if (!token.isEmpty())
        request.setRawHeader("Authorization", "Bearer " + token);
    m_replyCategory = m_current;
    m_answer.clear();
    m_buffer.clear();
    m_sawDone = false;
    m_replyMessage = append(m_current, "assistant", "");
    if (!m_replyMessage)
        return;
    m_reply = m_network.post(
        request, QJsonDocument(QJsonObject{{"model", model()}, {"messages", context}, {"stream", true}})
                     .toJson(QJsonDocument::Compact));
    emit busyChanged();
    clearStatus();
    connect(m_reply, &QNetworkReply::readyRead, this, [this] { consumeStream(); });
    connect(m_reply, &QNetworkReply::finished, this, [this] {
        if (!m_reply)
            return;
        consumeStream(true);
        const int code = m_reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if (m_reply->error() != QNetworkReply::NoError)
            setStatus(
                tr("Reply failed (%1): %2. Your message is saved.").arg(code).arg(m_reply->errorString()));
        else if (code >= 300)
            setStatus(tr("Model endpoint returned HTTP %1.").arg(code));
        else if (m_answer.isEmpty())
            setStatus(tr("The model returned no text. Check its endpoint and model name."));
        else if (!m_sawDone &&
                 m_reply->header(QNetworkRequest::ContentTypeHeader).toString().contains("event-stream"))
            setStatus(tr("Connection ended before completion; the partial reply is saved."));
        if (m_answer.isEmpty())
            execute("DELETE FROM messages WHERE id=?", {m_replyMessage});
        m_reply->deleteLater();
        m_reply = nullptr;
        emit busyChanged();
        emit changed();
    });
}
void Backend::consumeStream(bool final) {
    if (!m_reply)
        return;
    m_buffer += m_reply->readAll();
    const QString type = m_reply->header(QNetworkRequest::ContentTypeHeader).toString();
    if (!type.contains("event-stream")) {
        if (final) {
            const auto o = QJsonDocument::fromJson(m_buffer).object();
            const auto choices = o["choices"].toArray();
            if (!choices.isEmpty())
                m_answer = choices[0].toObject()["message"].toObject()["content"].toString();
            if (o.contains("error"))
                setStatus(tr("The model endpoint reported an error."));
            if (!m_answer.isEmpty())
                updateMessage(m_replyMessage, m_answer);
        }
        return;
    }
    bool changed = false;
    while (m_buffer.contains('\n') || (final && !m_buffer.isEmpty())) {
        int end = m_buffer.indexOf('\n');
        if (end < 0)
            end = m_buffer.size();
        QByteArray line = m_buffer.left(end).trimmed();
        m_buffer.remove(0, qMin(end + 1, int(m_buffer.size())));
        if (!line.startsWith("data:"))
            continue;
        line = line.mid(5).trimmed();
        if (line == "[DONE]") {
            m_sawDone = true;
            continue;
        }
        const auto o = QJsonDocument::fromJson(line).object();
        const auto choices = o["choices"].toArray();
        if (!choices.isEmpty()) {
            auto choice = choices[0].toObject();
            m_answer += choice["delta"].toObject()["content"].toString();
            if (!choice["finish_reason"].isNull() && !choice["finish_reason"].isUndefined())
                m_sawDone = true;
            changed = true;
        }
    }
    if (changed)
        updateMessage(m_replyMessage, m_answer);
}
void Backend::cancel() {
    if (m_reply) {
        m_reply->abort();
        setStatus(tr("Generation stopped. Any partial reply is saved."));
    }
}
void Backend::refreshBindings() {
    if (m_bindingProcess.state() != QProcess::NotRunning)
        return;
    m_bindingProcess.start("omarchy", {"menu", "keybindings", "--print"});
}
void Backend::launch(const QString &action, int id) {
    release();
    if (id < 0)
        id = m_current;
    if (action == "folder") {
        QString path = field(id, "folder");
        if (path.isEmpty())
            path = QDir::homePath() + "/Work";
        if (path.startsWith("~/"))
            path = QDir::homePath() + path.mid(1);
        if (!QFileInfo(path).isDir()) {
            setStatus(tr("Folder does not exist: %1. Edit the workspace location first.").arg(path));
            emit showChat();
            return;
        }
        if (!QDesktopServices::openUrl(QUrl::fromLocalFile(path)))
            setStatus(tr("Could not open the folder."));
    } else if (action == "browser") {
        if (!QProcess::startDetached("omarchy", {"launch", "browser"}))
            setStatus(tr("Could not launch the browser."));
    } else if (action == "terminal") {
        QString path = field(id, "folder");
        if (path.startsWith("~/"))
            path = QDir::homePath() + path.mid(1);
        if (path.isEmpty())
            path = QDir::homePath() + "/Work";
        if (!QFileInfo(path).isDir()) {
            setStatus(tr("Workspace folder does not exist."));
            emit showChat();
            return;
        }
        if (!QProcess::startDetached("xdg-terminal-exec", {"--dir=" + path}))
            setStatus(tr("Could not launch the terminal."));
    } else if (action == "keybindings") {
        emit showBindings();
        refreshBindings();
    } else if (action == "generate") {
        openChat(id);
        setStatus(
            tr("Describe what you want to generate in the chat. File-building tools are not connected yet."));
    } else if (action == "chat")
        openChat(id);
}
void Backend::hold() {
    if (m_held)
        return;
    m_held = true;
    m_hold.start();
}
void Backend::release() {
    m_held = false;
    m_hold.stop();
    if (m_expanded) {
        m_expanded = false;
        emit expandedChanged();
    }
}
void Backend::preview() {
    m_held = true;
    m_hold.stop();
    m_expanded = true;
    emit expandedChanged();
    emit showMenu();
}
void Backend::openChat(int id) {
    release();
    select(id);
    emit showChat();
}
