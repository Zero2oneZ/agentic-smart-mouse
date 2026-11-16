#pragma once
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>

// Read assignments as data, never as shell code. Explicit environment wins.
inline QByteArray modelCredential(const QString &directory, const QString &variable) {
    if (!QRegularExpression("^[A-Za-z_][A-Za-z0-9_]*$").match(variable).hasMatch())
        return {};
    const auto name = variable.toUtf8();
    if (qEnvironmentVariableIsSet(name.constData()))
        return qgetenv(name.constData());
    QFile file(directory + "/secrets.env");
    const QFileInfo info(file);
    if (!info.isFile() || info.isSymLink() || info.ownerId() != QFileInfo(directory).ownerId() ||
        (info.permissions() & (QFile::ReadGroup | QFile::WriteGroup | QFile::ExeGroup |
                               QFile::ReadOther | QFile::WriteOther | QFile::ExeOther)) ||
        info.size() > 65536 || !file.open(QIODevice::ReadOnly))
        return {};
    QByteArray result;
    for (auto line : file.readAll().split('\n')) {
        line = line.trimmed();
        if (line.startsWith("export ")) line = line.mid(7).trimmed();
        const int equals = line.indexOf('=');
        if (equals < 0 || line.left(equals).trimmed() != name) continue;
        auto value = line.mid(equals + 1).trimmed();
        if (value.startsWith('"') || value.startsWith('\'')) {
            if (value.size() < 2 || value.back() != value.front()) return {};
            value = value.mid(1, value.size() - 2);
        }
        if (value.contains('\r') || value.contains('\0') || value.contains('"') || value.contains('\'')) return {};
        result = value;
    }
    return result;
}
