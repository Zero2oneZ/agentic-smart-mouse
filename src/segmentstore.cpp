#include "segmentstore.h"
#include <QCryptographicHash>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSqlQuery>
#include <QUuid>

namespace {
QString uuid(const QString &prefix) { return prefix + QUuid::createUuid().toString(QUuid::WithoutBraces); }
QString now() { return QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs); }
QString text(const QString &s) { return SessionLedger::redact(s.isNull() ? QString("") : s); }
QVariantMap error(const QString &s) { return {{"error", s}}; }
bool exec(QSqlDatabase db, const QString &sql, const QVariantList &args = {}) {
    QSqlQuery q(db);
    q.prepare(sql);
    for (const auto &v : args)
        q.addBindValue(v);
    return q.exec();
}
} // namespace
SegmentStore::SegmentStore(const QSqlDatabase &db, SessionLedger &ledger) : m_db(db), m_ledger(ledger) {
    exec(m_db, "CREATE TABLE IF NOT EXISTS segments(seq INTEGER PRIMARY KEY AUTOINCREMENT,id TEXT UNIQUE NOT "
               "NULL,category INTEGER NOT NULL REFERENCES categories(id),kind TEXT NOT NULL,title TEXT NOT "
               "NULL,description TEXT NOT NULL,summary TEXT NOT NULL DEFAULT '',state TEXT NOT NULL DEFAULT "
               "'collecting',created TEXT NOT NULL,updated TEXT NOT NULL,worker TEXT NOT NULL DEFAULT "
               "'',token TEXT NOT NULL DEFAULT '',expires INTEGER NOT NULL DEFAULT 0,attempts INTEGER NOT "
               "NULL DEFAULT 0,error TEXT NOT NULL DEFAULT '')");
    exec(m_db, "CREATE INDEX IF NOT EXISTS segments_queue ON segments(state,expires,seq)");
    exec(m_db, "CREATE TABLE IF NOT EXISTS segment_artifacts(seq INTEGER PRIMARY KEY AUTOINCREMENT,id TEXT "
               "UNIQUE NOT NULL,segment TEXT NOT NULL REFERENCES segments(id),kind TEXT NOT NULL,content "
               "TEXT NOT NULL,source TEXT NOT NULL,event INTEGER REFERENCES events(id),sha256 TEXT NOT "
               "NULL,created TEXT NOT NULL)");
    exec(m_db, "CREATE TABLE IF NOT EXISTS segment_edges(src TEXT NOT NULL REFERENCES segments(id),dst TEXT "
               "NOT NULL REFERENCES segments(id),relation TEXT NOT NULL,PRIMARY KEY(src,dst,relation))");
    exec(m_db, "CREATE TABLE IF NOT EXISTS segment_grants(category INTEGER NOT NULL REFERENCES "
               "categories(id),segment TEXT NOT NULL REFERENCES segments(id),PRIMARY KEY(category,segment))");
    exec(m_db, "CREATE TABLE IF NOT EXISTS segment_revisions(seq INTEGER PRIMARY KEY AUTOINCREMENT,segment "
               "TEXT NOT NULL REFERENCES segments(id),description TEXT NOT NULL,summary TEXT NOT NULL,worker "
               "TEXT NOT NULL,created TEXT NOT NULL)");
}
bool SegmentStore::visible(int scope, const QString &id) const {
    QSqlQuery q(m_db);
    q.prepare("SELECT category FROM segments WHERE id=?");
    q.addBindValue(id);
    q.exec();
    if (!q.next())
        return false;
    if (m_ledger.contains(scope, q.value(0).toInt()))
        return true;
    q.prepare("SELECT 1 FROM segment_grants WHERE category=? AND segment=?");
    q.addBindValue(scope);
    q.addBindValue(id);
    q.exec();
    return q.next();
}
QVariantMap SegmentStore::create(int scope, int category, const QString &kind, const QString &title,
                                 const QString &description) {
    if (!m_ledger.contains(scope, category))
        return error("Workspace is outside this scope");
    if (title.trimmed().isEmpty() || title.size() > 200 || description.size() > 4000 ||
        !QRegularExpression("^[a-z][a-z0-9_.-]{0,63}$").match(kind).hasMatch())
        return error("Supply a title up to 200 characters, a description up to 4000 characters, and a simple "
                     "kind such as web_search");
    QString id = uuid("seg_"), time = now();
    if (!exec(
            m_db,
            "INSERT INTO segments(id,category,kind,title,description,created,updated) VALUES(?,?,?,?,?,?,?)",
            {id, category, kind, text(title.trimmed()), text(description), time, time}))
        return error("Could not create segment");
    exec(
        m_db,
        "INSERT INTO segment_revisions(segment,description,summary,worker,created) VALUES(?,?,'','author',?)",
        {id, text(description), time});
    m_ledger.record(category, "segment.created", title, id + "\n" + description, id);
    return read(scope, id);
}
QVariantMap SegmentStore::insertArtifact(const QString &id, const QString &kind, const QString &content,
                                         const QString &source, qint64 event) {
    if (content.toUtf8().size() > 1024 * 1024 || source.size() > 4000 ||
        !QRegularExpression("^[a-z][a-z0-9_.-]{0,63}$").match(kind).hasMatch())
        return error("Artifacts require a simple kind and at most 1 MiB of text; split larger artifacts into "
                     "ordered parts");
    QString safe = text(content), artifact = uuid("part_"),
            hash = QString::fromLatin1(
                QCryptographicHash::hash(safe.toUtf8(), QCryptographicHash::Sha256).toHex());
    if (!exec(m_db,
              "INSERT INTO segment_artifacts(id,segment,kind,content,source,event,sha256,created) "
              "VALUES(?,?,?,?,?,?,?,?)",
              {artifact, id, kind, safe, text(source), event ? QVariant(event) : QVariant(), hash, now()}))
        return error("Could not store segment artifact");
    return {{"id", artifact}, {"segment", id}, {"sha256", hash}};
}
QVariantMap SegmentStore::add(int scope, const QString &id, const QString &kind, const QString &content,
                              const QString &source, qint64 event) {
    if (!visible(scope, id))
        return error("Segment is outside this scope");
    if (!m_db.transaction())
        return error("Database is busy");
    QSqlQuery q(m_db);
    q.prepare("SELECT state,category FROM segments WHERE id=?");
    q.addBindValue(id);
    q.exec();
    q.next();
    if (q.value(0).toString() != "collecting") {
        m_db.rollback();
        return error("This segment is sealed; create a continuation to preserve its original evidence");
    }
    int category = q.value(1).toInt();
    if (!m_ledger.contains(scope, category)) {
        m_db.rollback();
        return error("Linked segments are read-only in this workspace");
    }
    if (event && m_ledger.read(scope, event).contains("error")) {
        m_db.rollback();
        return error("Source event is outside this scope");
    }
    auto result = insertArtifact(id, kind, content, source, event);
    if (result.contains("error")) {
        m_db.rollback();
        return result;
    }
    if (!exec(m_db, "UPDATE segments SET updated=? WHERE id=?", {now(), id}) || !m_db.commit()) {
        m_db.rollback();
        return error("Could not save artifact");
    }
    m_ledger.record(category, "segment.artifact", kind + " added", result["id"].toString(), id);
    return result;
}
QVariantMap SegmentStore::read(int scope, const QString &id) const {
    if (!visible(scope, id))
        return error("Segment is outside this scope");
    QSqlQuery q(m_db);
    q.prepare("SELECT "
              "seq,category,kind,title,description,summary,state,created,updated,attempts,error,worker,"
              "expires FROM "
              "segments WHERE id=?");
    q.addBindValue(id);
    q.exec();
    if (!q.next())
        return error("Segment not found");
    QVariantMap result{{"id", id}};
    QStringList names{"seq",     "category", "kind",     "title",     "description", "summary", "state",
                      "created", "updated",  "attempts", "lastError", "worker",      "expires"};
    for (int i = 0; i < names.size(); ++i)
        result[names[i]] = q.value(i);
    q.prepare("SELECT id,kind,source,sha256,length(content),event,created FROM segment_artifacts WHERE "
              "segment=? ORDER BY seq");
    q.addBindValue(id);
    q.exec();
    QVariantList artifacts;
    while (q.next())
        artifacts.append(QVariantMap{{"id", q.value(0)},
                                     {"kind", q.value(1)},
                                     {"source", q.value(2)},
                                     {"sha256", q.value(3)},
                                     {"characters", q.value(4)},
                                     {"event", q.value(5)},
                                     {"created", q.value(6)}});
    result["artifacts"] = artifacts;
    q.prepare("SELECT src,dst,relation FROM segment_edges WHERE src=? OR dst=? ORDER BY src,dst,relation");
    q.addBindValue(id);
    q.addBindValue(id);
    q.exec();
    QVariantList links;
    while (q.next()) {
        QString other = q.value(0).toString() == id ? q.value(1).toString() : q.value(0).toString();
        if (visible(scope, other))
            links.append(QVariantMap{{"from", q.value(0)}, {"to", q.value(1)}, {"relation", q.value(2)}});
    }
    result["links"] = links;
    q.prepare("SELECT description,summary,worker,created FROM segment_revisions WHERE segment=? ORDER BY seq "
              "DESC LIMIT 10");
    q.addBindValue(id);
    q.exec();
    QVariantList revisions;
    while (q.next())
        revisions.append(QVariantMap{{"description", q.value(0)},
                                     {"summary", q.value(1)},
                                     {"worker", q.value(2)},
                                     {"created", q.value(3)}});
    result["revisions"] = revisions;
    return result;
}
QVariantMap SegmentStore::artifact(int scope, const QString &id, int offset) const {
    QSqlQuery q(m_db);
    q.prepare("SELECT segment,kind,content,source,event,sha256 FROM segment_artifacts WHERE id=?");
    q.addBindValue(id);
    q.exec();
    if (!q.next() || !visible(scope, q.value(0).toString()))
        return error("Artifact is outside this scope");
    if (!q.value(4).isNull()) {
        QSqlQuery owner(m_db);
        owner.prepare("SELECT category FROM segments WHERE id=?");
        owner.addBindValue(q.value(0));
        owner.exec();
        owner.next();
        auto result = m_ledger.read(owner.value(0).toInt(), q.value(4).toLongLong(), offset);
        result["artifact"] = id;
        result["segment"] = q.value(0);
        return result;
    }
    QString content = q.value(2).toString();
    offset = qBound(0, offset, int(content.size()));
    return {{"id", id},
            {"segment", q.value(0)},
            {"kind", q.value(1)},
            {"content", content.mid(offset, 6000)},
            {"source", q.value(3)},
            {"sha256", q.value(5)},
            {"offset", offset},
            {"nextOffset", offset + 6000 < content.size() ? offset + 6000 : -1}};
}
QVariantList SegmentStore::list(int scope, const QString &state, const QString &query, qint64 before,
                                int limit) const {
    QSqlQuery q(m_db);
    q.prepare(
        "WITH RECURSIVE branch(id) AS (SELECT id FROM categories WHERE id=? UNION ALL SELECT c.id FROM "
        "categories c JOIN branch b ON c.parent=b.id) SELECT "
        "s.id,s.seq,s.category,s.kind,s.title,s.description,s.summary,s.state,s.created,s.updated,s.attempts,"
        "s.error FROM segments s WHERE (?=1 OR s.category "
        "IN(SELECT id FROM branch) OR s.id IN(SELECT segment FROM segment_grants WHERE category=?)) AND "
        "(?='' OR s.state=?) AND (?='' OR instr(lower(s.title||' '||s.description||' '||s.summary||' "
        "'||s.kind||' '||s.id),lower(?))>0) AND (?=0 OR s.seq<?) ORDER BY s.seq DESC LIMIT ?");
    for (const auto &v :
         QVariantList{scope, scope, scope, state.isNull() ? QString("") : state,
                      state.isNull() ? QString("") : state, query.isNull() ? QString("") : query,
                      query.isNull() ? QString("") : query, before, before, qBound(1, limit, 100)})
        q.addBindValue(v);
    q.exec();
    QVariantList rows;
    const QStringList names{"id",      "seq",   "category", "kind",    "title",    "description",
                            "summary", "state", "created",  "updated", "attempts", "lastError"};
    while (q.next()) {
        QVariantMap row;
        for (int i = 0; i < names.size(); ++i)
            row[names[i]] = q.value(i);
        rows.append(row);
    }
    return rows;
}
QVariantMap SegmentStore::ready(int scope, const QString &id) {
    auto segment = read(scope, id);
    if (segment.contains("error"))
        return segment;
    if (!m_ledger.contains(scope, segment["category"].toInt()))
        return error("Linked segments are read-only");
    if (segment["artifacts"].toList().isEmpty() && segment["links"].toList().isEmpty())
        return error("Add evidence or component links before making a segment ready");
    QSqlQuery q(m_db);
    q.prepare("UPDATE segments SET state='ready',updated=? WHERE id=? AND state='collecting'");
    q.addBindValue(now());
    q.addBindValue(id);
    if (!q.exec() || q.numRowsAffected() != 1)
        return error("Only collecting segments can become ready");
    m_ledger.record(segment["category"].toInt(), "segment.ready", segment["title"].toString(), id, id);
    return read(scope, id);
}
QVariantMap SegmentStore::claim(int scope, const QString &worker, int seconds) {
    if (worker.trimmed().isEmpty() || worker.size() > 100)
        return error("Supply a worker name up to 100 characters");
    if (!exec(m_db, "BEGIN IMMEDIATE"))
        return error("Database is busy");
    qint64 time = QDateTime::currentSecsSinceEpoch();
    QSqlQuery q(m_db);
    q.prepare("WITH RECURSIVE branch(id) AS (SELECT id FROM categories WHERE id=? UNION ALL SELECT c.id FROM "
              "categories c JOIN branch b ON c.parent=b.id) SELECT s.id FROM segments s JOIN categories c ON "
              "c.id=s.category WHERE c.archived=0 AND (?=1 OR s.category IN(SELECT id FROM branch)) AND "
              "(s.state='ready' OR (s.state='processing' AND s.expires<=?)) ORDER BY s.seq LIMIT 1");
    for (const auto &v : QVariantList{scope, scope, time})
        q.addBindValue(v);
    q.exec();
    if (!q.next()) {
        m_db.commit();
        return {{"empty", true}};
    }
    QString id = q.value(0).toString(), token = uuid("lease_");
    if (!exec(
            m_db,
            "UPDATE segments SET "
            "state='processing',worker=?,token=?,expires=?,attempts=attempts+1,error='',updated=? WHERE id=?",
            {text(worker), token, time + qBound(1, seconds, 3600), now(), id}) ||
        !m_db.commit()) {
        m_db.rollback();
        return error("Could not claim segment");
    }
    auto result = read(scope, id);
    m_ledger.record(result["category"].toInt(), "segment.claimed", result["title"].toString(),
                    id + "\nWorker: " + worker + "\nAttempt: " + result["attempts"].toString(), id);
    result["lease"] = token;
    result["expires"] = time + qBound(1, seconds, 3600);
    return result;
}
bool SegmentStore::lease(int scope, const QString &id, const QString &token) const {
    auto segment = read(scope, id);
    if (segment.contains("error") || !m_ledger.contains(scope, segment["category"].toInt()))
        return false;
    QSqlQuery q(m_db);
    q.prepare("SELECT 1 FROM segments WHERE id=? AND state='processing' AND token=? AND expires>?");
    q.addBindValue(id);
    q.addBindValue(token);
    q.addBindValue(QDateTime::currentSecsSinceEpoch());
    q.exec();
    return q.next();
}
QVariantMap SegmentStore::renew(int scope, const QString &id, const QString &token, int seconds) {
    if (!exec(m_db, "BEGIN IMMEDIATE"))
        return error("Database is busy");
    if (!lease(scope, id, token)) {
        m_db.rollback();
        return error("Lease is invalid or expired");
    }
    qint64 until = QDateTime::currentSecsSinceEpoch() + qBound(1, seconds, 3600);
    bool ok = exec(m_db, "UPDATE segments SET expires=?,updated=? WHERE id=?", {until, now(), id});
    if (!ok || !m_db.commit()) {
        m_db.rollback();
        return error("Could not renew lease");
    }
    return {{"id", id}, {"expires", until}};
}
QVariantMap SegmentStore::finish(int scope, const QString &id, const QString &token,
                                 const QString &description, const QString &summary, const QString &result) {
    if (description.trimmed().isEmpty() || summary.trimmed().isEmpty() || description.size() > 4000 ||
        summary.size() > 8000)
        return error(
            "Completion needs a description up to 4000 characters and summary up to 8000 characters");
    if (!exec(m_db, "BEGIN IMMEDIATE"))
        return error("Database is busy");
    if (!lease(scope, id, token)) {
        m_db.rollback();
        return error("Lease is invalid or expired");
    }
    auto part = insertArtifact(id, "result", result, "worker result");
    if (part.contains("error")) {
        m_db.rollback();
        return part;
    }
    auto segment = read(scope, id);
    QSqlQuery worker(m_db);
    worker.prepare("SELECT worker FROM segments WHERE id=?");
    worker.addBindValue(id);
    worker.exec();
    worker.next();
    bool ok =
        exec(m_db,
             "INSERT INTO segment_revisions(segment,description,summary,worker,created) VALUES(?,?,?,?,?)",
             {id, text(description), text(summary), worker.value(0), now()}) &&
        exec(m_db,
             "UPDATE segments SET state='complete',description=?,summary=?,updated=?,token='',expires=0 "
             "WHERE id=?",
             {text(description), text(summary), now(), id});
    if (!ok || !m_db.commit()) {
        m_db.rollback();
        return error("Could not complete segment");
    }
    m_ledger.record(segment["category"].toInt(), "segment.complete", segment["title"].toString(),
                    id + "\n" + summary, id);
    return read(scope, id);
}
QVariantMap SegmentStore::fail(int scope, const QString &id, const QString &token, const QString &message) {
    if (!exec(m_db, "BEGIN IMMEDIATE"))
        return error("Database is busy");
    if (!lease(scope, id, token)) {
        m_db.rollback();
        return error("Lease is invalid or expired");
    }
    auto segment = read(scope, id);
    auto part = insertArtifact(id, "worker_error", message, "worker failure");
    bool ok = !part.contains("error") &&
              exec(m_db, "UPDATE segments SET state='failed',error=?,token='',expires=0,updated=? WHERE id=?",
                   {text(message).left(4000), now(), id});
    if (!ok || !m_db.commit()) {
        m_db.rollback();
        return error("Could not record worker failure");
    }
    m_ledger.record(segment["category"].toInt(), "segment.failed", segment["title"].toString(),
                    id + "\n" + message.left(4000), id);
    return read(scope, id);
}
QVariantMap SegmentStore::retry(int scope, const QString &id) {
    auto segment = read(scope, id);
    if (segment.contains("error") || !m_ledger.contains(scope, segment["category"].toInt()))
        return error("Segment is outside this scope");
    QSqlQuery q(m_db);
    q.prepare("UPDATE segments SET state='ready',updated=? WHERE id=? AND state='failed'");
    q.addBindValue(now());
    q.addBindValue(id);
    if (!q.exec() || q.numRowsAffected() != 1)
        return error("Only failed segments can be retried");
    m_ledger.record(segment["category"].toInt(), "segment.retried", segment["title"].toString(), id, id);
    return read(scope, id);
}
QVariantMap SegmentStore::link(int scope, const QString &from, const QString &to, const QString &relation) {
    auto source = read(scope, from);
    if (source.contains("error") || !visible(scope, to))
        return error("Both segments must be in scope");
    if (!m_ledger.contains(scope, source["category"].toInt()) || source["state"] != "collecting")
        return error("Add links only to your collecting segment");
    if (from == to || !QStringList{"uses", "derived_from", "continues"}.contains(relation))
        return error("Choose another segment and uses, derived_from, or continues");
    if (!exec(m_db, "INSERT OR IGNORE INTO segment_edges(src,dst,relation) VALUES(?,?,?)",
              {from, to, relation}))
        return error("Could not link segments");
    m_ledger.record(source["category"].toInt(), "segment.linked", source["title"].toString(),
                    from + " " + relation + " " + to, from);
    return read(scope, from);
}
QVariantMap SegmentStore::compose(int scope, int category, const QString &title, const QStringList &ids) {
    if (ids.isEmpty() || ids.size() > 32)
        return error("Choose between 1 and 32 segments");
    for (const auto &id : ids)
        if (!visible(scope, id))
            return error("A component is outside this scope");
    if (!m_db.transaction())
        return error("Database is busy");
    auto result =
        create(scope, category, "bundle", title, "Bundle of linked segment summaries and original evidence");
    if (result.contains("error")) {
        m_db.rollback();
        return result;
    }
    for (const auto &id : ids) {
        auto linked = link(scope, result["id"].toString(), id, "uses");
        if (linked.contains("error")) {
            m_db.rollback();
            return linked;
        }
    }
    if (!m_db.commit()) {
        m_db.rollback();
        return error("Could not compose segment bundle");
    }
    return read(scope, result["id"].toString());
}
QVariantMap SegmentStore::process(int scope) {
    auto job = claim(scope, "builtin.extractive-v1");
    if (job.contains("error") || job.value("empty").toBool())
        return job;
    QString summary;
    QJsonArray evidence;
    for (const auto &v : job["artifacts"].toList()) {
        auto part = v.toMap();
        auto page = summary.size() < 4000 ? artifact(scope, part["id"].toString()) : QVariantMap{};
        QString excerpt = page["content"].toString().simplified().left(350);
        if (!excerpt.isEmpty())
            summary += part["kind"].toString() + ": " + excerpt + "\n";
        evidence.append(QJsonObject{{"id", part["id"].toString()},
                                    {"sha256", part["sha256"].toString()},
                                    {"kind", part["kind"].toString()}});
    }
    for (const auto &v : job["links"].toList()) {
        auto edge = v.toMap();
        if (edge["from"] != job["id"])
            continue;
        auto child = read(scope, edge["to"].toString());
        summary += QString("%1 [%2]: %3\n")
                       .arg(child["title"].toString(), child["id"].toString(),
                            child["summary"].toString().left(400));
    }
    if (summary.trimmed().isEmpty())
        summary = "Evidence retained; no text excerpt available.";
    QString description = job["description"].toString();
    if (description.isEmpty())
        description = job["kind"].toString() + ": " + job["title"].toString();
    QJsonObject output{{"processor", "builtin.extractive-v1"},
                       {"segment", job["id"].toString()},
                       {"evidence", evidence},
                       {"links", QJsonArray::fromVariantList(job["links"].toList())},
                       {"summary", summary.left(6000)}};
    return finish(scope, job["id"].toString(), job["lease"].toString(), description, summary.left(6000),
                  QString::fromUtf8(QJsonDocument(output).toJson()));
}
void SegmentStore::grant(int category, const QString &id) {
    exec(m_db, "INSERT OR IGNORE INTO segment_grants(category,segment) VALUES(?,?)", {category, id});
}
