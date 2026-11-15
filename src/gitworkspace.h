#pragma once
#include <QStringList>
#include <QVariantMap>
class GitWorkspace {
  public:
    static QVariantMap snapshot(const QString &folder);
    static QVariantMap fork(const QString &folder, const QString &destination, const QString &branch);
    static QVariantMap checkpoint(const QString &folder, const QString &branch, const QStringList &paths,
                                  const QString &remote, bool push);
};
