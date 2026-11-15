#pragma once
#include "sessionledger.h"
#include <QSqlDatabase>
#include <QVariantMap>

// Planning/evidence ledger. Rust retains authority over actual execution/acceptance.
class BuildLedger {
  public:
    BuildLedger(const QSqlDatabase &db, SessionLedger &events, const QString &directory);
    QVariantMap call(int scope, const QString &operation, const QVariantMap &arguments);

  private:
    QVariantMap read(int scope, const QString &id) const;
    QSqlDatabase m_db;
    SessionLedger &m_events;
    QString m_directory, m_problem;
    bool sql(const QString &statement, const QVariantList &values = {}) const;
};
