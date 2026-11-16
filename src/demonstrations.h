#pragma once
#include <QObject>
#include <QSqlDatabase>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>

// Explicit, bounded demonstrations. No global keyboard hooks, browser DB reads or replay.
class Demonstrations : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantMap current READ current NOTIFY changed)
    Q_PROPERTY(QVariantList history READ history NOTIFY changed)
    Q_PROPERTY(QVariantMap draft READ draft NOTIFY changed)
    Q_PROPERTY(QString problem READ problem NOTIFY changed)
  public:
    explicit Demonstrations(const QSqlDatabase &db, bool recover = true, QObject *parent = nullptr);
    QVariantMap current() const;
    QVariantList history() const;
    QVariantMap draft() const { return m_draft; }
    QString problem() const { return m_problem; }
    Q_INVOKABLE bool start(const QString &title);
    Q_INVOKABLE void togglePause();
    Q_INVOKABLE void stop();
    Q_INVOKABLE void select(const QString &id);
    Q_INVOKABLE bool addNote(const QString &text);
    Q_INVOKABLE bool addUrl(const QString &text);
    Q_INVOKABLE bool importBrowser(const QUrl &file);
    Q_INVOKABLE bool makeDraft(const QString &name);
    void action(const QString &code, const QString &label);
    static QString safeUrl(const QString &text);
  signals:
    void changed();

  private:
    bool append(const QString &kind, const QString &label, const QString &url = {},
                const QString &target = {});
    bool fail(const QString &message);
    QSqlDatabase m_db;
    QString m_id, m_problem;
    QVariantMap m_draft;
};
