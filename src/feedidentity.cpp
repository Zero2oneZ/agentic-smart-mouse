#include "backend.h"
#include <QCryptographicHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSqlQuery>

QVariantMap Backend::feedIdentity() const {
    QSqlQuery q(m_db);
    q.prepare("SELECT document FROM feed_origins WHERE category=?");
    q.addBindValue(m_current);
    if (q.exec() && q.next())
        return QJsonDocument::fromJson(q.value(0).toByteArray()).object().toVariantMap();
    return {{"label", title()},
            {"route", tr("Workspace chat")},
            {"color", "#9ac8fa"},
            {"mark", QString::number(m_current)},
            {"category", m_current}};
}
QVariantMap Backend::identifyFeed(int category, const QStringList &path, const QString &spread,
                                  const QString &action, bool remember) {
    if (field(category, "name").isEmpty() || path.size() > 32 ||
        !QStringList{"recent", "favorites", "pinned"}.contains(spread) || action.size() > 80)
        return {};
    QStringList safe;
    for (const auto &part : path) {
        auto text = SessionLedger::redact(part).simplified().left(100);
        if (!text.isEmpty())
            safe << text;
    }
    const QString label = field(category, "name");
    QJsonObject seed{
        {"category", category}, {"path", safe.join(" / ")}, {"spread", spread}, {"action", action}};
    const auto hash = QCryptographicHash::hash(QJsonDocument(seed).toJson(QJsonDocument::Compact),
                                               QCryptographicHash::Sha256)
                          .toHex();
    const QStringList colors{"#9ac8fa", "#a6d49c", "#d7b1f1", "#f0c18f", "#8fd1cd"};
    QVariantMap result{{"category", category},
                       {"label", label},
                       {"route", safe.join(" › ")},
                       {"spread", spread},
                       {"action", action},
                       {"mark", QString::fromLatin1(hash.left(6)).toUpper()},
                       {"color", colors[hash[0] % colors.size()]}};
    if (remember) {
        QSqlQuery q(m_db);
        q.prepare("INSERT INTO feed_origins(category,document) VALUES(?,?) ON CONFLICT(category) DO UPDATE "
                  "SET document=excluded.document");
        q.addBindValue(category);
        q.addBindValue(QString::fromUtf8(
            QJsonDocument(QJsonObject::fromVariantMap(result)).toJson(QJsonDocument::Compact)));
        if (!q.exec())
            return {};
        emit changed();
    }
    return result;
}
