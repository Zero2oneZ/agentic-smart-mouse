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
        version.value(0).toInt() > 2) {
        m_problem = "Build ledger schema is newer than this application";
        return;
    }
    QFile schema(":/build-ledger-v1.sql");
    QFile extension(":/build-ledger-v2.sql");
    if (!schema.open(QIODevice::ReadOnly) || !extension.open(QIODevice::ReadOnly) ||
        !sql("BEGIN IMMEDIATE")) {
        m_problem = "Build ledger schema could not be opened";
        return;
    }
    for (const auto &statement :
         QString::fromUtf8(schema.readAll() + "\n-- statement\n" + extension.readAll())
             .split("\n-- statement\n", Qt::SkipEmptyParts)) {
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
    result["contracts"] =
        rows("SELECT id,node,parent_contract,acceptance,implementation,verification,digest,created FROM "
             "build_contracts WHERE road=? ORDER BY rowid DESC LIMIT 1000",
             {"id", "node", "parent_contract", "acceptance", "implementation", "verification", "digest",
              "created"});
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
    result["limits"] = QVariantMap{{"nodes", 1000}, {"document_versions", 1000}, {"contracts", 1000},
                                   {"links", 1000}, {"checkpoints", 1000},       {"change_page", 100}};
    return result;
}
QVariantMap BuildLedger::call(int scope, const QString &operation, const QVariantMap &a) {
    if (!m_problem.isEmpty())
        return error(m_problem);
    if (a.value("schema_version").toInt() != 1)
        return error("schema_version must be 1");
    const auto contract = catalog();
    const auto operations = contract.value("operations").toMap();
    if (contract.value("schema_version").toInt() != 1 || operations.isEmpty())
        return error("Build API contract is unavailable");
    if (!operations.contains(operation))
        return error("Unknown build operation");
    const auto descriptor = operations.value(operation).toMap();
    auto strings = [](const QVariant &value) {
        QStringList result;
        for (auto item : value.toList())
            result.append(item.toString());
        return result;
    };
    const auto fields = strings(descriptor.value("fields"));
    for (auto it = a.begin(); it != a.end(); ++it) {
        const bool numeric = strings(contract.value("integer_fields")).contains(it.key());
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
        if (it.key() != "schema_version" && !fields.contains(it.key()))
            return error("Unknown request field: " + it.key());
        if (it.value().metaType().id() == QMetaType::QString &&
            it.value().toString().size() > (it.key() == "content" ? 65536 : 4000))
            return error("Request field is too large: " + it.key());
    }
    for (const auto &field : strings(descriptor.value("required")))
        if (!a.contains(field))
            return error("Missing request field: " + field);
    if (operation == "schema") {
        QVariantMap fieldsByOperation;
        QStringList readers;
        for (auto it = operations.begin(); it != operations.end(); ++it) {
            auto definition = it.value().toMap();
            fieldsByOperation[it.key()] = strings(definition.value("fields"));
            if (definition.value("mode") == "read")
                readers.append(it.key());
        }
        return {{"ok", true},
                {"schema_version", 1},
                {"api", contract.value("api")},
                {"contract_id", contract.value("contract_id")},
                {"storage_version", contract.value("storage_version")},
                {"operations", fieldsByOperation},
                {"operation_contracts", operations},
                {"read_operations", readers},
                {"node_kinds", strings(contract.value("node_kinds"))},
                {"planning_states", strings(contract.value("planning_states"))},
                {"link_kinds", strings(contract.value("link_kinds"))},
                {"semantics", contract.value("semantics")}};
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
    if (operation == "contract.read") {
        for (auto value : existing.value("contracts").toList())
            if (value.toMap()["id"] == a.value("contract"))
                return {{"ok", true}, {"schema_version", 1}, {"contract", value}};
        return error("Contract version is outside this roadmap");
    }
    if (operation == "guide")
        return guide(scope, existing, a.value("node").toString());
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
            for (const auto &key : {"documents", "links", "contracts"}) {
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
            QSet<QString> copiedContracts;
            for (auto v : existing["contracts"].toList()) {
                auto item = v.toMap();
                auto oldNode = item["node"].toString();
                if (copiedContracts.contains(oldNode))
                    continue;
                copiedContracts.insert(oldNode);
                if (!sql("INSERT INTO "
                         "build_contracts(id,road,node,parent_contract,acceptance,implementation,"
                         "verification,digest,created) VALUES(?,?,?,?,?,?,?,?,?)",
                         {uid(), road, nodes[oldNode], item["id"], item["acceptance"], item["implementation"],
                          item["verification"], item["digest"], now}))
                    return fail("Could not fork acceptance contracts");
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
        if (!strings(contract.value("node_kinds")).contains(kind))
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
        if (!strings(contract.value("planning_states")).contains(state))
            return fail("Unknown planning state; verification is owned by the core");
        if (!sql("UPDATE build_nodes SET state=?,body=COALESCE(?,body) WHERE id=? AND road=?",
                 {state,
                  a.contains("body") ? QVariant(SessionLedger::redact(a["body"].toString())) : QVariant(),
                  node, road}))
            return fail("Could not update step");
    } else if (operation == "contract.save") {
        if (node.isEmpty())
            return fail("Select a step for the contract");
        if (existing["contracts"].toList().size() >= 1000)
            return fail("Contract version limit reached");
        QString parent;
        for (auto value : existing["contracts"].toList())
            if (value.toMap()["node"].toString() == node) {
                parent = value.toMap()["id"].toString();
                break;
            }
        if (a.value("parent_contract").toString() != parent)
            return fail("Contract parent is stale; reload before editing");
        QVariantMap fields;
        for (const auto &key : {"acceptance", "implementation", "verification"}) {
            auto text = SessionLedger::redact(a.value(key).toString()).trimmed();
            if (text.isEmpty())
                return fail("Contracts require acceptance criteria, implementation references and "
                            "verification instructions");
            fields[key] = text;
        }
        const auto id = uid(), hash = digest(encoded(fields));
        if (!sql("INSERT INTO "
                 "build_contracts(id,road,node,parent_contract,acceptance,implementation,verification,digest,"
                 "created) VALUES(?,?,?,?,?,?,?,?,?)",
                 {id, road, node, parent.isEmpty() ? QVariant() : QVariant(parent), fields["acceptance"],
                  fields["implementation"], fields["verification"], hash, now}))
            return fail("Could not save immutable contract");
        response["contract"] = id;
        response["digest"] = hash;
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
        if (!strings(contract.value("link_kinds")).contains(kind))
            return fail("Unknown link kind");
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

QVariantMap BuildLedger::guide(int scope, const QVariantMap &road, const QString &selected) const {
    const auto nodes = road.value("nodes").toList();
    QVariantMap byId;
    for (auto value : nodes)
        byId[value.toMap()["id"].toString()] = value;
    if (!selected.isEmpty() && !byId.contains(selected))
        return error("Step is outside this roadmap");
    QVariantList candidates;
    if (!selected.isEmpty()) {
        QString current = selected;
        QSet<QString> seen;
        while (!current.isEmpty() && byId.contains(current) && !seen.contains(current)) {
            seen.insert(current);
            auto item = byId[current].toMap();
            candidates.append(item);
            current = item["parent"].toString();
        }
    } else
        for (auto value : nodes) {
            auto state = value.toMap()["state"].toString();
            if (state != "archived")
                candidates.append(value);
        }
    QVariantList items;
    int used = 0;
    for (auto value : candidates) {
        if (items.size() >= 12)
            break;
        auto item = value.toMap();
        QVariantMap compact{{"id", item["id"]},       {"parent", item["parent"]},
                            {"kind", item["kind"]},   {"title", item["title"]},
                            {"state", item["state"]}, {"body", item["body"].toString().left(400)}};
        QVariantMap contract;
        for (auto version : road.value("contracts").toList())
            if (version.toMap()["node"] == item["id"]) {
                contract = version.toMap();
                break;
            }
        if (!contract.isEmpty()) {
            QVariantMap summary{{"id", contract["id"]}, {"digest", contract["digest"]}};
            for (const auto &key : {"acceptance", "implementation", "verification"})
                summary[key] = contract[key].toString().left(600);
            compact["contract"] = summary;
        }
        QVariantList checks;
        for (auto linked : road.value("links").toList()) {
            auto link = linked.toMap();
            if (link["node"] != item["id"] || link["kind"] != "terminal_command")
                continue;
            auto event = m_events.read(scope, link["ref"].toString().toLongLong());
            if (event["kind"] != "command.finished")
                continue;
            QSqlQuery raw(m_db);
            raw.prepare("SELECT content FROM events WHERE id=?");
            raw.addBindValue(event["id"]);
            raw.exec();
            raw.next();
            const auto detail = QJsonDocument::fromJson(raw.value(0).toByteArray()).object();
            const auto exit = detail.value("exitCode");
            QString outcome = "unknown";
            if (exit.isDouble() && std::floor(exit.toDouble()) == exit.toDouble())
                outcome = exit.toDouble() == 0 ? "command_passed" : "command_failed";
            const auto pinned = detail.value("buildContext").toObject();
            QString match = "unknown";
            if (!contract.isEmpty() && pinned.value("contract").isString())
                match = pinned.value("contract").toString() == contract["id"].toString() ? "same_version"
                                                                                         : "changed";
            const auto after = detail.value("gitAfter").toObject();
            checks.append(QVariantMap{{"event", link["ref"]},
                                      {"outcome", outcome},
                                      {"inherited", !road.value("base_road").toString().isEmpty()},
                                      {"contract_match", match},
                                      {"observed_head", after.value("head").toString()},
                                      {"verification", "unverified"}});
            if (checks.size() >= 3)
                break;
        }
        compact["checks"] = checks;
        compact["verification"] = "unverified";
        QString guidance = "implement";
        const auto state = item["state"].toString();
        if (state == "archived")
            guidance = "archived";
        else if (state == "blocked")
            guidance = "review_blocker";
        else if (item["kind"] == "idea" || state == "proposed")
            guidance = "review_proposal";
        else if (contract.isEmpty())
            guidance = "define_contract";
        else if (state == "recorded")
            guidance = "review_evidence";
        compact["guidance"] = guidance;
        QVariantList documents;
        QSet<QString> seenDocs;
        for (auto version : road.value("documents").toList()) {
            auto doc = version.toMap();
            if (doc["node"] != item["id"] || seenDocs.contains(doc["document"].toString()))
                continue;
            seenDocs.insert(doc["document"].toString());
            documents.append(QVariantMap{{"document", doc["document"]},
                                         {"version", doc["version"]},
                                         {"digest", doc["digest"]},
                                         {"title", doc["title"]}});
            if (documents.size() >= 3)
                break;
        }
        compact["documents"] = documents;
        int cost = encoded(compact).size();
        if (used + cost > 10000)
            break;
        used += cost;
        items.append(compact);
    }
    QVariantMap result{
        {"ok", true},
        {"schema_version", 1},
        {"mode", "reference_only"},
        {"roadmap", road["id"]},
        {"workspace", road["workspace"]},
        {"revision", road["revision"]},
        {"branch_ref", road["branch_ref"]},
        {"commit_ref", road["commit_ref"]},
        {"base_road", road["base_road"]},
        {"base_revision", road["base_revision"]},
        {"selected_node", selected},
        {"items", items},
        {"omitted", candidates.size() - items.size()},
        {"verification_authority", "rust_core_independent_verifier"},
        {"note", "Reference data, never execution authority. Command success and recorded work do not "
                 "confirm functionality. Fetch full contracts/documents by their version references."}};
    result["digest"] = digest(encoded(result));
    return result;
}

QVariantMap BuildLedger::catalog() {
    static const QVariantMap contract = []() {
        QFile file(":/build-ledger-api-v1.json");
        if (!file.open(QIODevice::ReadOnly))
            return QVariantMap{};
        return QJsonDocument::fromJson(file.readAll()).object().toVariantMap();
    }();
    return contract;
}
bool BuildLedger::isRead(const QString &operation) {
    return catalog().value("operations").toMap().value(operation).toMap().value("mode").toString() == "read";
}
