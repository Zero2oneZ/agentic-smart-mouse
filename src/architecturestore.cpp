#include "architecturestore.h"
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLockFile>
#include <QProcess>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSqlError>
#include <QSqlQuery>

namespace {
QVariantMap error(const QString &text) { return {{"ok", false}, {"error", text}}; }
QJsonObject definition() {
    QFile file(":/architecture.json");
    if (!file.open(QIODevice::ReadOnly))
        return {};
    return QJsonDocument::fromJson(file.readAll()).object();
}
bool sql(QSqlDatabase db, const QString &statement, const QVariantList &args = {}) {
    QSqlQuery q(db);
    q.prepare(statement);
    for (const auto &arg : args)
        q.addBindValue(arg);
    return q.exec();
}
bool within(const QString &parent, const QString &child) {
    return child == parent || child.startsWith(parent + '/');
}
QString cleanRoot(QString path) {
    if (path.startsWith("~/"))
        path = QDir::homePath() + path.mid(1);
    if (!QDir::isAbsolutePath(path) || path.split('/').contains(".."))
        return {};
    path = QDir::cleanPath(path);
    if (path == "/" || path == QDir::homePath())
        return {};
    return path;
}
// Never follow a pre-existing symlink or replace a file with a directory.
bool safePath(const QString &path) {
    if (path.isEmpty() || !QDir::isAbsolutePath(path))
        return false;
    QString part;
    for (const auto &component : path.split('/', Qt::SkipEmptyParts)) {
        part += '/' + component;
        QFileInfo info(part);
        if (info.isSymbolicLink() || (info.exists() && !info.isDir()))
            return false;
    }
    return true;
}
bool makeDirectory(const QString &path) {
    if (!safePath(path))
        return false;
    if (QFileInfo::exists(path))
        return true;
    const QString parent = QFileInfo(path).absolutePath();
    if (!makeDirectory(parent) || !QDir().mkdir(path))
        return false;
    return QFile::setPermissions(path, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
}
bool writeNew(const QString &path, const QByteArray &content) {
    if (!safePath(QFileInfo(path).absolutePath()) || QFileInfo(path).isSymbolicLink())
        return false;
    QFile file(path);
    if (file.exists()) {
        return file.open(QIODevice::ReadOnly) && file.readAll() == content;
    }
    if (!file.open(QIODevice::WriteOnly | QIODevice::NewOnly))
        return false;
    file.setPermissions(QFile::ReadOwner | QFile::WriteOwner);
    return file.write(content) == content.size() && file.flush();
}
bool git(const QString &cwd, const QStringList &args, QByteArray *output = nullptr) {
    QProcess process;
    auto env = QProcessEnvironment::systemEnvironment();
    for (const auto &key : env.keys())
        if (key.startsWith("GIT_"))
            env.remove(key);
    env.insert("GIT_TERMINAL_PROMPT", "0");
    process.setProcessEnvironment(env);
    process.start("git", QStringList{"-c", "core.hooksPath=/dev/null", "-c", "core.fsmonitor=false", "-c",
                                     "init.templateDir=", "-c", "commit.gpgSign=false", "-C", cwd} +
                             args);
    if (!process.waitForFinished(8000)) {
        process.kill();
        process.waitForFinished(1000);
        return false;
    }
    if (output)
        *output = process.readAllStandardOutput().trimmed();
    return process.exitStatus() == QProcess::NormalExit && process.exitCode() == 0;
}
bool insideGit(QString path) {
    while (!QFileInfo::exists(path))
        path = QFileInfo(path).absolutePath();
    return git(path, {"rev-parse", "--show-toplevel"});
}
QString key(const QString &text) {
    const QString normalized = text.normalized(QString::NormalizationForm_C).trimmed().toCaseFolded();
    QString slug = normalized;
    slug.replace(QRegularExpression("[^a-z0-9]+"), "-");
    slug = slug.left(48);
    if (slug.isEmpty() || slug == "-")
        slug = "item";
    return slug + '-' +
           QString::fromLatin1(
               QCryptographicHash::hash(normalized.toUtf8(), QCryptographicHash::Sha256).toHex().left(12));
}
bool validLabel(const QString &text) {
    return text.size() <= 120 && text != "." && text != ".." &&
           !text.contains(QRegularExpression("[/\\\\\\x00-\\x1f\\x7f]"));
}
bool validCapability(const QJsonObject &item) {
    const QString id = item["id"].toString(), path = item["path"].toString();
    return QRegularExpression("^[a-z][a-z0-9-]{0,39}$").match(id).hasMatch() &&
           QRegularExpression("^(media|libraries)/[a-z0-9][a-z0-9/-]{0,159}$").match(path).hasMatch() &&
           !path.contains("//") && !path.endsWith('/') && item["description"].toString().size() <= 240;
}
QJsonObject registeredDefinition(const QString &tool) {
    if (tool.isEmpty())
        return definition();
    QFile stored(tool + "/templates/media-v1.json");
    if (!safePath(tool + "/templates") || QFileInfo(stored).isSymbolicLink() || stored.size() > 32768 ||
        !stored.open(QIODevice::ReadOnly))
        return {};
    const auto schema = QJsonDocument::fromJson(stored.readAll()).object(), base = definition();
    if (schema["id"] != base["id"] || schema["version"] != base["version"] ||
        schema["layouts"] != base["layouts"] || schema["capabilities"].toArray().size() > 32)
        return {};
    const auto caps = schema["capabilities"].toArray();
    for (const auto &item : base["capabilities"].toArray())
        if (!caps.contains(item))
            return {};
    QStringList ids, paths;
    for (const auto &value : caps) {
        const auto item = value.toObject();
        if (!validCapability(item) || ids.contains(item["id"].toString()) ||
            paths.contains(item["path"].toString()))
            return {};
        ids << item["id"].toString();
        paths << item["path"].toString();
    }
    return schema;
}
} // namespace

ArchitectureStore::ArchitectureStore(const QSqlDatabase &db, SessionLedger &ledger, const QString &directory)
    : m_db(db), m_ledger(ledger), m_directory(directory) {
    sql(m_db, "CREATE TABLE IF NOT EXISTS architecture_projects(category INTEGER PRIMARY KEY REFERENCES "
              "categories(id), project TEXT NOT NULL UNIQUE)");
    sql(m_db, "CREATE TABLE IF NOT EXISTS architecture_bindings(category INTEGER NOT NULL REFERENCES "
              "categories(id), capability TEXT NOT NULL, path TEXT NOT NULL, PRIMARY "
              "KEY(category,capability,path))");
    sql(m_db,
        "CREATE TABLE IF NOT EXISTS architecture_genres(category INTEGER NOT NULL REFERENCES "
        "categories(id), genre TEXT NOT NULL, entity TEXT NOT NULL, PRIMARY KEY(category,genre,entity))");
}
QString ArchitectureStore::setting(const QString &name) const {
    QSqlQuery q(m_db);
    q.prepare("SELECT value FROM settings WHERE name=?");
    q.addBindValue("architecture_" + name);
    q.exec();
    return q.next() ? q.value(0).toString() : QString();
}
QVariantMap ArchitectureStore::catalog(int scope) const {
    QVariantList attachments, genres, projects;
    QSqlQuery q(
        "SELECT category,capability,path FROM architecture_bindings ORDER BY category,capability,path", m_db);
    while (q.next())
        if (m_ledger.contains(scope, q.value(0).toInt()))
            attachments.append(QVariantMap{{"category", q.value(0)},
                                           {"capability", q.value(1)},
                                           {"path", q.value(2)},
                                           {"owner", "user"}});
    QSqlQuery g("SELECT category,genre,entity FROM architecture_genres ORDER BY category,genre,entity", m_db);
    while (g.next())
        if (m_ledger.contains(scope, g.value(0).toInt()))
            genres.append(
                QVariantMap{{"category", g.value(0)}, {"genre", g.value(1)}, {"entity", g.value(2)}});
    QSqlQuery p("SELECT category,project FROM architecture_projects ORDER BY category", m_db);
    while (p.next())
        if (m_ledger.contains(scope, p.value(0).toInt()))
            projects.append(QVariantMap{{"category", p.value(0)}, {"id", p.value(1)}});
    QVariantMap result{{"initialized", !setting("tool_root").isEmpty()},
                       {"template", registeredDefinition(setting("tool_root")).toVariantMap()},
                       {"attachments", attachments},
                       {"genres", genres},
                       {"projects", projects},
                       {"containerStatus", "not_configured"}};
    if (scope == 1) {
        result["toolRoot"] = setting("tool_root");
        result["runtimeRoot"] = setting("runtime_root");
        result["defaultToolRoot"] = m_directory + "/development";
        result["defaultRuntimeRoot"] = m_directory + "/belongings";
    }
    return result;
}
QVariantMap ArchitectureStore::initialize(int scope, const QString &requestedTool,
                                          const QString &requestedRuntime) {
    if (scope != 1)
        return error("Initialize shared architecture from master chat");
    QLockFile lock(m_directory + "/architecture.lock");
    if (!lock.tryLock(0))
        return error("Another architecture operation is running");
    QString tool =
        cleanRoot(requestedTool.isEmpty()
                      ? (setting("tool_root").isEmpty() ? m_directory + "/development" : setting("tool_root"))
                      : requestedTool);
    QString runtime = cleanRoot(
        requestedRuntime.isEmpty()
            ? (setting("runtime_root").isEmpty() ? m_directory + "/belongings" : setting("runtime_root"))
            : requestedRuntime);
    if (tool.isEmpty() || runtime.isEmpty() || within(tool, runtime) || within(runtime, tool) ||
        within(tool, m_directory) || within(runtime, m_directory) || !safePath(tool) || !safePath(runtime))
        return error("Choose separate absolute tool and belongings roots outside the private Hotbox "
                     "database; symlinks are not allowed");
    if (!setting("tool_root").isEmpty()) {
        if (tool != setting("tool_root") || runtime != setting("runtime_root"))
            return error("Architecture is already initialized; roots cannot be changed implicitly");
        return {{"ok", true},
                {"reused", true},
                {"toolRoot", tool},
                {"runtimeRoot", runtime},
                {"containerStatus", "not_configured"}};
    }
    if (insideGit(runtime) || insideGit(tool))
        return error("Use fresh roots outside existing Git repositories");
    for (const auto &path : {tool, runtime})
        if (QFileInfo::exists(path) && !QDir(path).isEmpty())
            return error(
                "Initial architecture roots must be empty; existing belongings are not adopted or moved");
    if (!makeDirectory(tool) || !makeDirectory(runtime) || !makeDirectory(tool + "/templates") ||
        !makeDirectory(tool + "/projects"))
        return error("Cannot create architecture roots");
    const QByteArray schema = QJsonDocument(definition()).toJson();
    const QByteArray marker = "{\"format\":\"hotbox-development-v1\",\"template\":\"media-v1\"}\n";
    const QByteArray ignore =
        "build/\n.cache/\n.env\n.env.*\n*.db\n*.db-*\n*.sqlite3\n*.sqlite3-*\n*.log\n*.jsonl\n";
    if (!writeNew(tool + "/architecture.json", marker) ||
        !writeNew(tool + "/templates/media-v1.json", schema) || !writeNew(tool + "/.gitignore", ignore) ||
        !writeNew(tool + "/README.md",
                  "# Hotbox development monorepo\n\nTool-owned templates and project source. User belongings "
                  "live in a separate root.\nRuntime paths and personal media metadata are kept in the "
                  "private Hotbox registry.\n") ||
        !writeNew(runtime + "/.hotbox-belongings", "hotbox-belongings-v1\n"))
        return error("Could not write architecture markers; existing files were not replaced");
    if (!git(tool, {"init", "--initial-branch=main"}) ||
        !git(tool,
             {"add", "--", "architecture.json", "templates/media-v1.json", ".gitignore", "README.md"}) ||
        !git(tool, {"-c", "user.name=Hotbox", "-c", "user.email=hotbox@localhost", "commit", "-m",
                    "Initialize tool-owned architecture and media template"}))
        return error(
            "Could not initialize the development Git repository; created directories remain in place");
    if (!m_db.transaction())
        return error("Could not save architecture roots");
    bool saved =
        sql(m_db, "INSERT INTO settings(name,value) VALUES('architecture_tool_root',?)", {tool}) &&
        sql(m_db, "INSERT INTO settings(name,value) VALUES('architecture_runtime_root',?)", {runtime}) &&
        sql(m_db, "UPDATE categories SET folder=? WHERE id=1 AND folder=''", {tool});
    if (!saved || !m_db.commit()) {
        m_db.rollback();
        return error("Could not save architecture roots; created directories remain in place");
    }
    m_ledger.record(
        1, "architecture.initialized", "Initialized development and belongings roots",
        QString::fromUtf8(QJsonDocument(QJsonObject{{"toolRoot", tool}, {"runtimeRoot", runtime}}).toJson()));
    return {
        {"ok", true}, {"toolRoot", tool}, {"runtimeRoot", runtime}, {"containerStatus", "not_configured"}};
}
QVariantMap ArchitectureStore::defineCapability(int scope, const QString &id, const QString &relativePath,
                                                const QString &description) {
    if (scope != 1)
        return error("Register shared capabilities from master chat");
    QLockFile lock(m_directory + "/architecture.lock");
    if (!lock.tryLock(0))
        return error("Another architecture operation is running");
    const QString tool = setting("tool_root");
    if (tool.isEmpty())
        return error("Initialize architecture first");
    auto schema = registeredDefinition(tool);
    if (schema.isEmpty())
        return error("Architecture template is invalid or unsupported");
    QJsonObject item{{"id", id}, {"path", relativePath}, {"description", description}};
    if (!validCapability(item) || SessionLedger::redact(description) != description)
        return error("Use a lowercase capability ID, a relative media/ or libraries/ path, and a description "
                     "up to 240 characters");
    auto caps = schema["capabilities"].toArray();
    for (const auto &value : caps) {
        auto existing = value.toObject();
        if (existing == item)
            return {{"ok", true}, {"reused", true}, {"capability", item.toVariantMap()}};
        if (existing["id"].toString() == id || within(existing["path"].toString(), relativePath) ||
            within(relativePath, existing["path"].toString()))
            return error("Capability IDs and library paths are stable; choose a new non-overlapping library");
    }
    if (caps.size() >= 32)
        return error("This architecture version supports at most 32 capabilities");
    caps.append(item);
    schema["capabilities"] = caps;
    QSaveFile file(tool + "/templates/media-v1.json");
    if (!file.open(QIODevice::WriteOnly))
        return error("Cannot save the architecture definition");
    const auto bytes = QJsonDocument(schema).toJson();
    if (bytes.size() > 32768)
        return error("Architecture definition exceeds this version's 32 KiB limit");
    if (file.write(bytes) != bytes.size() || !file.commit())
        return error("Cannot save the architecture definition");
    m_ledger.record(1, "architecture.defined", "Registered storage capability " + id,
                    QString::fromUtf8(QJsonDocument(item).toJson()));
    return {{"ok", true}, {"capability", item.toVariantMap()}};
}
QVariantMap ArchitectureStore::plan(int scope, int category, const QVariantMap &a) const {
    if (!m_ledger.contains(scope, category))
        return error("Workspace is outside this conversation's scope");
    QString tool = setting("tool_root"), runtime = setting("runtime_root");
    if (tool.isEmpty())
        return error("Initialize architecture from master chat first");
    if (category == 1)
        return error("Select a focused project workspace for a development environment");
    QStringList capabilities = a.value("capabilities").toStringList();
    capabilities.removeDuplicates();
    if (capabilities.isEmpty() || capabilities.size() > 32)
        return error("Choose one or more registered capabilities");
    const QString artist = a.value("artist").toString().trimmed(),
                  album = a.value("album").toString().trimmed(),
                  genre = a.value("genre").toString().trimmed();
    if (!validLabel(artist) || !validLabel(album) || !validLabel(genre) ||
        (!album.isEmpty() && artist.isEmpty()))
        return error("Use labels up to 120 characters without path separators; albums require an artist");
    if ((!artist.isEmpty() || !genre.isEmpty()) && !capabilities.contains("music") &&
        !capabilities.contains("album-covers"))
        return error("Artist, album and genre labels require music or album-cover storage");
    if (!genre.isEmpty() && !capabilities.contains("music"))
        return error("Genre associations require music storage");
    QString project = "workspace-" + QString::number(category), source = tool + "/projects/" + project;
    QSqlQuery q(m_db);
    q.prepare("SELECT folder FROM categories WHERE id=? AND archived=0");
    q.addBindValue(category);
    q.exec();
    if (!q.next())
        return error("Choose an active workspace");
    if (!q.value(0).toString().isEmpty() && QDir::cleanPath(q.value(0).toString()) != source)
        return error(
            "This workspace already links another folder; create a new workspace to manage a project here");
    QJsonObject schema = registeredDefinition(tool);
    if (schema.isEmpty())
        return error("The installed media-v1 template differs from the supported architecture; resolve its "
                     "version before provisioning");
    QVariantList directories;
    QStringList unique;
    auto add = [&](const QString &path, const QString &owner, const QString &capability) {
        if (!unique.contains(path)) {
            unique << path;
            directories.append(QVariantMap{{"path", path},
                                           {"owner", owner},
                                           {"capability", capability},
                                           {"existing", QFileInfo(path).isDir()}});
        }
    };
    add(source + "/src", "tool", "source");
    for (const auto &dir : {"state", "config", "cache"})
        add(runtime + "/apps/" + project + '/' + dir, "user", "application-" + QString(dir));
    QMap<QString, QString> paths;
    for (const auto &value : schema["capabilities"].toArray()) {
        auto item = value.toObject();
        paths[item["id"].toString()] = item["path"].toString();
    }
    for (const auto &capability : capabilities) {
        if (!paths.contains(capability))
            return error("Unknown capability: " + capability);
        add(runtime + '/' + paths[capability], "user", capability);
    }
    const auto layouts = schema["layouts"].toObject();
    auto entityPath = [&](const QString &name) {
        QString relative = layouts[name].toString();
        relative.replace("{artist}", key(artist))
            .replace("{album}", key(album))
            .replace("{genre}", key(genre));
        return runtime + '/' + relative;
    };
    if (capabilities.contains("music")) {
        if (!artist.isEmpty()) {
            add(entityPath("artist"), "user", "music");
            if (!album.isEmpty())
                add(entityPath("songs"), "user", "music");
        }
        if (!genre.isEmpty())
            add(entityPath("genre"), "user", "music");
    }
    if (capabilities.contains("album-covers") && !artist.isEmpty())
        add(entityPath(album.isEmpty() ? "band" : "cover"), "user", "album-covers");
    if (!safePath(tool) || !safePath(runtime))
        return error("An architecture root was replaced or redirected");
    for (const auto &value : directories)
        if (!safePath(value.toMap()["path"].toString()))
            return error("A requested directory is a file or passes through a symlink");
    QVariantMap result{{"ok", true},
                       {"template", "media-v1"},
                       {"project", project},
                       {"source", source},
                       {"runtime", runtime + "/apps/" + project},
                       {"directories", directories},
                       {"capabilities", capabilities},
                       {"containerStatus", "not_configured"}};
    if (!genre.isEmpty()) {
        QString entity = artist.isEmpty() ? QString() : entityPath(album.isEmpty() ? "artist" : "album");
        result["genre"] = genre;
        result["genrePath"] = entityPath("genre");
        result["entity"] = entity;
    }
    return result;
}
QVariantMap ArchitectureStore::provision(int scope, int category, const QVariantMap &arguments) {
    QLockFile lock(m_directory + "/architecture.lock");
    if (!lock.tryLock(0))
        return error("Another architecture operation is running");
    auto result = plan(scope, category, arguments);
    if (!result.value("ok").toBool())
        return result;
    QString tool = setting("tool_root"), runtime = setting("runtime_root");
    if (insideGit(runtime))
        return error("Belongings root is now inside a Git repository; provisioning paused");
    if (!git(tool, {"rev-parse", "--show-toplevel"}))
        return error("The development Git repository is unavailable");
    for (const auto &value : result["directories"].toList())
        if (!makeDirectory(value.toMap()["path"].toString()))
            return error("Could not create a requested directory; existing data was left in place");
    QString source = result["source"].toString(), project = result["project"].toString();
    QJsonObject manifest{{"format", "hotbox-project-v1"},
                         {"id", project},
                         {"template", "media-v1"},
                         {"runtimeBinding", "Resolve workspace bindings through the private Hotbox registry"},
                         {"containerStatus", "not_configured"}};
    // No artist names, genre labels, runtime paths or private content enter the Git tree.
    if (!writeNew(source + "/project.json", QJsonDocument(manifest).toJson()) ||
        !writeNew(source + "/src/.gitkeep", QByteArray()))
        return error("Project scaffold conflicts with an existing file; nothing was overwritten");
    if (!m_db.transaction())
        return error("Could not save project bindings; created directories remain available for retry");
    bool saved = sql(m_db, "INSERT OR IGNORE INTO architecture_projects(category,project) VALUES(?,?)",
                     {category, project});
    for (const auto &value : result["directories"].toList()) {
        auto dir = value.toMap();
        if (dir["owner"] == "user")
            saved = sql(m_db,
                        "INSERT OR IGNORE INTO architecture_bindings(category,capability,path) VALUES(?,?,?)",
                        {category, dir["capability"], dir["path"]}) &&
                    saved;
    }
    if (!result.value("entity").toString().isEmpty())
        saved = sql(m_db, "INSERT OR IGNORE INTO architecture_genres(category,genre,entity) VALUES(?,?,?)",
                    {category, result["genre"], result["entity"]}) &&
                saved;
    saved = sql(m_db, "UPDATE categories SET folder=? WHERE id=?", {source, category}) && saved;
    if (!saved || !m_db.commit()) {
        m_db.rollback();
        return error("Could not save project bindings; created directories remain available for retry");
    }
    m_ledger.record(category, "architecture.provisioned", "Attached reusable storage and development project",
                    QString::fromUtf8(QJsonDocument(QJsonObject::fromVariantMap(result)).toJson()));
    return result;
}
