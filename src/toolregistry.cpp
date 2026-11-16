#include "backend.h"
#include <QCoreApplication>
#include <QDateTime>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QSqlQuery>
#include <QUuid>

namespace {
QString fresh(const QString &prefix) { return prefix + QUuid::createUuid().toString(QUuid::Id128); }
bool identifier(const QString &text) {
    return QRegularExpression("^[A-Za-z0-9_-][A-Za-z0-9_.-]{0,95}$").match(text).hasMatch();
}
QVariantMap failure(const QString &text) { return {{"ok", false}, {"error", text}}; }
QByteArray json(const QVariantMap &value) {
    return QJsonDocument(QJsonObject::fromVariantMap(value)).toJson(QJsonDocument::Compact);
}
bool labels(const QVariantMap &a) {
    const auto name = a.value("name").toString().trimmed(),
               group = a.value("menu_category").toString().trimmed();
    return !name.isEmpty() && name.toUtf8().size() <= 200 && !group.isEmpty() &&
           group.toUtf8().size() <= 100 && a.value("purpose").toString().toUtf8().size() <= 4000 &&
           !name.contains(QRegularExpression("[\\x00-\\x1f\\x7f]")) &&
           !group.contains(QRegularExpression("[\\x00-\\x1f\\x7f]"));
}
} // namespace

void Backend::initToolRegistry(bool automatic) {
    execute(
        "CREATE TABLE IF NOT EXISTS tool_context_bindings(tool_id TEXT PRIMARY KEY,owner_category INTEGER "
        "NOT NULL REFERENCES categories(id),chat INTEGER NOT NULL UNIQUE REFERENCES categories(id),chat_ref "
        "TEXT NOT NULL UNIQUE,metadata TEXT NOT NULL DEFAULT '{}')");
    execute("CREATE TABLE IF NOT EXISTS tool_registry_outbox(request_id TEXT PRIMARY KEY,tool_id TEXT NOT "
            "NULL REFERENCES tool_context_bindings(tool_id),document TEXT NOT NULL,state TEXT NOT NULL "
            "DEFAULT 'pending',response TEXT NOT NULL DEFAULT '',error TEXT NOT NULL DEFAULT '')");
    execute("CREATE TABLE IF NOT EXISTS tool_wheel_items(tool_id TEXT PRIMARY KEY REFERENCES "
            "tool_context_bindings(tool_id),favorite INTEGER NOT NULL DEFAULT 0,pinned INTEGER NOT NULL "
            "DEFAULT 0,used INTEGER NOT NULL DEFAULT 0)");
    // Cached labels can be displayed, but access/action eligibility requires a fresh core read.
    QProcessEnvironment env;
    env.insert("LANG", "C.UTF-8");
    m_toolProcess.setProcessEnvironment(env);
    m_toolTimeout.setSingleShot(true);
    m_toolTimeout.setInterval(8000);
    connect(&m_toolTimeout, &QTimer::timeout, this, [this] {
        m_toolProblem = tr("Tool registry timed out. Saved requests can be retried.");
        m_toolProcess.kill();
    });
    connect(&m_toolProcess, &QProcess::readyReadStandardOutput, this, [this] {
        m_toolOutput += m_toolProcess.readAllStandardOutput();
        if (m_toolOutput.size() > 262144) {
            m_toolProblem = tr("Tool registry response is too large.");
            m_toolProcess.kill();
        }
    });
    connect(&m_toolProcess, &QProcess::readyReadStandardError, this,
            [this] { m_toolProcess.readAllStandardError(); });
    connect(&m_toolProcess, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            m_toolTimeout.stop();
            m_toolReady = false;
            m_toolProblem = tr("Tool registry helper could not start. Update Hotbox and retry.");
            emit toolsChanged();
        }
    });
    connect(&m_toolProcess, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
            [this](int code, QProcess::ExitStatus) { finishToolCore(code); });
    connect(this, &Backend::changed, this, &Backend::toolsChanged);
    if (automatic)
        QTimer::singleShot(0, this, &Backend::refreshToolRegistry);
}

