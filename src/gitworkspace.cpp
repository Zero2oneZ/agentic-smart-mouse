#include "gitworkspace.h"
#include "sessionledger.h"
#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLockFile>
#include <QProcess>
#include <QProcessEnvironment>
#include <QRegularExpression>
#include <QStandardPaths>

namespace {
struct Result {
    bool ok;
    QString text;
};
Result git(const QString &cwd, const QStringList &args, int timeout = 8000) {
    QProcess p;
    auto env = QProcessEnvironment::systemEnvironment();
    for (const auto &key : QStringList{"GIT_DIR", "GIT_WORK_TREE", "GIT_INDEX_FILE", "GIT_COMMON_DIR",
                                       "GIT_OBJECT_DIRECTORY", "GIT_ALTERNATE_OBJECT_DIRECTORIES"})
        env.remove(key);
    env.insert("GIT_TERMINAL_PROMPT", "0");
    if (!env.contains("GIT_SSH_COMMAND"))
        env.insert("GIT_SSH_COMMAND", "ssh -oBatchMode=yes");
    p.setProcessEnvironment(env);
    p.start("git", QStringList{"--literal-pathspecs", "-c", "core.hooksPath=/dev/null", "-c",
                               "core.fsmonitor=false", "-c", "credential.interactive=false", "-C", cwd} +
                       args);
    if (!p.waitForFinished(timeout)) {
        p.kill();
        p.waitForFinished(1000);
        return {false, "Git operation timed out"};
    }
    QByteArray out = p.readAllStandardOutput();
    if (p.exitCode() != 0)
        out += p.readAllStandardError();
    return {p.exitStatus() == QProcess::NormalExit && p.exitCode() == 0,
            SessionLedger::redact(QString::fromUtf8(out.left(262144))).trimmed()};
}
QVariantMap error(const QString &message) { return {{"ok", false}, {"error", message}}; }
} // namespace
QVariantMap GitWorkspace::snapshot(const QString &folder) {
    auto root = git(folder, {"rev-parse", "--show-toplevel"});
    if (!root.ok)
        return error(root.text);
    auto branch = git(root.text, {"symbolic-ref", "--short", "HEAD"});
    auto head = git(root.text, {"rev-parse", "HEAD"});
    auto status = git(root.text, {"status", "--short", "--untracked-files=normal"});
    auto tree = git(root.text, {"ls-tree", "-r", "--name-only", "HEAD"});
    auto history = git(root.text, {"log", "-15", "--date=iso-strict", "--format=%h %aI %s"});
    if (!head.ok || !status.ok || !tree.ok || !history.ok)
        return error("Could not read Git status, tree, or history");
    return {{"ok", true},
            {"root", root.text},
            {"branch", branch.text},
            {"head", head.text},
            {"status", status.text},
            {"tree", tree.text.left(24000)},
            {"history", history.text},
            {"treeTruncated", tree.text.size() > 24000}};
}
QVariantMap GitWorkspace::fork(const QString &folder, const QString &destination, const QString &branch) {
    if (!QRegularExpression("^hotbox/[a-zA-Z0-9_-]+$").match(branch).hasMatch())
        return error("Invalid managed branch name");
    auto root = git(folder, {"rev-parse", "--show-toplevel"});
    if (!root.ok)
        return error(root.text);
    if (QFileInfo::exists(destination))
        return error("Worktree destination already exists");
    QDir().mkpath(QFileInfo(destination).absolutePath());
    auto result = git(root.text, {"worktree", "add", "-b", branch, destination, "HEAD"}, 15000);
    if (!result.ok)
        return error(result.text);
    return {{"ok", true},
            {"root", QFileInfo(destination).canonicalFilePath()},
            {"branch", branch},
            {"source", root.text}};
}
QVariantMap GitWorkspace::checkpoint(const QString &folder, const QString &branch, const QStringList &paths,
                                     const QString &remote, bool push) {
    if (!branch.startsWith("hotbox/"))
        return error("Only managed Hotbox branches can be checkpointed");
    QString identity = QFileInfo(folder).canonicalFilePath();
    QString lockRoot = QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    QLockFile lock(
        lockRoot + "/hotbox-git-" +
        QString::fromLatin1(QCryptographicHash::hash(identity.toUtf8(), QCryptographicHash::Sha256).toHex()) +
        ".lock");
    lock.setStaleLockTime(0);
    if (!lock.tryLock())
        return error("Another checkpoint is already running for this worktree");
    auto current = git(folder, {"symbolic-ref", "--short", "HEAD"});
    if (!current.ok || current.text != branch)
        return error("Worktree branch changed; background Git paused");
    auto staged = git(folder, {"diff", "--cached", "--name-only"});
    if (!staged.ok || !staged.text.isEmpty())
        return error("Existing staged changes require manual review");
    QStringList clean;
    QDir root(folder);
    for (const auto &path : paths) {
        QString p = path.trimmed();
        if (p.isEmpty())
            continue;
        if (QDir::isAbsolutePath(p) || QDir::cleanPath(p) != p || p == "." || p == ".." ||
            p.startsWith("../") || SessionLedger::privatePath(p) ||
            QRegularExpression("(?i)\\.(log|jsonl|sqlite3|db)(-|$)").match(p).hasMatch())
            return error("Unsafe checkpoint path: " + p);
        QFileInfo info(root.filePath(p));
        if (info.isSymLink() || info.isDir())
            return error("Select individual regular files, not directories or symlinks");
        QString ancestor = QFileInfo(info.absolutePath()).canonicalFilePath();
        QString canonicalRoot = QFileInfo(folder).canonicalFilePath();
        if (ancestor != canonicalRoot && !ancestor.startsWith(canonicalRoot + "/"))
            return error("Checkpoint path escapes its worktree");
        if (info.exists()) {
            if (info.size() > 2 * 1024 * 1024)
                return error("Checkpoint file exceeds 2 MiB: " + p);
            QFile file(info.filePath());
            if (!file.open(QIODevice::ReadOnly))
                return error("Cannot inspect checkpoint file");
            QByteArray data = file.readAll();
            QString text = QString::fromUtf8(data);
            if (SessionLedger::redact(text) != text)
                return error("Potential credential in checkpoint file: " + p);
        } else if (!git(folder, {"ls-files", "--error-unmatch", "--", p}).ok)
            return error("Checkpoint path does not exist: " + p);
        clean << p;
    }
    clean.removeDuplicates();
    if (clean.size() > 200)
        return error("Choose at most 200 exact files per checkpoint");
    if (clean.isEmpty())
        return error("Choose exact files for background checkpointing first");
    auto add = git(folder, QStringList{"add", "--"} + clean);
    if (!add.ok)
        return error(add.text);
    auto diff = git(folder, {"diff", "--cached", "--quiet"});
    bool committed = false;
    if (!diff.ok) {
        auto commit = git(folder, {"commit", "-m", "Checkpoint Hotbox workspace"});
        if (!commit.ok)
            return error(commit.text);
        committed = true;
    }
    if (push) {
        if (!QRegularExpression("^[A-Za-z0-9][A-Za-z0-9_.-]*$").match(remote).hasMatch())
            return error("Select an existing named Git remote");
        auto url = git(folder, {"remote", "get-url", "--push", remote});
        if (!url.ok)
            return error(url.text);
        auto sent = git(folder, {"push", "--porcelain", remote, "HEAD:refs/heads/" + branch}, 20000);
        if (!sent.ok)
            return error(sent.text);
    }
    auto result = snapshot(folder);
    if (!result.value("ok").toBool())
        return result;
    result["committed"] = committed;
    result["pushed"] = push;
    result["paths"] = clean;
    return result;
}
