#include "buildledger.h"
#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QSet>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>
#include <cmath>

namespace {
QString uid() { return QUuid::createUuid().toString(QUuid::Id128); }
QString stamp() { return QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs); }
QString encoded(const QVariantMap &value) {
    return QString::fromUtf8(
        QJsonDocument(QJsonObject::fromVariantMap(value)).toJson(QJsonDocument::Compact));
}
QVariantMap decoded(const QVariant &value) {
    return QJsonDocument::fromJson(value.toByteArray()).object().toVariantMap();
}
QString digest(const QString &text) {
    return QString::fromLatin1(QCryptographicHash::hash(text.toUtf8(), QCryptographicHash::Sha256).toHex());
}
QVariantMap error(const QString &text) { return {{"ok", false}, {"schema_version", 1}, {"error", text}}; }
} // namespace
bool BuildLedger::sql(const QString &statement, const QVariantList &values) const {
    QSqlQuery q(m_db);
    q.prepare(statement);
    for (auto value : values)
        q.addBindValue(value);
    return q.exec();
}
BuildLedger::BuildLedger(const QSqlDatabase &db, SessionLedger &events, const QString &directory)
    : m_db(db), m_events(events), m_directory(directory + "/build-ledger") {
    QSqlQuery version(m_db);
    if (version.exec("SELECT MAX(version) FROM build_schema") && version.next() &&
        version.value(0).toInt() > 1) {
        m_problem = "Build ledger schema is newer than this application";
        return;
    }
    QFile schema(":/build-ledger-v1.sql");
    if (!schema.open(QIODevice::ReadOnly) || !sql("BEGIN IMMEDIATE")) {
        m_problem = "Build ledger schema could not be opened";
        return;
    }
    for (const auto &statement :
         QString::fromUtf8(schema.readAll()).split("\n-- statement\n", Qt::SkipEmptyParts)) {
        if (!sql(statement)) {
            sql("ROLLBACK");
            m_problem = "Build ledger schema could not be initialized";
            return;
        }
    }
    if (!sql("COMMIT"))
        m_problem = "Build ledger schema could not be committed";
}
QVariantMap BuildLedger::read(int scope, const QString &id) const {
    QSqlQuery q(m_db);
    q.prepare("SELECT category,title,revision,base_road,base_revision,branch_ref,commit_ref,created FROM "
              "build_roads WHERE id=?");
    q.addBindValue(id);
    if (!q.exec() || !q.next() || !m_events.contains(scope, q.value(0).toInt()))
        return error("Roadmap is outside this workspace or does not exist");
    QVariantMap result{{"ok", true},
                       {"schema_version", 1},
                       {"id", id},
                       {"workspace", q.value(0)},
                       {"master_chat", 1},
                       {"title", q.value(1)},
                       {"revision", q.value(2)},
                       {"base_road", q.value(3)},
                       {"base_revision", q.value(4)},
                       {"branch_ref", q.value(5)},
                       {"commit_ref", q.value(6)},
                       {"created", q.value(7)}};
    auto rows = [&](const QString &query, const QStringList &keys) {
        QVariantList list;
        QSqlQuery r(m_db);
        r.prepare(query);
        r.addBindValue(id);
        r.exec();
        while (r.next()) {
            QVariantMap item;
            for (int i = 0; i < keys.size(); i++)
                item[keys[i]] = r.value(i);
            list.append(item);
        }
        return list;
    };
    result["nodes"] = rows("SELECT id,parent,kind,title,body,state,created FROM build_nodes WHERE road=? "
                           "ORDER BY rowid LIMIT 1000",
                           {"id", "parent", "kind", "title", "body", "state", "created"});
    result["documents"] = rows("SELECT "
                               "d.id,d.node,d.title,v.id,v.number,v.parent_version,v.digest,v.branch_ref,v."
                               "commit_ref,v.created FROM build_docs d JOIN build_doc_versions v ON "
                               "v.doc=d.id WHERE d.road=? ORDER BY v.rowid DESC LIMIT 1000",
                               {"document", "node", "title", "version", "number", "parent_version", "digest",
                                "branch_ref", "commit_ref", "created"});
    result["links"] =
        rows("SELECT id,node,kind,ref,created FROM build_links WHERE road=? ORDER BY rowid DESC LIMIT 1000",
             {"id", "node", "kind", "ref", "created"});
    result["checkpoints"] = rows("SELECT id,revision,digest,created FROM build_checkpoints WHERE road=? "
                                 "ORDER BY rowid DESC LIMIT 1000",
                                 {"id", "revision", "digest", "created"});
    result["changes"] = rows("SELECT revision,operation,created,event FROM build_changes WHERE road=? ORDER "
                             "BY revision DESC LIMIT 100",
                             {"revision", "operation", "created", "event"});
    QSqlQuery source(m_db);
    source.prepare("SELECT payload FROM build_changes WHERE road=? AND revision=1 AND operation='fork'");
    source.addBindValue(id);
    if (source.exec() && source.next())
        result["base_node"] = decoded(source.value(0)).value("node", QString(""));
    result["limits"] = QVariantMap{{"nodes", 1000},
                                   {"document_versions", 1000},
                                   {"links", 1000},
                                   {"checkpoints", 1000},
                                   {"change_page", 100}};
    return result;
}
QVariantMap BuildLedger::call(int scope, const QString &operation, const QVariantMap &a) {
    if (!m_problem.isEmpty())
        return error(m_problem);
    if (a.value("schema_version").toInt() != 1)
        return error("schema_version must be 1");
    const QMap<QString, QStringList> fields{
        {"list", {}},
        {"schema", {}},
        {"read", {"roadmap"}},
        {"document.read", {"roadmap", "version", "offset"}},
        {"checkpoint.read", {"roadmap", "checkpoint"}},
        {"history", {"roadmap", "before"}},
        {"create", {"request_id", "title", "branch_ref", "commit_ref"}},
        {"node.add", {"request_id", "roadmap", "expected_revision", "parent", "kind", "title", "body"}},
        {"node.update", {"request_id", "roadmap", "expected_revision", "node", "state", "body"}},
        {"document.save",
         {"request_id", "roadmap", "expected_revision", "node", "document", "parent_version", "title",
          "content", "branch_ref", "commit_ref"}},
        {"link", {"request_id", "roadmap", "expected_revision", "node", "kind", "ref"}},
        {"checkpoint", {"request_id", "roadmap", "expected_revision"}},
        {"fork",
         {"request_id", "roadmap", "expected_revision", "title", "branch_ref", "commit_ref", "checkpoint",
          "node"}}};
    if (!fields.contains(operation))
        return error("Unknown build operation");
    for (auto it = a.begin(); it != a.end(); ++it) {
        const bool numeric =
            QStringList{"schema_version", "expected_revision", "offset", "before"}.contains(it.key());
        if (numeric) {
            const int type = it.value().metaType().id();
            const auto number = it.value().toDouble();
            if (!QList<int>{QMetaType::Int, QMetaType::UInt, QMetaType::LongLong, QMetaType::ULongLong,
                            QMetaType::Double}
                     .contains(type) ||
                !std::isfinite(number) || number < 0 || number > 9007199254740991.0 ||
                std::floor(number) != number)
                return error("Expected a nonnegative integer: " + it.key());
        } else if (it.value().metaType().id() != QMetaType::QString)
            return error("Expected text: " + it.key());
        if (it.key() != "schema_version" && !fields[operation].contains(it.key()))
            return error("Unknown request field: " + it.key());
        if (it.value().metaType().id() == QMetaType::QString &&
            it.value().toString().size() > (it.key() == "content" ? 65536 : 4000))
            return error("Request field is too large: " + it.key());
    }
    if (operation == "schema") {
        QVariantMap operations;
        for (auto i = fields.begin(); i != fields.end(); ++i)
            operations[i.key()] = i.value();
        return {{"ok", true},
                {"schema_version", 1},
                {"api", "hotbox.build"},
                {"operations", operations},
                {"node_kinds", QStringList{"milestone", "checkpoint", "step", "substep", "issue", "test",
                                           "debug", "documentation"}},
                {"planning_states", QStringList{"planned", "in_progress", "blocked", "recorded", "archived"}},
                {"link_kinds", QStringList{"agent_run", "chat_event", "terminal_command"}},
                {"semantics", "idempotent_mutations_with_expected_revision"}};
    }
    if (operation == "list") {
        QSqlQuery q("SELECT id,category,title,revision,created FROM build_roads ORDER BY rowid DESC", m_db);
        QVariantList list;
        while (q.next() && list.size() < 100)
            if (m_events.contains(scope, q.value(1).toInt()))
                list.append(QVariantMap{{"id", q.value(0)},
                                        {"workspace", q.value(1)},
                                        {"title", q.value(2)},
                                        {"revision", q.value(3)},
                                        {"created", q.value(4)}});
        return {{"ok", true}, {"schema_version", 1}, {"roadmaps", list}};
    }
    auto road = a.value("roadmap").toString();
    QVariantMap existing;
    if (operation != "create") {
        existing = read(scope, road);
        if (!existing["ok"].toBool())
            return existing;
    }
    if (operation == "read")
        return existing;
    if (operation == "history") {
        QSqlQuery q(m_db);
        q.prepare("SELECT revision,operation,payload,created,event FROM build_changes WHERE road=? AND (?=0 "
                  "OR revision<?) ORDER BY revision DESC LIMIT 40");
        q.addBindValue(road);
        q.addBindValue(a.value("before", 0));
        q.addBindValue(a.value("before", 0));
        q.exec();
        QVariantList rows;
        while (q.next())
            rows.append(QVariantMap{{"revision", q.value(0)},
                                    {"operation", q.value(1)},
                                    {"request", decoded(q.value(2))},
                                    {"created", q.value(3)},
                                    {"event", q.value(4)}});
        return {{"ok", true}, {"schema_version", 1}, {"changes", rows}};
    }
    if (operation == "document.read") {
        QSqlQuery q(m_db);
        q.prepare("SELECT v.content,v.digest,v.created FROM build_doc_versions v JOIN build_docs d ON "
                  "d.id=v.doc WHERE v.id=? AND d.road=?");
        q.addBindValue(a.value("version"));
        q.addBindValue(road);
        if (!q.exec() || !q.next())
            return error("Document version is outside this roadmap");
        auto content = q.value(0).toString();
        const int offset = a.value("offset", 0).toInt();
        if (offset < 0 || offset > content.size())
            return error("Invalid document offset");
        return {{"ok", true},
                {"schema_version", 1},
                {"content", content.mid(offset, 6000)},
                {"digest", q.value(1)},
                {"created", q.value(2)},
                {"nextOffset", offset + 6000 < content.size() ? offset + 6000 : -1}};
    }
    if (operation == "checkpoint.read") {
        QSqlQuery q(m_db);
        q.prepare("SELECT snapshot,digest FROM build_checkpoints WHERE id=? AND road=?");
        q.addBindValue(a.value("checkpoint"));
        q.addBindValue(road);
        if (!q.exec() || !q.next())
            return error("Checkpoint is outside this roadmap");
        return {
            {"ok", true}, {"schema_version", 1}, {"snapshot", decoded(q.value(0))}, {"digest", q.value(1)}};
    }
    const auto key = a.value("request_id").toString();
    if (key.isEmpty() || key.size() > 128)
        return error("Mutations require a request_id up to 128 characters");
    const auto fingerprint = digest(operation + encoded(a));
    if (!sql("BEGIN IMMEDIATE"))
        return error("Build ledger is busy");
    auto fail = [&](const QString &text) {
        sql("ROLLBACK");
        return error(text);
    };
    QSqlQuery replay(m_db);
    replay.prepare("SELECT fingerprint,response FROM build_requests WHERE scope=? AND key=?");
    replay.addBindValue(scope);
    replay.addBindValue(key);
    replay.exec();
    if (replay.next()) {
        if (replay.value(0) != fingerprint)
            return fail("request_id was already used for different input");
        auto result = decoded(replay.value(1));
        sql("ROLLBACK");
        result["replayed"] = true;
        return result;
    }
    int category = scope, revision = 1;
    if (operation != "create") {
        existing = read(scope, road);
        category = existing["workspace"].toInt();
        if (!a.contains("expected_revision") ||
            a["expected_revision"].toLongLong() != existing["revision"].toLongLong())
            return fail("Stale roadmap revision; reload before changing it");
        revision = existing["revision"].toInt() + 1;
        if (operation != "fork" && !sql("UPDATE build_roads SET revision=? WHERE id=? AND revision=?",
                                        {revision, road, a["expected_revision"]}))
            return fail("Could not advance roadmap revision");
    }
    const auto now = stamp();
    QVariantMap response{{"ok", true}, {"schema_version", 1}, {"request_id", key}};
    auto title = SessionLedger::redact(a.value("title").toString()).trimmed().left(160);
    auto node = a.value("node").toString();
    auto nodeExists = [&](const QString &id) {
        QSqlQuery q(m_db);
        q.prepare("SELECT 1 FROM build_nodes WHERE id=? AND road=?");
        q.addBindValue(id);
        q.addBindValue(road);
        q.exec();
        return q.next();
    };
    if (!node.isEmpty() && !nodeExists(node))
        return fail("Step is outside this roadmap");
    if (operation == "create" || operation == "fork") {
        if (title.isEmpty())
            return fail("A roadmap title is required");
        if (operation == "fork" && a.contains("checkpoint")) {
            QSqlQuery snapshot(m_db);
            snapshot.prepare("SELECT snapshot FROM build_checkpoints WHERE id=? AND road=?");
            snapshot.addBindValue(a["checkpoint"]);
            snapshot.addBindValue(road);
            if (!snapshot.exec() || !snapshot.next())
                return fail("Checkpoint is outside this roadmap");
            existing = decoded(snapshot.value(0));
        }
        if (operation == "fork" && !node.isEmpty()) {
            const auto all = existing["nodes"].toList();
            QSet<QString> selected{node};
            bool found = false;
            for (auto value : all)
                if (value.toMap()["id"].toString() == node)
                    found = true;
            if (!found)
                return fail("Selected item is absent from this checkpoint");
            bool changed = true;
            while (changed) {
                changed = false;
                for (auto value : all) {
                    auto item = value.toMap();
                    if (selected.contains(item["parent"].toString()) &&
                        !selected.contains(item["id"].toString())) {
                        selected.insert(item["id"].toString());
                        changed = true;
                    }
                }
            }
            QVariantList chosen;
            for (auto value : all)
                if (selected.contains(value.toMap()["id"].toString()))
                    chosen.append(value);
            existing["nodes"] = chosen;
            for (const auto &key : {"documents", "links"}) {
                QVariantList filtered;
                for (auto value : existing[key].toList())
                    if (selected.contains(value.toMap()["node"].toString()))
                        filtered.append(value);
                existing[key] = filtered;
            }
            response["source_node"] = node;
        }
        auto previous = road;
        road = uid();
        revision = 1;
        if (!sql("INSERT INTO "
                 "build_roads(id,category,title,revision,base_road,base_revision,branch_ref,commit_ref,"
                 "created) VALUES(?,?,?,?,?,?,?,?,?)",
                 {road, category, title, 1, previous.isEmpty() ? QVariant() : previous,
                  operation == "fork" ? existing["revision"] : QVariant(), a.value("branch_ref", QString("")),
                  a.value("commit_ref", QString("")), now}))
            return fail("Could not create roadmap");
        if (operation == "fork") {
            QMap<QString, QString> nodes;
            for (auto v : existing["nodes"].toList()) {
                auto item = v.toMap();
                nodes[item["id"].toString()] = uid();
            }
            // Parent links are filled after all nodes exist, so source ordering does not matter.
            for (auto v : existing["nodes"].toList()) {
                auto item = v.toMap();
                if (!sql("INSERT INTO build_nodes(id,road,kind,title,body,state,created) "
                         "VALUES(?,?,?,?,?,?,?)",
                         {nodes[item["id"].toString()], road, item["kind"], item["title"], item["body"],
                          item["state"], now}))
                    return fail("Could not fork steps");
            }
            for (auto v : existing["nodes"].toList()) {
                auto item = v.toMap();
                if (nodes.contains(item["parent"].toString()))
                    if (!sql("UPDATE build_nodes SET parent=? WHERE id=?",
                             {nodes[item["parent"].toString()], nodes[item["id"].toString()]}))
                        return fail("Could not fork step ancestry");
            }
            QSet<QString> copied;
            for (auto v : existing["documents"].toList()) {
                auto item = v.toMap();
                auto oldDoc = item["document"].toString();
                if (copied.contains(oldDoc))
                    continue;
                copied.insert(oldDoc);
                QString doc = uid();
                auto oldNode = item["node"].toString();
                if (!sql("INSERT INTO build_docs(id,road,node,title) VALUES(?,?,?,?)",
                         {doc, road, oldNode.isEmpty() ? QVariant() : QVariant(nodes[oldNode]),
                          item["title"]}))
                    return fail("Could not fork documents");
                QSqlQuery version(m_db);
                version.prepare("SELECT content,digest FROM build_doc_versions WHERE id=? AND doc=?");
                version.addBindValue(item["version"]);
                version.addBindValue(oldDoc);
                if (!version.exec() || !version.next())
                    return fail("Fork document version is missing");
                if (!sql("INSERT INTO "
                         "build_doc_versions(id,doc,parent_version,number,content,digest,branch_ref,commit_"
                         "ref,created) VALUES(?,?,?,1,?,?,?,?,?)",
                         {uid(), doc, item["version"], version.value(0), version.value(1),
                          a.value("branch_ref", QString("")), a.value("commit_ref", QString("")), now}))
                    return fail("Could not fork document version");
            }
            for (auto v : existing["links"].toList()) {
                auto item = v.toMap();
                auto oldNode = item["node"].toString();
                if (!sql("INSERT INTO build_links(id,road,node,kind,ref,created) VALUES(?,?,?,?,?,?)",
                         {uid(), road, oldNode.isEmpty() ? QVariant() : QVariant(nodes[oldNode]),
                          item["kind"], item["ref"], now}))
                    return fail("Could not fork evidence references");
            }
            response["node_map"] = QVariantMap();
            QVariantMap mapping;
            for (auto i = nodes.begin(); i != nodes.end(); ++i)
                mapping[i.key()] = i.value();
            response["node_map"] = mapping;
        }
    } else if (operation == "node.add") {
        if (title.isEmpty())
            return fail("A step title is required");
        auto kind = a.value("kind").toString();
        if (!QStringList{"milestone", "checkpoint", "step", "substep", "issue", "test", "debug",
                         "documentation"}
                 .contains(kind))
            return fail("Unknown step kind");
        const auto parent = a.value("parent").toString();
        if (kind == "substep" && parent.isEmpty())
            return fail("A substep requires a parent step");
        if (!parent.isEmpty() && !nodeExists(parent))
            return fail("Parent step is outside this roadmap");
        if (existing["nodes"].toList().size() >= 1000)
            return fail("Roadmap step limit reached; create a focused roadmap");
        node = uid();
        if (!sql("INSERT INTO build_nodes(id,road,parent,kind,title,body,state,created) "
                 "VALUES(?,?,?,?,?,?,'planned',?)",
                 {node, road, parent.isEmpty() ? QVariant() : parent, kind, title,
                  SessionLedger::redact(a.value("body", QString("")).toString()), now}))
            return fail("Could not add step");
        response["node"] = node;
    } else if (operation == "node.update") {
        if (node.isEmpty())
            return fail("Select a step");
        auto state = a.value("state").toString();
        if (!QStringList{"planned", "in_progress", "blocked", "recorded", "archived"}.contains(state))
            return fail("Unknown planning state; verification is owned by the core");
        if (!sql("UPDATE build_nodes SET state=?,body=COALESCE(?,body) WHERE id=? AND road=?",
                 {state,
                  a.contains("body") ? QVariant(SessionLedger::redact(a["body"].toString())) : QVariant(),
                  node, road}))
            return fail("Could not update step");
    } else if (operation == "document.save") {
        auto document = a.value("document").toString();
        int number = 1;
        QString previous = a.value("parent_version").toString();
        if (document.isEmpty()) {
            if (title.isEmpty() || !previous.isEmpty())
                return fail("New document needs a title and no parent_version");
            document = uid();
            if (!sql("INSERT INTO build_docs(id,road,node,title) VALUES(?,?,?,?)",
                     {document, road, node.isEmpty() ? QVariant() : node, title}))
                return fail("Could not create document");
        } else {
            QSqlQuery q(m_db);
            q.prepare("SELECT v.id,v.number,d.title FROM build_docs d JOIN build_doc_versions v ON "
                      "v.doc=d.id WHERE "
                      "d.id=? AND d.road=? ORDER BY v.number DESC LIMIT 1");
            q.addBindValue(document);
            q.addBindValue(road);
            if (!q.exec() || !q.next() || previous != q.value(0).toString())
                return fail("Document parent_version is stale or outside this roadmap");
            if (!title.isEmpty() && title != q.value(2).toString())
                return fail("Document identity title is stable; create a new document to rename it");
            number = q.value(1).toInt() + 1;
        }
        if (!a.contains("content"))
            return fail("Document content is required");
        const auto content = SessionLedger::redact(a["content"].toString()), hash = digest(content),
                   version = uid();
        if (existing["documents"].toList().size() >= 1000)
            return fail("Document version limit reached; fork a focused roadmap");
        const QString objects = m_directory + "/objects";
        if (QFileInfo(m_directory).isSymLink() || QFileInfo(objects).isSymLink())
            return fail("Document storage cannot be a symlink");
        if (!QDir().mkpath(objects))
            return fail("Could not create private document storage");
        QFile::setPermissions(m_directory, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
        QFile::setPermissions(objects, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner);
        const QString path = objects + "/" + hash + ".txt";
        QFile prior(path);
        if (QFileInfo(path).isSymLink())
            return fail("Document object cannot be a symlink");
        if (QFile::exists(path)) {
            if (!prior.open(QIODevice::ReadOnly) || prior.readAll() != content.toUtf8())
                return fail("Document object failed its digest check");
        } else {
            QSaveFile file(path);
            if (!file.open(QIODevice::WriteOnly) || file.write(content.toUtf8()) != content.toUtf8().size() ||
                !file.commit())
                return fail("Could not write document object");
            QFile::setPermissions(path, QFile::ReadOwner | QFile::WriteOwner);
        }
        if (!sql("INSERT INTO "
                 "build_doc_versions(id,doc,parent_version,number,content,digest,branch_ref,commit_ref,"
                 "created) VALUES(?,?,?,?,?,?,?,?,?)",
                 {version, document, previous.isEmpty() ? QVariant() : previous, number, content, hash,
                  a.value("branch_ref", existing.value("branch_ref", QString(""))),
                  a.value("commit_ref", existing.value("commit_ref", QString(""))), now}))
            return fail("Could not save document version");
        response["document"] = document;
        response["version"] = version;
        response["digest"] = hash;
    } else if (operation == "link") {
        auto kind = a.value("kind").toString(), ref = a.value("ref").toString();
        QSqlQuery check(m_db);
        if (kind == "agent_run") {
            check.prepare("SELECT 1 FROM agent_runs WHERE id=?");
            check.addBindValue(ref);
        } else if (kind == "chat_event" || kind == "terminal_command") {
            auto event = m_events.read(category, ref.toLongLong());
            if (event.contains("error"))
                return fail("Event is outside the roadmap workspace");
            if (kind == "terminal_command" && !event["kind"].toString().startsWith("command."))
                return fail("Event is not a recorded command");
        } else
            return fail("Unknown link kind");
        if (kind == "agent_run") {
            if (!check.exec() || !check.next())
                return fail("Agent run does not exist");
            QSqlQuery target(m_db);
            target.prepare("SELECT target FROM agent_runs WHERE id=?");
            target.addBindValue(ref);
            target.exec();
            target.next();
            if (!m_events.contains(category, decoded(target.value(0))["id"].toInt()))
                return fail("Agent run is outside the roadmap workspace");
        }
        if (existing["links"].toList().size() >= 1000)
            return fail("Link limit reached");
        auto link = uid();
        if (!sql("INSERT INTO build_links(id,road,node,kind,ref,created) VALUES(?,?,?,?,?,?)",
                 {link, road, node.isEmpty() ? QVariant() : node, kind, ref, now}))
            return fail("Could not link evidence");
        response["link"] = link;
    } else if (operation == "checkpoint") {
        auto snapshot = encoded(existing), hash = digest(snapshot), id = uid();
        if (existing["checkpoints"].toList().size() >= 1000)
            return fail("Checkpoint limit reached");
        if (!sql(
                "INSERT INTO build_checkpoints(id,road,revision,snapshot,digest,created) VALUES(?,?,?,?,?,?)",
                {id, road, existing["revision"], snapshot, hash, now}))
            return fail("Could not save checkpoint");
        response["checkpoint"] = id;
        response["digest"] = hash;
    }
    const auto event = m_events.record(
        category, "build." + operation, title.isEmpty() ? operation : title,
        encoded(QVariantMap{{"roadmap", road}, {"revision", revision}, {"node", node}}), "build:" + road);
    if (!event ||
        !sql("INSERT INTO build_changes(road,revision,operation,payload,created,event) VALUES(?,?,?,?,?,?)",
             {road, revision, operation, SessionLedger::redact(encoded(a)), now, event}))
        return fail("Could not record build change");
    response["roadmap"] = road;
    response["revision"] = revision;
    response["event"] = event;
    response["master_chat"] = 1;
    if (!sql("INSERT INTO build_requests(scope,key,fingerprint,response) VALUES(?,?,?,?)",
             {scope, key, fingerprint, encoded(response)}) ||
        !sql("COMMIT"))
        return fail("Could not commit build change");
    return response;
}