QVariantMap Backend::boundTool(const QString &tool) const {
    QSqlQuery q(m_db);
    q.prepare("SELECT b.owner_category,b.chat,b.chat_ref,b.metadata FROM tool_context_bindings b "
              "JOIN categories p ON p.id=b.owner_category JOIN categories c ON c.id=b.chat WHERE b.tool_id=? "
              "AND p.archived=0 AND c.archived=0");
    q.addBindValue(tool);
    if (!q.exec() || !q.next() ||
        (!m_ledger->contains(m_current, q.value(0).toInt()) && m_current != q.value(1).toInt()))
        return {};
    auto entry = QJsonDocument::fromJson(q.value(3).toByteArray()).object().toVariantMap();
    entry["tool_id"] = tool;
    entry["owner_category"] = q.value(0);
    entry["chat"] = q.value(1);
    entry["chat_ref"] = q.value(2);
    entry["verified"] = m_verifiedTools.contains(tool) && m_toolReady;
    return entry;
}

QVariantList Backend::registeredTools() const {
    QVariantList rows;
    QSqlQuery q("SELECT tool_id FROM tool_context_bindings ORDER BY tool_id", m_db);
    while (q.next()) {
        auto row = boundTool(q.value(0).toString());
        if (row.isEmpty())
            continue;
        if (!row.contains("name")) {
            row["name"] = field(row["chat"].toInt(), "name");
            row["menu_category"] = tr("Pending");
        }
        row["action"] = "registered_tool";
        row["glyph"] = "⚒";
        row["run_available"] = false;
        row["run_reason"] = tr("This draft has no runnable release yet.");
        QSqlQuery pending(m_db);
        pending.prepare("SELECT state,error FROM tool_registry_outbox WHERE tool_id=? AND state<>'accepted' "
                        "ORDER BY rowid DESC LIMIT 1");
        pending.addBindValue(row["tool_id"]);
        if (pending.exec() && pending.next()) {
            row["sync_state"] = pending.value(0);
            row["sync_error"] = pending.value(1);
        } else
            row["sync_state"] = "saved";
        rows.append(row);
    }
    return rows;
}

void Backend::requestToolCore(const QString &stage, const QVariantMap &request) {
    m_toolStage = stage;
    m_toolOutput.clear();
    const QString candidate =
        qEnvironmentVariable("HOTBOX_CORE_BINARY", QCoreApplication::applicationDirPath() + "/hotbox-core");
    const QFileInfo file(candidate);
    const QString helper =
        file.isFile() && file.isExecutable() && !file.isSymLink() ? file.canonicalFilePath() : QString();
    if (helper.isEmpty()) {
        m_toolReady = false;
        m_toolProblem = tr("Tools need the Hotbox core helper. Update Hotbox, then retry.");
        emit toolsChanged();
        return;
    }
    m_toolTimeout.start();
    m_toolProcess.start(helper, stage == "probe" ? QStringList{"tool-menu-catalog"}
                                                 : QStringList{m_directory + "/orchestration.sqlite3"});
    if (stage != "probe") {
        m_toolProcess.write(json(request));
        m_toolProcess.closeWriteChannel();
    }
    emit toolsChanged();
}

void Backend::refreshToolRegistry() {
    if (toolRegistryBusy())
        return;
    m_toolReady = false;
    m_toolScanning = false;
    m_toolScan.clear();
    m_toolProblem.clear();
    m_verifiedTools.clear();
    requestToolCore("probe");
}

void Backend::retryToolRegistry() {
    if (toolRegistryBusy())
        return;
    execute("UPDATE tool_registry_outbox SET state='pending',error='' WHERE state='error'");
    refreshToolRegistry();
}

void Backend::advanceToolRegistry() {
    if (!m_toolReady || toolRegistryBusy())
        return;
    QSqlQuery pending(m_db);
    pending.prepare(
        "SELECT request_id,document FROM tool_registry_outbox WHERE state='pending' ORDER BY rowid LIMIT 1");
    if (pending.exec() && pending.next()) {
        m_toolRequest = pending.value(0).toString();
        m_toolMutation = QJsonDocument::fromJson(pending.value(1).toByteArray()).object().toVariantMap();
        if (m_toolMutation["command"] == "tool_menu_register") {
            const auto payload = m_toolMutation["request"].toMap();
            requestToolCore(
                "resource",
                {{"command", "resource"}, {"actor", "local-user"}, {"resource", payload["resource"]}});
        } else
            requestToolCore("mutation", m_toolMutation);
        return;
    }
    m_toolRequest.clear();
    if (!m_toolScanning) {
        m_toolScanning = true;
        QSqlQuery bindings("SELECT tool_id FROM tool_context_bindings ORDER BY tool_id", m_db);
        while (bindings.next())
            m_toolScan << bindings.value(0).toString();
    }
    if (!m_toolScan.isEmpty()) {
        const QString tool = m_toolScan.takeFirst();
        requestToolCore("read", {{"command", "tool_menu_read"}, {"actor", "local-user"}, {"tool_id", tool}});
        m_toolMutation = {{"tool_id", tool}};
        return;
    }
    emit toolsChanged();
}

