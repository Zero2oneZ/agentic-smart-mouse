#include "backend.h"
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QSqlQuery>
#include <QStandardPaths>

QVariantMap Backend::runtimeReport() const {
    QSqlQuery check("PRAGMA quick_check", m_db);
    const bool databaseOk = check.next() && check.value(0).toString() == "ok";
    QVariantList storage;
    auto add = [&](const QString &name, const QString &path, const QString &owner, const QString &purpose) {
        storage.append(QVariantMap{{"name", name},
                                   {"path", path},
                                   {"owner", owner},
                                   {"purpose", purpose},
                                   {"exists", QFileInfo::exists(path)}});
    };
    add(tr("Chat and private registry"), m_db.databaseName(), "private-state",
        tr("Chats, event evidence, origins, bindings and agent lifecycle metadata"));
    add(tr("Task database"), m_directory + "/orchestration.sqlite3", "private-state",
        tr("Rust workflow checkpoints, leases and local task results metadata"));
    add(tr("Build documents"),m_directory+"/build-ledger","private-evidence",tr("Immutable document objects linked to versioned roadmap history"));
    add(tr("Task results"), m_directory + "/task-results", "user-output",
        tr("Checked notes results; never source architecture"));
    add(tr("Agent runs"), m_directory + "/agent-runs", "private-runtime",
        tr("One run directory per ID: prompts, working files, output and result logs"));
    add(tr("Managed worktrees"), m_directory + "/worktrees", "managed-source",
        tr("Explicit isolated Git branches; conversation data stays outside pushed files"));
    const auto registry = architecture();
    if (registry["initialized"].toBool() && registry.contains("toolRoot")) {
        add(tr("Architecture source"), registry["toolRoot"].toString(), "tool-source",
            tr("Reusable definitions, templates and project source"));
        add(tr("User belongings"), registry["runtimeRoot"].toString(), "user-data",
            tr("Shared libraries and application data outside the source repository"));
    }
    bool withinGit = false;
    QDir cursor(m_directory);
    do {
        if (QFileInfo::exists(cursor.filePath(".git/HEAD")) || QFileInfo(cursor.filePath(".git")).isFile()) {
            withinGit = true;
            break;
        }
    } while (cursor.cdUp());
    return {
        {"ok", databaseOk && m_tasks->available() && !withinGit},
        {"databaseOk", databaseOk},
        {"coreAvailable", m_tasks->available()},
        {"runtimeInsideGit", withinGit},
        {"architectureInitialized", registry["initialized"]},
        {"storage", storage},
        {"build", QVariantMap{{"cargoAvailable", !QStandardPaths::findExecutable("cargo").isEmpty()},
                              {"desktopBinary", QCoreApplication::applicationFilePath()},
                              {"coreBinary", m_tasks->helperPath()}}},
        {"providers", QVariantMap{{"codexAvailable", !QStandardPaths::findExecutable("codex").isEmpty()},
                                  {"claudeAvailable", !QStandardPaths::findExecutable("claude").isEmpty()}}},
        {"agentRuns", m_agents->jobs().size()},
        {"terminalMode",
         tr("Agent terminals follow recorded output; they are not interactive PTY attachments")}};
}
