#pragma once
#include "segmentstore.h"
#include "sessionledger.h"
#include <QFutureWatcher>
#include <QJsonArray>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QObject>
#include <QPointer>
#include <QProcess>
#include <QSqlDatabase>
#include <QTimer>
#include <QVariantList>
#include <memory>

class Backend : public QObject {
    Q_OBJECT
    Q_PROPERTY(QVariantList categories READ categories NOTIFY changed)
    Q_PROPERTY(QVariantList messages READ messages NOTIFY changed)
    Q_PROPERTY(QVariantList segments READ segments NOTIFY activityChanged)
    Q_PROPERTY(bool segmentAuto READ segmentAuto WRITE setSegmentAuto NOTIFY activityChanged)
    Q_PROPERTY(QVariantList timeline READ timeline NOTIFY activityChanged)
    Q_PROPERTY(QVariantMap gitState READ gitState NOTIFY activityChanged)
    Q_PROPERTY(bool taskBusy READ taskBusy NOTIFY activityChanged)
    Q_PROPERTY(QVariantList bindings READ bindings NOTIFY bindingsChanged)
    Q_PROPERTY(int currentId READ currentId NOTIFY changed)
    Q_PROPERTY(QString title READ title NOTIFY changed)
    Q_PROPERTY(QString folder READ folder NOTIFY changed)
    Q_PROPERTY(QString brief READ brief NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY statusChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(bool expanded READ expanded NOTIFY expandedChanged)
    Q_PROPERTY(QString endpoint READ endpoint NOTIFY settingsChanged)
    Q_PROPERTY(QString model READ model NOTIFY settingsChanged)
    Q_PROPERTY(QString keyVariable READ keyVariable NOTIFY settingsChanged)
  public:
    explicit Backend(const QString &directory = {}, QObject *parent = nullptr);
    ~Backend() override;
    QVariantList categories() const;
    QVariantList messages() const;
    QVariantList bindings() const { return m_bindings; }
    int currentId() const { return m_current; }
    QString title() const;
    QString folder() const;
    QString brief() const;
    QString status() const { return m_status; }
    bool busy() const { return m_reply; }
    bool expanded() const { return m_expanded; }
    QString endpoint() const { return setting("endpoint", "http://127.0.0.1:11434/v1/chat/completions"); }
    QString model() const { return setting("model", ""); }
    QString keyVariable() const { return setting("key_variable", "HOTBOX_API_KEY"); }
    Q_INVOKABLE void select(int id);
    Q_INVOKABLE int addCategory(const QString &name, const QString &folder, const QString &brief,
                                int parent = 0);
    Q_INVOKABLE void updateCategory(int id, const QString &name, const QString &folder, const QString &brief);
    Q_INVOKABLE void archive(int id);
    Q_INVOKABLE void restore(int id);
    Q_INVOKABLE void send(const QString &text);
    Q_INVOKABLE void cancel();
    Q_INVOKABLE void saveSettings(const QString &endpoint, const QString &model, const QString &keyVariable);
    Q_INVOKABLE void refreshBindings();
    Q_INVOKABLE void launch(const QString &action, int id = -1);
    Q_INVOKABLE void hold();
    Q_INVOKABLE void release();
    Q_INVOKABLE void preview();
    Q_INVOKABLE void openChat(int id = 1);
    Q_INVOKABLE QVariantList children(int parent) const;
    Q_INVOKABLE QVariantList archivedCategories() const;
    Q_INVOKABLE void clearStatus();
    void setStatus(const QString &text);
    QVariantList timeline() const;
    QVariantList segments() const;
    bool segmentAuto() const;
    Q_INVOKABLE void setSegmentAuto(bool enabled);
    Q_INVOKABLE QVariantList segmentPage(const QString &state = QString(""),
                                         const QString &query = QString(""), qint64 before = 0) const;
    Q_INVOKABLE QVariantMap segmentAction(const QString &action,
                                          const QVariantMap &arguments = QVariantMap());
    QVariantMap gitState() const;
    bool taskBusy() const { return m_gitInFlight; }
    QVariantMap lastGitResult() const { return m_lastGitResult; }
    Q_INVOKABLE QVariantList eventPage(int category, qint64 before = 0) const;
    Q_INVOKABLE QVariantMap readEvent(qint64 id, int offset = 0) const;
    Q_INVOKABLE qint64 recordEvent(int category, const QString &kind, const QString &summary,
                                   const QString &content, const QString &source = QString(""));
    Q_INVOKABLE bool attachSession(const QString &path);
    Q_INVOKABLE int focusWorkspace(int category, const QString &name, const QString &handoff);
    Q_INVOKABLE void captureGit();
    Q_INVOKABLE void forkGit();
    Q_INVOKABLE void configureGit(const QString &paths, const QString &remote, bool automatic, bool push);
    Q_INVOKABLE void checkpointGit();
    void processBackgroundGit();
    QJsonArray requestContext() const;
    QVariantMap contextTool(const QString &name, const QVariantMap &arguments, int scope = 0);

  signals:
    void changed();
    void activityChanged();
    void statusChanged();
    void busyChanged();
    void expandedChanged();
    void bindingsChanged();
    void settingsChanged();
    void showMenu();
    void showChat();
    void showBindings();
    void createRequested(int parent);

  private:
    QString field(int id, const QString &name) const;
    QString setting(const QString &name, const QString &fallback) const;
    bool execute(const QString &sql, const QVariantList &args = {}) const;
    int append(int category, const QString &role, const QString &content);
    void updateMessage(int id, const QString &content);
    void consumeStream(bool final = false);
    void startReply();
    void collectTools(const QJsonObject &delta);
    QJsonArray toolDefinitions() const;
    void gitTask(const QString &operation, int category);
    std::unique_ptr<SessionLedger> m_ledger;
    std::unique_ptr<SegmentStore> m_segments;
    QTimer m_segmentPoll;
    QFutureWatcher<QVariantMap> m_gitTask;
    QTimer m_background, m_activityPoll;
    qint64 m_dataVersion = -1;
    unsigned m_backgroundNext = 0;
    int m_taskCategory = 1;
    QString m_taskOperation;
    bool m_gitInFlight = false;
    QVariantMap m_lastGitResult;
    QJsonArray m_context;
    QMap<int, QJsonObject> m_toolCalls;
    int m_toolRounds = 0;
    bool m_cancelled = false;

    QSqlDatabase m_db;
    QString m_connection, m_directory, m_status;
    int m_current = 1, m_replyCategory = 1, m_replyMessage = 0;
    QTimer m_hold;
    bool m_held = false, m_expanded = false;
    QNetworkAccessManager m_network;
    QPointer<QNetworkReply> m_reply;
    QByteArray m_buffer;
    QString m_answer;
    bool m_sawDone = false;
    QVariantList m_bindings;
    QProcess m_bindingProcess;
};