bool Backend::projectToolEntry(const QVariantMap &entry) {
    QSqlQuery q(m_db);
    q.prepare("SELECT owner_category,chat_ref,metadata,chat FROM tool_context_bindings WHERE tool_id=?");
    q.addBindValue(entry["tool_id"]);
    if (!q.exec() || !q.next() || entry["resource"].toString() != "workspace-" + q.value(0).toString() ||
        entry["originating_chat"] != q.value(1) || entry["revision"].toLongLong() < 1 || !labels(entry))
        return false;
    const auto current = QJsonDocument::fromJson(q.value(2).toByteArray()).object();
    const qint64 previous = current["revision"].toInteger(), incoming = entry["revision"].toLongLong();
    // An idempotency receipt describes its original revision, not necessarily current state.
    if (previous > incoming)
        return true;
    if (previous == incoming && previous > 0 && current != QJsonObject::fromVariantMap(entry))
        return false;
    const int chat = q.value(3).toInt();
    const bool rename =
        field(chat, "name") == current["name"].toString() && field(chat, "name") != entry["name"].toString();
    if (!m_db.transaction())
        return false;
    bool ok = execute("UPDATE tool_context_bindings SET metadata=? WHERE tool_id=?",
                      {QString::fromUtf8(json(entry)), entry["tool_id"]});
    if (ok && rename)
        ok = execute("UPDATE categories SET name=? WHERE id=?", {entry["name"], chat});
    if (!ok || !m_db.commit()) {
        m_db.rollback();
        return false;
    }
    m_verifiedTools.insert(entry["tool_id"].toString());
    if (rename)
        emit changed();
    return true;
}

void Backend::finishToolCore(int code) {
    m_toolTimeout.stop();
    m_toolOutput += m_toolProcess.readAllStandardOutput();
    const auto envelope = QJsonDocument::fromJson(m_toolOutput).object();
    const bool ok = code == 0 && envelope["ok"].toBool() && m_toolOutput.size() <= 262144;
    auto rejectRequest = [this] {
        if (!m_toolRequest.isEmpty() &&
            !execute("UPDATE tool_registry_outbox SET state='error',error=? WHERE request_id=?",
                     {m_toolProblem, m_toolRequest})) {
            m_toolReady = false;
            m_toolProblem =
                tr("Could not save registry failure state. Requests are kept; retry after checking storage.");
        }
    };
    if (!ok) {
        m_toolProblem = envelope["error"].toString(
            tr("Tool registry request failed. Saved requests are kept; retry after checking setup."));
        if (m_toolStage == "probe") {
            m_toolReady = false;
            emit toolsChanged();
            return;
        }
        rejectRequest();
        if (m_toolStage == "read") {
            const QString tool = m_toolMutation["tool_id"].toString();
            m_verifiedTools.remove(tool);
            if (tool == m_toolOpen)
                m_toolOpen.clear();
        }
        QTimer::singleShot(0, this, &Backend::advanceToolRegistry);
        emit toolsChanged();
        return;
    }
    const auto result = envelope["result"].toObject().toVariantMap();
    if (m_toolStage == "probe") {
        const auto operations = result["operations"].toMap();
        m_toolReady = result["schema_version"].toInt() == 1 && result["api"] == "hotbox.tool-menu" &&
                      operations.contains("tool_menu_register") && operations.contains("tool_menu_update") &&
                      operations.contains("tool_menu_read");
        if (!m_toolReady) {
            m_toolProblem =
                tr("The core helper does not support tool registration. Update both Hotbox binaries.");
            emit toolsChanged();
            return;
        }
    } else if (m_toolStage == "resource") {
        requestToolCore("mutation", m_toolMutation);
        return;
    } else {
        const QString expected = m_toolStage == "mutation"
                                     ? m_toolMutation["request"].toMap()["tool_id"].toString()
                                     : m_toolMutation["tool_id"].toString();
        if (result["tool_id"].toString() != expected || !projectToolEntry(result)) {
            m_toolProblem = tr("Tool registry returned an invalid development-context mapping.");
            m_verifiedTools.remove(expected);
            rejectRequest();
        } else if (m_toolStage == "mutation") {
            bool saved = m_db.transaction();
            if (saved) {
                saved = execute(
                    "UPDATE tool_registry_outbox SET state='accepted',response=?,error='' WHERE request_id=?",
                    {QString::fromUtf8(json(result)), m_toolRequest});
                QSqlQuery context(m_db);
                context.prepare("SELECT b.chat FROM tool_context_bindings b WHERE b.tool_id=? AND NOT "
                                "EXISTS(SELECT 1 FROM events WHERE kind='tool.registered' AND source=?)");
                context.addBindValue(expected);
                context.addBindValue(m_toolRequest);
                if (saved && context.exec() && context.next())
                    saved = m_ledger->record(context.value(0).toInt(), "tool.registered",
                                             tr("Tool metadata saved: %1").arg(result["name"].toString()),
                                             QString::fromUtf8(json(result)), m_toolRequest) > 0;
                if (!saved || !m_db.commit()) {
                    m_db.rollback();
                    saved = false;
                }
            }
            if (!saved) {
                m_toolReady = false;
                m_toolProblem =
                    tr("Could not save the registry receipt. The request is kept for safe replay.");
            } else
                emit activityChanged();
        }
        if (expected == m_toolOpen && m_toolStage == "read") {
            auto bound = boundTool(expected);
            m_toolOpen.clear();
            if (bound["verified"].toBool() && m_current == m_toolOpenScope)
                openChat(bound["chat"].toInt());
        }
    }
    emit toolsChanged();
    QTimer::singleShot(0, this, &Backend::advanceToolRegistry);
}

