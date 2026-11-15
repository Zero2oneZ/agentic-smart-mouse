#pragma once
#include "sessionledger.h"
#include <QSqlDatabase>
#include <QVariantMap>

// Describes and provisions storage. It does not execute applications or containers.
class ArchitectureStore {
  public:
    ArchitectureStore(const QSqlDatabase &db, SessionLedger &ledger, const QString &directory);
    QVariantMap catalog(int scope) const;
    QVariantMap initialize(int scope, const QString &toolRoot, const QString &runtimeRoot);
    QVariantMap defineCapability(int scope, const QString &id, const QString &relativePath,
                                 const QString &description);
    QVariantMap plan(int scope, int category, const QVariantMap &arguments) const;
    QVariantMap provision(int scope, int category, const QVariantMap &arguments);

  private:
    QString setting(const QString &key) const;
    QSqlDatabase m_db;
    SessionLedger &m_ledger;
    QString m_directory;
};
