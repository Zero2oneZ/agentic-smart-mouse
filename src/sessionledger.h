#pragma once
#include <QSqlDatabase>
#include <QVariantList>
#include <QVariantMap>

class SessionLedger {
  public:
    explicit SessionLedger(const QSqlDatabase &db);
    qint64 record(int category, const QString &kind, const QString &summary, const QString &content,
                  const QString &source = {});
    QVariantList timeline(int category, qint64 before = 0, int limit = 40) const;
    QVariantMap read(int category, qint64 event, int offset = 0) const;
    bool contains(int scope, int category) const;
    void grant(int category, qint64 event);
    static QString redact(QString text);
    static bool privatePath(const QString &path);

  private:
    QSqlDatabase m_db;
    QString m_session;
};