QVariantMap Backend::registerToolDraft(const QVariantMap &a) {
    if (!m_toolReady)
        return failure(tr("The tool registry is unavailable. Check setup first."));
    const auto request = a.value("request_id").toString();
    const int owner = a.value("category", m_current).toInt();
    if (!identifier(request) || !labels(a) || field(owner, "name").isEmpty())
        return failure(tr("Supply a request ID, name, purpose, category and a project in this scope."));
    const QString name = SessionLedger::redact(a["name"].toString().trimmed()),
                  purpose = SessionLedger::redact(a["purpose"].toString()),
                  group = SessionLedger::redact(a["menu_category"].toString().trimmed());
    QSqlQuery replay(m_db);
    replay.prepare("SELECT document,tool_id FROM tool_registry_outbox WHERE request_id=?");
    replay.addBindValue(request);
    if (replay.exec() && replay.next()) {
        if (boundTool(replay.value(1).toString()).isEmpty())
            return failure(tr("Tool is outside this scope."));
        const auto saved = QJsonDocument::fromJson(replay.value(0).toByteArray()).object();
        const auto payload = saved["request"].toObject();
        if (saved["command"] != "tool_menu_register" || payload["name"] != name ||
            payload["purpose"] != purpose || payload["menu_category"] != group ||
            payload["resource"].toString() != "workspace-" + QString::number(owner))
            return failure(tr("Request ID already used for different tool input."));
        return {{"ok", true}, {"tool_id", replay.value(1)}};
    }
    if (!m_ledger->contains(m_current, owner))
        return failure(tr("Project is outside this scope."));
    if (!m_db.transaction())
        return failure(tr("Tool context is busy."));
    const QString tool = fresh("tol_");
    QString installation = setting("tool_installation", "");
    if (installation.isEmpty()) {
        installation = QUuid::createUuid().toString(QUuid::Id128);
        if (!execute("INSERT INTO settings(name,value) VALUES('tool_installation',?)", {installation})) {
            m_db.rollback();
            return failure(tr("Could not save installation identity."));
        }
    }
    QSqlQuery chat(m_db);
    chat.prepare("INSERT INTO categories(parent,name,folder,brief) VALUES(?,?,?,?)");
    chat.addBindValue(owner);
    chat.addBindValue(name);
    chat.addBindValue(field(owner, "folder"));
    chat.addBindValue(purpose + "\n\nParent workspace #" + QString::number(owner) + "\n" +
                      field(owner, "brief").left(1800));
    if (!chat.exec()) {
        m_db.rollback();
        return failure(tr("Could not create the tool development context."));
    }
    const int context = chat.lastInsertId().toInt();
    const QString ref = "qt_" + installation + "_" + QString::number(context);
    QVariantMap payload{{"schema_version", 1},
                        {"request_id", request},
                        {"actor", "local-user"},
                        {"tool_id", tool},
                        {"resource", "workspace-" + QString::number(owner)},
                        {"name", name},
                        {"purpose", purpose},
                        {"menu_category", group},
                        {"originating_chat", ref},
                        {"initial", QVariant()}};
    QVariantMap document{{"command", "tool_menu_register"}, {"request", payload}};
    const bool saved =
        execute("INSERT INTO tool_context_bindings(tool_id,owner_category,chat,chat_ref,metadata) "
                "VALUES(?,?,?,?,?)",
                {tool, owner, context, ref, QString::fromUtf8(json(payload))}) &&
        execute("INSERT INTO tool_registry_outbox(request_id,tool_id,document) VALUES(?,?,?)",
                {request, tool, QString::fromUtf8(json(document))});
    if (!saved || !m_db.commit()) {
        m_db.rollback();
        return failure(tr("Could not save the tool registration request."));
    }
    recordEvent(context, "tool.draft", name, purpose, tool);
    m_toolOpen = tool;
    m_toolOpenScope = m_current;
    m_toolScanning = false;
    emit changed();
    advanceToolRegistry();
    return {{"ok", true}, {"tool_id", tool}, {"chat", context}, {"state", "pending"}};
}

