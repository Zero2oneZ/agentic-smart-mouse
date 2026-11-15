#include "sessionledger.h"
#include <QDateTime>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSqlQuery>
#include <QUuid>

SessionLedger::SessionLedger(const QSqlDatabase &db)
    : m_db(db), m_session(QUuid::createUuid().toString(QUuid::WithoutBraces)) {
    QSqlQuery q(m_db);
    q.exec("CREATE TABLE IF NOT EXISTS events(id INTEGER PRIMARY KEY AUTOINCREMENT, category INTEGER NOT "
           "NULL REFERENCES categories(id), session TEXT NOT NULL, kind TEXT NOT NULL, summary TEXT NOT "
           "NULL, content TEXT NOT NULL, source TEXT NOT NULL, created TEXT NOT NULL)");
    q.exec("CREATE INDEX IF NOT EXISTS events_category_id ON events(category,id)");
    q.exec("CREATE TABLE IF NOT EXISTS event_links(category INTEGER NOT NULL REFERENCES categories(id), "
           "event INTEGER NOT NULL REFERENCES events(id), PRIMARY KEY(category,event))");
    // Older conversations become discoverable without changing their original message rows.
    q.exec("INSERT INTO events(category,session,kind,summary,content,source,created) SELECT "
           "m.category,'legacy','chat.'||m.role,substr(m.content,1,160),m.content,'message:'||m.id,m.created|"
           "|'Z' FROM messages m WHERE m.content<>'' AND NOT EXISTS(SELECT 1 FROM events e WHERE "
           "e.source='message:'||m.id) ORDER BY m.id");
}
bool SessionLedger::privatePath(const QString &path) {
    static const QRegularExpression deny(
        R"((^|/)(\.git|\.aws|\.ssh|\.env(?:\..*)?|credentials(?:\..*)?|secrets?(?:\..*)?|auth\.json|id_rsa|id_ed25519)(/|$)|\.(pem|key|p12|sqlite3|db)(-|$))",
        QRegularExpression::CaseInsensitiveOption);
    return deny.match(path).hasMatch();
}
QString SessionLedger::redact(QString text) {
    text.replace(QRegularExpression(R"((?i)(authorization\s*[:=]\s*(?:bearer\s+)?)[^\s"\r\n]+)"),
                 "\\1[REDACTED]");
    text.replace(
        QRegularExpression(
            R"((?i)((?:api[_-]?key|password|access[_-]?token|secret)\s*["']?\s*[:=]\s*["']?)[^\s"'\r\n,}]+)"),
        "\\1[REDACTED]");
    text.replace(
        QRegularExpression(
            R"(\b(?:sk-[A-Za-z0-9_-]{16,}|gh[pousr]_[A-Za-z0-9_]{20,}|github_pat_[A-Za-z0-9_]{20,})\b)"),
        "[REDACTED]");
    text.replace(QRegularExpression(R"((https?://)[^/\s:@]+:[^/\s@]+@)"), "\\1[REDACTED]@");
    text.replace(
        QRegularExpression(R"(-----BEGIN [^-]*PRIVATE KEY-----[\s\S]*?-----END [^-]*PRIVATE KEY-----)"),
        "[REDACTED PRIVATE KEY]");
    return text;
}
qint64 SessionLedger::record(int category, const QString &kind, const QString &summary,
                             const QString &content, const QString &source) {
    QSqlQuery q(m_db);
    q.prepare(
        "INSERT INTO events(category,session,kind,summary,content,source,created) VALUES(?,?,?,?,?,?,?)");
    for (const auto &v :
         QVariantList{category, m_session, kind.left(64), redact(summary.simplified()).left(180),
                      redact(content).left(262144), source.isNull() ? QString("") : redact(source),
                      QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)})
        q.addBindValue(v);
    return q.exec() ? q.lastInsertId().toLongLong() : 0;
}
bool SessionLedger::contains(int scope, int category) const {
    if (scope == 1)
        return true;
    QSqlQuery q(m_db);
    q.prepare("WITH RECURSIVE branch(id) AS (SELECT id FROM categories WHERE id=? UNION ALL SELECT c.id FROM "
              "categories c JOIN branch b ON c.parent=b.id) SELECT 1 FROM branch WHERE id=?");
    q.addBindValue(scope);
    q.addBindValue(category);
    q.exec();
    return q.next();
}
QVariantList SessionLedger::timeline(int category, qint64 before, int limit) const {
    QSqlQuery q(m_db);
    q.prepare("WITH RECURSIVE branch(id) AS (SELECT id FROM categories WHERE id=? UNION ALL SELECT c.id FROM "
              "categories c JOIN branch b ON c.parent=b.id) SELECT "
              "e.id,e.category,c.name,e.kind,e.summary,e.created,e.session,e.source FROM events e JOIN "
              "categories c ON c.id=e.category WHERE (?=1 OR e.category IN(SELECT id FROM branch)) AND (?=0 "
              "OR e.id<?) ORDER BY e.id DESC LIMIT ?");
    for (const auto &v : QVariantList{category, category, before, before, qBound(1, limit, 100)})
        q.addBindValue(v);
    q.exec();
    QVariantList rows;
    while (q.next())
        rows.append(QVariantMap{{"id", q.value(0)},
                                {"category", q.value(1)},
                                {"name", q.value(2)},
                                {"kind", q.value(3)},
                                {"summary", redact(q.value(4).toString()).left(180)},
                                {"created", q.value(5)},
                                {"session", q.value(6)},
                                {"source", redact(q.value(7).toString())}});
    return rows;
}
void SessionLedger::grant(int category, qint64 event) {
    QSqlQuery q(m_db);
    q.prepare("INSERT OR IGNORE INTO event_links(category,event) VALUES(?,?)");
    q.addBindValue(category);
    q.addBindValue(event);
    q.exec();
}
QVariantMap SessionLedger::read(int category, qint64 event, int offset) const {
    QSqlQuery q(m_db);
    q.prepare("SELECT category,kind,summary,content,source,created FROM events WHERE id=?");
    q.addBindValue(event);
    q.exec();
    if (!q.next())
        return {{"error", "Event is outside this workspace"}};
    if (!contains(category, q.value(0).toInt())) {
        QSqlQuery linked(m_db);
        linked.prepare("SELECT 1 FROM event_links WHERE category=? AND event=?");
        linked.addBindValue(category);
        linked.addBindValue(event);
        linked.exec();
        if (!linked.next())
            return {{"error", "Event is outside this workspace"}};
    }
    QString content = redact(q.value(3).toString());
    offset = qBound(0, offset, int(content.size()));
    return {{"id", event},
            {"category", q.value(0)},
            {"kind", q.value(1)},
            {"summary", redact(q.value(2).toString())},
            {"content", content.mid(offset, 6000)},
            {"offset", offset},
            {"nextOffset", offset + 6000 < content.size() ? offset + 6000 : -1},
            {"source", redact(q.value(4).toString())},
            {"created", q.value(5)}};
}
