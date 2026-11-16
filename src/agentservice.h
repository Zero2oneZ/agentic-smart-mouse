#pragma once
#include <QObject>
#include <QProcess>
#include <QSqlDatabase>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>
#include <functional>

class AgentService : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList jobs READ jobs NOTIFY changed)
    Q_PROPERTY(QVariantMap selected READ selected NOTIFY changed)
    Q_PROPERTY(QString problem READ problem NOTIFY changed)
  public:
    AgentService(const QSqlDatabase &db, const QString &directory, bool recover, QObject *parent = nullptr);
    ~AgentService() override;
    void setBuildBinder(
        std::function<bool(const QString &, const QString &, const QString &, int, qint64)> binder) {
        m_buildBinder = std::move(binder);
    }
    void setBuildContext(std::function<QVariantMap(const QString &, const QString &, int)> reader) {
        m_buildContext = std::move(reader);
    }
    QVariantList jobs() const;
    QVariantMap selected() const;
    QVariantMap receipt(const QString &id) const;
    QString problem() const { return m_problem; }
    Q_INVOKABLE QString start(const QString &prompt, const QVariantMap &target, const QString &provider,
                              const QString &previous = QString());
    Q_INVOKABLE bool select(const QString &id);
    Q_INVOKABLE void stop(const QString &id);
    Q_INVOKABLE void dismiss(const QString &id);
    Q_INVOKABLE bool terminal(const QString &id);
  signals:
    void capturedEvent(int category, const QString &kind, const QString &summary, const QString &content,
                       const QString &source);
    void changed();

  private:
    QVariantMap read(const QString &id) const;
    void publish();
    void finish(const QString &id, const QString &state, const QString &detail);
    void recordReceipt(const QString &id, const QString &outcome, const QVariant &exitCode = QVariant());
    QString m_directory, m_selected, m_problem;
    bool m_receiptsReady = false;
    QSqlDatabase m_db;
    std::function<bool(const QString &, const QString &, const QString &, int, qint64)> m_buildBinder;
    std::function<QVariantMap(const QString &, const QString &, int)> m_buildContext;
    QTimer m_publishTimer;
    QMap<QString, QProcess *> m_processes;
    QMap<QString, QString> m_outcomes;
};
