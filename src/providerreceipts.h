#pragma once
#include <QByteArray>
#include <QVariantMap>

// Provider observations only. Receipts never grant leases or accept artifacts.
class ProviderReceipts {
  public:
    static QVariantMap catalog();
    static QString revision();
    static QVariantMap normalize(const QString &provider, const QByteArray &source, bool complete);
};
