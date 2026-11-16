#include "taskservice.h"
#include <QCoreApplication>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcessEnvironment>
#include <QUrl>
#include <QUuid>

TaskService::TaskService(const QString &directory, bool automatic, QObject *parent)
    : QObject(parent), m_directory(directory), m_automatic(automatic) {
    findHelper();
    QProcessEnvironment env;
    env.insert("LANG", "C.UTF-8");
    m_process.setProcessEnvironment(env);
    m_timeout.setSingleShot(true);
    m_timeout.setInterval(8000);
    connect(&m_timeout, &QTimer::timeout, this, [this] {
        m_process.kill();
        fail(tr("This step took too long. Your saved work is kept. Try checking setup again."));
    });
    connect(&m_process, &QProcess::readyReadStandardOutput, this, [this] {
        m_output += m_process.readAllStandardOutput();
        if (m_output.size() > 512000) {
            m_process.kill();
            fail(tr("The task service returned too much data. Your saved work is kept."));
        }
    });
    connect(&m_process, &QProcess::readyReadStandardError, this,
            [this] { m_process.readAllStandardError(); });
    connect(&m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) {
            m_timeout.stop();
            fail(tr("Background tasks could not start. Check setup or update Hotbox."));
        }
    });
    connect(&m_process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished), this,
            [this](int code, QProcess::ExitStatus) { finish(code); });
    m_timer.setInterval(700);
    connect(&m_timer, &QTimer::timeout, this, [this] {
        if (!busy())
            request("notes_tick");
    });
    if (m_automatic)
        QTimer::singleShot(0, this, &TaskService::refresh);
}
TaskService::~TaskService() {
    m_timer.stop();
    if (busy()) {
        m_process.kill();
        m_process.waitForFinished(1000);
    }
}
void TaskService::findHelper() {
    QStringList candidates;
    const QString override = qEnvironmentVariable("HOTBOX_CORE_BINARY");
    if (!override.isEmpty())
        candidates << override;
    else {
        const QString app = QCoreApplication::applicationDirPath();
        candidates << app + "/hotbox-core";
    }
    m_helper.clear();
    for (const auto &candidate : candidates) {
        QFileInfo file(candidate);
        if (file.isFile() && file.isExecutable() && !file.isSymLink()) {
            m_helper = file.canonicalFilePath();
            break;
        }
    }
    if (m_helper.isEmpty())
        m_problem =
            tr("Background tasks need the Hotbox task service. Update Hotbox, then check setup again.");
}
void TaskService::fail(const QString &text) {
    m_problem = text;
    m_timer.stop();
    m_again = false;
    emit changed();
}
void TaskService::request(const QString &operation, QVariantMap arguments) {
    if (!available()) {
        emit changed();
        return;
    }
    if (busy()) {
        m_again = true;
        return;
    }
    m_operation = operation;
    arguments["command"] = operation;
    if (operation != "notes_start")
        arguments["actor"] = "local-user";
    m_output.clear();
    m_problem.clear();
    m_timeout.start();
    m_process.start(m_helper, {m_directory + "/orchestration.sqlite3"});
    m_process.write(
        QJsonDocument(QJsonObject::fromVariantMap(arguments)).toJson(QJsonDocument::Compact));
    m_process.closeWriteChannel();
    emit changed();
}
void TaskService::refresh() { request("notes_list", {{"resource", ""}}); }
void TaskService::retry() {
    if (busy())
        return;
    findHelper();
    refresh();
}
void TaskService::start(const QString &title, const QString &notes, int category) {
    if (busy())
        return;
    if (notes.toUtf8().size() > 65536) {
        fail(tr("These notes are too long. Try a smaller set."));
        return;
    }
    const QString id = "wrun_" + QUuid::createUuid().toString(QUuid::Id128);
    QVariantMap value{{"id", id},
                      {"actor", "local-user"},
                      {"resource", "workspace-" + QString::number(category)},
                      {"title", title.trimmed()},
                      {"notes", notes}};
    request("notes_start", {{"request", value}});
    emit demonstrated("notes_start", tr("Requested: organize pasted notes"));
}
void TaskService::select(const QString &id) {
    if (!busy())
        request("notes_read", {{"id", id}});
}
void TaskService::control(const QString &action) {
    if (!busy() && !m_selected.isEmpty() && QStringList{"pause", "resume", "cancel"}.contains(action)) {
        request("notes_control", {{"id", m_selected["id"]}, {"action", action}});
        emit demonstrated(action, tr("Requested: %1 local task").arg(action));
    }
}
void TaskService::finish(int code) {
    m_timeout.stop();
    m_output += m_process.readAllStandardOutput();
    if (!m_problem.isEmpty()) {
        emit changed();
        return;
    }
    QJsonParseError error;
    const auto doc = QJsonDocument::fromJson(m_output, &error);
    const auto envelope = doc.object();
    if (error.error != QJsonParseError::NoError || code != 0 || !envelope["ok"].toBool()) {
        if (envelope["error"].toString().contains("Add a title"))
            fail(tr("Use a title up to 100 characters and between 1 and 500 notes."));
        else
            fail(tr("The task service could not complete this step. Your saved work is kept. Check "
                    "setup or update Hotbox."));
        return;
    }
    const QVariant result = envelope["result"].toVariant();
    if (m_operation == "notes_list") {
        m_jobs = result.toList();
        bool active = false;
        for (const auto &item : m_jobs) {
            auto job = item.toMap();
            if (job["status"] == "queued" || job["status"] == "working")
                active = true;
            if (job["id"] == m_selected["id"]) {
                if (job["status"] == m_selected["status"])
                    job["preview"] = m_selected["preview"];
                m_selected = job;
            }
        }
        if (active && m_automatic)
            m_timer.start();
        else
            m_timer.stop();
    } else if (!result.isNull()) {
        const auto job = result.toMap();
        if (m_operation != "notes_tick" || m_selected.isEmpty() || m_selected["id"] == job["id"])
            m_selected = job;
        m_again = true;
    } else if (m_operation == "notes_tick") {
        m_timer.stop();
        m_again = true;
    }
    emit changed();
    if (m_again) {
        m_again = false;
        QTimer::singleShot(0, this, &TaskService::refresh);
    }
}
void TaskService::openResult(bool folder) {
    if (m_selected["status"] != "complete")
        return;
    const QString path = m_selected["result_path"].toString();
    const QString root = QFileInfo(m_directory + "/task-results").canonicalFilePath();
    QFileInfo file(path);
    if (root.isEmpty() || !file.isFile() || file.isSymLink() ||
        !file.canonicalFilePath().startsWith(root + "/")) {
        fail(tr("The saved result is unavailable. Your task history is kept."));
        return;
    }
    if (!QDesktopServices::openUrl(
            QUrl::fromLocalFile(folder ? file.absolutePath() : file.absoluteFilePath())))
        fail(tr("Could not open the result. You can find its location in Details."));
}