QVariantMap Backend::updateToolDraft(const QVariantMap &a) {
    const auto tool = a.value("tool_id").toString(), request = a.value("request_id").toString();
    const auto bound = boundTool(tool);
    if (!m_toolReady || bound.isEmpty() || !identifier(request) || !labels(a))
        return failure(tr("Reload this tool and use its current metadata revision."));
    QVariantMap payload{{"schema_version", 1},
                        {"request_id", request},
                        {"actor", "local-user"},
                        {"tool_id", tool},
                        {"expected_revision", a["expected_revision"]},
                        {"name", SessionLedger::redact(a["name"].toString().trimmed())},
                        {"purpose", SessionLedger::redact(a["purpose"].toString())},
                        {"menu_category", SessionLedger::redact(a["menu_category"].toString().trimmed())}};
    QVariantMap document{{"command", "tool_menu_update"}, {"request", payload}};
    QSqlQuery replay(m_db);
    replay.prepare("SELECT document FROM tool_registry_outbox WHERE request_id=?");
    replay.addBindValue(request);
    if (replay.exec() && replay.next()) {
        if (QJsonDocument::fromJson(replay.value(0).toByteArray()).object() !=
            QJsonObject::fromVariantMap(document))
            return failure(tr("Request ID already used for different tool input."));
        return {{"ok", true}, {"tool_id", tool}};
    }
    if (!bound["verified"].toBool() ||
        a.value("expected_revision").toLongLong() != bound["revision"].toLongLong())
        return failure(tr("Reload this tool and use its current metadata revision."));
    QSqlQuery pending(m_db);
    pending.prepare("SELECT 1 FROM tool_registry_outbox WHERE tool_id=? AND state='pending'");
    pending.addBindValue(tool);
    if (pending.exec() && pending.next())
        return failure(tr("This tool already has a pending update."));
    if (!execute("INSERT INTO tool_registry_outbox(request_id,tool_id,document) VALUES(?,?,?)",
                 {request, tool, QString::fromUtf8(json(document))}))
        return failure(tr("The update could not be saved."));
    m_toolScanning = false;
    advanceToolRegistry();
    emit toolsChanged();
    return {{"ok", true}, {"tool_id", tool}};
}

void Backend::openToolContext(const QString &tool) {
    const auto entry = boundTool(tool);
    if (entry.isEmpty()) {
        setStatus(tr("Tool is outside this scope or its context is archived."));
        return;
    }
    m_toolOpen = tool;
    m_toolOpenScope = m_current;
    touchToolWheel(tool);
    refreshToolRegistry();
    if (toolRegistryBusy() && m_toolScanning && !m_toolScan.contains(tool))
        m_toolScan.prepend(tool);
}

bool Backend::markToolWheel(const QString &tool, const QString &group, bool enabled) {
    if (boundTool(tool).isEmpty() || !QStringList{"favorites", "pinned"}.contains(group))
        return false;
    const auto column = group == "favorites" ? QString("favorite") : QString("pinned");
    const bool ok =
        execute("INSERT INTO tool_wheel_items(tool_id," + column +
                    ") VALUES(?,?) ON CONFLICT(tool_id) DO UPDATE SET " + column + "=excluded." + column,
                {tool, enabled ? 1 : 0});
    emit toolsChanged();
    return ok;
}
void Backend::touchToolWheel(const QString &tool) {
    if (boundTool(tool).isEmpty())
        return;
    execute("INSERT INTO tool_wheel_items(tool_id,used) VALUES(?,?) ON CONFLICT(tool_id) DO UPDATE SET "
            "used=excluded.used",
            {tool, QDateTime::currentMSecsSinceEpoch()});
}
