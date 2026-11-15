#pragma once
#include <QObject>
#include <QProcess>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>

// Fixed local recipe bridge. No shell, model tools, arbitrary executors or host mounts.
class TaskService : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList jobs READ jobs NOTIFY changed)
    Q_PROPERTY(QVariantMap selected READ selected NOTIFY changed)
    Q_PROPERTY(bool available READ available NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(QString problem READ problem NOTIFY changed)
  public:
    explicit TaskService(const QString &directory, bool automatic = true, QObject *parent = nullptr);
    ~TaskService() override;
    QVariantList jobs() const { return m_jobs; }
    QVariantMap selected() const { return m_selected; }
    bool available() const { return !m_helper.isEmpty(); }
    bool busy() const { return m_process.state() != QProcess::NotRunning; }
    QString problem() const { return m_problem; }
    Q_INVOKABLE void refresh();
    Q_INVOKABLE void start(const QString &title, const QString &notes, int category);
    Q_INVOKABLE void select(const QString &id);
    Q_INVOKABLE void control(const QString &action);
    Q_INVOKABLE void openResult(bool folder = false);
    Q_INVOKABLE void retry();
  signals:
    void changed();

  private:
    void findHelper();
    void request(const QString &operation, QVariantMap arguments = {});
    void finish(int code);
    void fail(const QString &text);
    QString m_directory, m_helper, m_operation, m_problem;
    QVariantList m_jobs;
    QVariantMap m_selected;
    QProcess m_process;
    QTimer m_timer, m_timeout;
    QByteArray m_output;
    bool m_again = false, m_automatic = true;
};
