#pragma once
#include "sessionledger.h"
#include <QSqlDatabase>
#include <QVariantMap>

class SegmentStore {
  public:
    SegmentStore(const QSqlDatabase &db, SessionLedger &ledger);
    QVariantMap create(int scope, int category, const QString &kind, const QString &title,
                       const QString &description);
    QVariantMap add(int scope, const QString &id, const QString &kind, const QString &content,
                    const QString &source = {}, qint64 event = 0);
    QVariantMap read(int scope, const QString &id) const;
    QVariantMap artifact(int scope, const QString &id, int offset = 0) const;
    QVariantList list(int scope, const QString &state = {}, const QString &query = {}, qint64 before = 0,
                      int limit = 40) const;
    QVariantMap ready(int scope, const QString &id);
    QVariantMap claim(int scope, const QString &worker, int seconds = 300);
    QVariantMap renew(int scope, const QString &id, const QString &token, int seconds = 300);
    QVariantMap finish(int scope, const QString &id, const QString &token, const QString &description,
                       const QString &summary, const QString &result);
    QVariantMap fail(int scope, const QString &id, const QString &token, const QString &error);
    QVariantMap retry(int scope, const QString &id);
    QVariantMap link(int scope, const QString &from, const QString &to, const QString &relation);
    QVariantMap compose(int scope, int category, const QString &title, const QStringList &ids);
    QVariantMap process(int scope);
    void grant(int category, const QString &id);

  private:
    bool visible(int scope, const QString &id) const;
    bool lease(int scope, const QString &id, const QString &token) const;
    QVariantMap insertArtifact(const QString &id, const QString &kind, const QString &content,
                               const QString &source, qint64 event = 0);
    QSqlDatabase m_db;
    SessionLedger &m_ledger;
};
