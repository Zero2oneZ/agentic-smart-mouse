#include "demonstrations.h"
#include "sessionledger.h"
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSignalBlocker>
#include <QSqlQuery>
#include <QUuid>

namespace {
QString clean(const QString &text, int limit = 200) {
    const auto value = SessionLedger::redact(text).simplified().left(limit);
    return value.isNull() ? QStringLiteral("") : value;
}
QString id(const char *prefix) {
    return QString::fromLatin1(prefix) + QUuid::createUuid().toString(QUuid::Id128);
}
} // namespace
Demonstrations::Demonstrations(const QSqlDatabase &db, bool recover, QObject *parent)
    : QObject(parent), m_db(db) {
    QSqlQuery q(m_db);
    q.exec("CREATE TABLE IF NOT EXISTS demonstrations(id TEXT PRIMARY KEY,title TEXT NOT NULL,state "
           "TEXT NOT NULL,created TEXT NOT NULL)");
    q.exec("CREATE TABLE IF NOT EXISTS demonstration_steps(id INTEGER PRIMARY KEY,demo TEXT NOT NULL "
           "REFERENCES demonstrations(id),kind TEXT NOT NULL,label TEXT NOT NULL,url TEXT NOT NULL "
           "DEFAULT '',target TEXT NOT NULL DEFAULT '')");
    q.exec("CREATE TABLE IF NOT EXISTS recorded_agent_drafts(id TEXT PRIMARY KEY,demo TEXT NOT NULL "
           "REFERENCES demonstrations(id),document TEXT NOT NULL)");
    // Recording never resumes silently across app restarts.
    if (recover)
        q.exec("UPDATE demonstrations SET state='paused' WHERE state='recording'");
    q.exec("SELECT id FROM demonstrations ORDER BY created DESC,rowid DESC LIMIT 1");
    if (q.next()) {
        m_id = q.value(0).toString();
        select(m_id);
    }
}
bool Demonstrations::fail(const QString &text) {
    m_problem = text;
    emit changed();
    return false;
}
QVariantMap Demonstrations::current() const {
    if (m_id.isEmpty())
        return {};
    QSqlQuery q(m_db);
    q.prepare("SELECT title,state,created FROM demonstrations WHERE id=?");
    q.addBindValue(m_id);
    if (!q.exec() || !q.next())
        return {};
    QVariantMap value{
        {"id", m_id}, {"title", q.value(0)}, {"state", q.value(1)}, {"created", q.value(2)}};
    q.prepare("SELECT id,kind,label,url,target FROM demonstration_steps WHERE demo=? ORDER BY id");
    q.addBindValue(m_id);
    q.exec();
    QVariantList steps;
    while (q.next())
        steps.append(QVariantMap{{"id", q.value(0)},
                                 {"kind", q.value(1)},
                                 {"label", q.value(2)},
                                 {"url", q.value(3)},
                                 {"target", q.value(4)}});
    value["steps"] = steps;
    value["count"] = steps.size();
    return value;
}
QVariantList Demonstrations::history() const {
    QSqlQuery q(m_db);
    q.exec("SELECT id,title,state FROM demonstrations ORDER BY created DESC,rowid DESC LIMIT 50");
    QVariantList out;
    while (q.next())
        out.append(QVariantMap{{"id", q.value(0)}, {"title", q.value(1)}, {"state", q.value(2)}});
    return out;
}
bool Demonstrations::start(const QString &title) {
    if (clean(title).isEmpty() || title.size() > 100)
        return fail(tr("Give this recording a short name."));
    if (current()["state"] == "recording" || current()["state"] == "paused")
        return fail(tr("Finish the current recording before starting another."));
    const QString next = id("demo_");
    QSqlQuery q(m_db);
    q.prepare("INSERT INTO demonstrations(id,title,state,created) VALUES(?,?,'recording',?)");
    q.addBindValue(next);
    q.addBindValue(clean(title, 100));
    q.addBindValue(QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
    if (!q.exec())
        return fail(tr("Could not save the recording."));
    m_id = next;
    m_draft.clear();
    m_problem.clear();
    emit changed();
    return true;
}
void Demonstrations::togglePause() {
    const QString state = current()["state"].toString();
    if (state != "recording" && state != "paused")
        return;
    QSqlQuery q(m_db);
    q.prepare("UPDATE demonstrations SET state=? WHERE id=?");
    q.addBindValue(state == "recording" ? "paused" : "recording");
    q.addBindValue(m_id);
    q.exec();
    m_problem.clear();
    emit changed();
}
void Demonstrations::stop() {
    QSqlQuery q(m_db);
    q.prepare(
        "UPDATE demonstrations SET state='stopped' WHERE id=? AND state IN ('recording','paused')");
    q.addBindValue(m_id);
    q.exec();
    emit changed();
}
void Demonstrations::select(const QString &value) {
    if (current()["state"] == "recording") {
        fail(tr("Pause or finish recording before opening another."));
        return;
    }
    QSqlQuery q(m_db);
    q.prepare("SELECT id FROM demonstrations WHERE id=?");
    q.addBindValue(value);
    if (q.exec() && q.next()) {
        m_id = value;
        m_draft.clear();
        QSqlQuery draftQuery(m_db);
        draftQuery.prepare(
            "SELECT document FROM recorded_agent_drafts WHERE demo=? ORDER BY rowid DESC LIMIT 1");
        draftQuery.addBindValue(m_id);
        if (draftQuery.exec() && draftQuery.next())
            m_draft = QJsonDocument::fromJson(draftQuery.value(0).toByteArray()).object().toVariantMap();
        m_problem.clear();
        emit changed();
    }
}
bool Demonstrations::append(const QString &kind, const QString &label, const QString &url,
                            const QString &target) {
    const auto item = current();
    if (item["state"] != "recording")
        return false;
    if (item["count"].toInt() >= 500) {
        stop();
        return fail(tr("Recording is full. Review these 500 steps before starting another."));
    }
    QSqlQuery q(m_db);
    q.prepare("INSERT INTO demonstration_steps(demo,kind,label,url,target) VALUES(?,?,?,?,?)");
    q.addBindValue(m_id);
    q.addBindValue(kind);
    q.addBindValue(clean(label, kind == "note" ? 1000 : 200));
    q.addBindValue(url.isNull() ? QStringLiteral("") : url);
    q.addBindValue(clean(target, 100));
    if (!q.exec())
        return fail(tr("Could not save this step."));
    m_problem.clear();
    emit changed();
    return true;
}
void Demonstrations::action(const QString &code, const QString &label) {
    if (!QStringList{"folder", "browser", "terminal", "chat", "task", "activity", "keybindings",
                     "notes_start", "pause", "resume", "cancel", "research", "news", "theme"}
             .contains(code))
        return;
    append("hotbox_action", label, {}, code);
}
bool Demonstrations::addNote(const QString &text) {
    if (text.trimmed().isEmpty() || text.size() > 1000)
        return fail(tr("Add a note up to 1,000 characters."));
    return append("note", text);
}
QString Demonstrations::safeUrl(const QString &text) {
    QUrl url(text.trimmed(), QUrl::StrictMode);
    if (!url.isValid() || !QStringList{"http", "https"}.contains(url.scheme()) || url.host().isEmpty() ||
        !url.userInfo().isEmpty() || text.size() > 2048)
        return {};
    url.setQuery(QString());
    url.setFragment(QString());
    url.setUserInfo(QString());
    QStringList path;
    for (const auto &part : url.path().split('/', Qt::KeepEmptyParts)) {
        if (part.size() > 48 ||
            QRegularExpression("(?i)(token|password|secret|session|auth|apikey|api-key|cookie|oauth|"
                               "login|account|user|profile)")
                .match(part)
                .hasMatch() ||
            part.contains('@') || part.contains('%'))
            return url.adjusted(QUrl::RemovePath | QUrl::RemoveQuery | QUrl::RemoveFragment)
                       .toString(QUrl::FullyEncoded) +
                   "/";
        path << part;
    }
    url.setPath(path.join('/'));
    return SessionLedger::redact(url.toString(QUrl::FullyEncoded));
}
bool Demonstrations::addUrl(const QString &text) {
    const auto url = safeUrl(text);
    if (url.isEmpty())
        return fail(tr("Paste a web address without a username or password."));
    return append("navigate", tr("Open selected page"), url);
}
bool Demonstrations::importBrowser(const QUrl &fileUrl) {
    if (current()["state"] == "recording" || current()["state"] == "paused")
        return fail(tr("Finish the current recording before importing another."));
    if (!fileUrl.isLocalFile())
        return fail(tr("Choose a local browser recording."));
    QFile file(fileUrl.toLocalFile());
    QFileInfo info(file);
    if (info.isSymLink() || !info.isFile() || info.size() > 256000 || !file.open(QIODevice::ReadOnly))
        return fail(tr("Choose a browser recording smaller than 256 KB."));
    QJsonParseError error;
    const auto doc = QJsonDocument::fromJson(file.read(256001), &error);
    const auto source = doc.object();
    if (error.error != QJsonParseError::NoError || source["format"] != "hotbox-demonstration-v1" ||
        !source["steps"].isArray() || source["steps"].toArray().isEmpty() ||
        source["steps"].toArray().size() > 500)
        return fail(tr("This file is not a supported browser recording."));
    // Build a fresh whitelist-only copy. Unknown payload fields, values and credentials are never
    // retained.
    QVariantList safe;
    for (const auto &v : source["steps"].toArray()) {
        const auto s = v.toObject();
        const QString kind = s["kind"].toString();
        if (!QStringList{"navigate", "click", "input_field", "shortcut"}.contains(kind))
            return fail(tr("This recording contains an unsupported action."));
        const QString url = safeUrl(s["url"].toString());
        if (url.isEmpty())
            return fail(tr("This recording contains an unsupported web address."));
        const QString target = clean(s["target"].toString(), 100);
        if (kind == "shortcut" &&
            !QStringList{"Ctrl+L", "Ctrl+F", "Ctrl+R", "Ctrl+Enter"}.contains(target))
            continue;
        if (kind == "input_field" &&
            QRegularExpression("(?i)(password|secret|token|card|credit|otp|pin|cookie|auth)")
                .match(target)
                .hasMatch())
            continue;
        safe.append(
            QVariantMap{{"kind", kind},
                        {"url", url},
                        {"label", kind == "input_field" ? tr("Fill a field using a value supplied later")
                                  : kind == "shortcut"  ? tr("Use a browser shortcut")
                                  : kind == "click"     ? tr("Choose a page control")
                                                        : tr("Open selected page")},
                        {"target", target}});
    }
    if (safe.isEmpty())
        return fail(tr("There are no usable steps in this recording."));
    if (!m_db.transaction())
        return fail(tr("Recording storage is busy."));
    const QString previous = m_id;
    const auto previousDraft = m_draft;
    QSignalBlocker blocker(this);
    if (!start(tr("Browser demonstration"))) {
        m_db.rollback();
        blocker.unblock();
        emit changed();
        return false;
    }
    for (const auto &v : safe) {
        auto s = v.toMap();
        if (!append(s["kind"].toString(), s["label"].toString(), s["url"].toString(),
                    s["target"].toString())) {
            m_db.rollback();
            m_id = previous;
            m_draft = previousDraft;
            blocker.unblock();
            emit changed();
            return false;
        }
    }
    stop();
    if (!m_db.commit()) {
        m_db.rollback();
        m_id = previous;
        m_draft = previousDraft;
        blocker.unblock();
        return fail(tr("Could not save the browser recording."));
    }
    blocker.unblock();
    emit changed();
    return true;
}
bool Demonstrations::makeDraft(const QString &name) {
    const auto recording = current();
    const auto steps = recording["steps"].toList();
    if (recording["state"] != "stopped" || steps.isEmpty())
        return fail(tr("Finish a recording with at least one step first."));
    if (name.trimmed().isEmpty() || name.size() > 100)
        return fail(tr("Give the helper a short name."));
    QVariantList procedure;
    QStringList inputs;
    bool browser = false;
    for (const auto &v : steps) {
        auto s = v.toMap();
        const auto kind = s["kind"].toString();
        if (kind == "input_field") {
            const QString parameter = QString("value_%1").arg(inputs.size() + 1);
            inputs << parameter;
            s["instruction"] = tr("Fill %1 with a value supplied when the task runs.")
                                   .arg(s["target"].toString().isEmpty() ? tr("the selected field")
                                                                         : s["target"].toString());
        } else
            s["instruction"] =
                kind == "navigate"   ? tr("Open %1").arg(s["url"].toString())
                : kind == "click"    ? tr("Choose %1 on the recorded page.").arg(s["target"].toString())
                : kind == "shortcut" ? tr("Use %1 in the browser.").arg(s["target"].toString())
                                     : s["label"].toString();
        if (kind != "note" && kind != "hotbox_action")
            browser = true;
        // Source identifiers are evidence, never a replay target or current authority.
        procedure.append(QVariantMap{{"instruction", s["instruction"]},
                                     {"kind", kind},
                                     {"page", s["url"]},
                                     {"target", s["target"]},
                                     {"source_step", s["id"]}});
    }
    m_draft = {
        {"id", id("draft_")},
        {"name", clean(name, 100)},
        {"state", "draft"},
        {"enabled", false},
        {"source_trust", "untrusted_demonstration"},
        {"browser_session_ref", QVariant()},
        {"category", browser ? "Web" : "Personal workflow"},
        {"role", "specialist"},
        {"source", m_id},
        {"steps", procedure},
        {"parameters", inputs},
        {"session",
         browser ? tr("Sign in through your own browser when needed. Cookies stay in the browser.")
                 : QString()},
        {"review", tr("Review the steps, choose tools and permissions, supply input values, and test "
                      "before enabling. This draft does not replay actions or enable a schedule.")}};
    QSqlQuery q(m_db);
    q.prepare("INSERT INTO recorded_agent_drafts(id,demo,document) VALUES(?,?,?)");
    q.addBindValue(m_draft["id"]);
    q.addBindValue(m_id);
    q.addBindValue(QString::fromUtf8(
        QJsonDocument(QJsonObject::fromVariantMap(m_draft)).toJson(QJsonDocument::Compact)));
    if (!q.exec()) {
        m_draft.clear();
        return fail(tr("Could not save the helper draft."));
    }
    m_problem.clear();
    emit changed();
    return true;
}
