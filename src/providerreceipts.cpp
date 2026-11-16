#include "providerreceipts.h"
#include <QCryptographicHash>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <cmath>

namespace {
QByteArray definition() {
    QFile file(":/provider-receipt-v1.json");
    if (!file.open(QIODevice::ReadOnly))
        return {};
    return file.readAll();
}
} // namespace
QVariantMap ProviderReceipts::catalog() {
    static const auto value = QJsonDocument::fromJson(definition()).object().toVariantMap();
    return value;
}
QString ProviderReceipts::revision() {
    static const auto value =
        QString::fromLatin1(QCryptographicHash::hash(definition(), QCryptographicHash::Sha256).toHex());
    return value;
}
QVariantMap ProviderReceipts::normalize(const QString &provider, const QByteArray &source, bool complete) {
    const auto contract = catalog(), profile = contract.value("providers").toMap().value(provider).toMap(),
               limits = contract.value("limits").toMap();
    QVariantMap result{{"status", "unknown"},
                       {"reason", "no_terminal_usage"},
                       {"input_tokens", QVariant()},
                       {"output_tokens", QVariant()},
                       {"provider_input_tokens", QVariant()},
                       {"cached_input_tokens", QVariant()},
                       {"cache_read_input_tokens", QVariant()},
                       {"cache_creation_input_tokens", QVariant()},
                       {"provider_failed", false},
                       {"terminal_records", 0},
                       {"duplicate_records", 0}};
    auto unknown = [&](const QString &reason) {
        result["status"] = "unknown";
        result["reason"] = reason;
        return result;
    };
    if (profile.isEmpty())
        return unknown("unsupported_provider");
    if (source.size() > limits.value("source_bytes").toLongLong())
        return unknown("source_limit");
    QJsonObject terminal;
    QByteArray fingerprint;
    QString identity;
    int records = 0, duplicates = 0;
    for (const auto &line : source.split('\n')) {
        if (line.size() > limits.value("line_bytes").toLongLong())
            return unknown("line_limit");
        if (line.trimmed().isEmpty())
            continue;
        QJsonParseError parse;
        auto parsed = QJsonDocument::fromJson(line, &parse);
        if (parse.error != QJsonParseError::NoError || !parsed.isObject())
            return unknown("malformed_provider_stdout");
        auto event = parsed.object();
        if (!profile.value("failure_type").toString().isEmpty() &&
            event.value("type").toString() == profile.value("failure_type").toString())
            result["provider_failed"] = true;
        if (event.value("type").toString() != profile.value("terminal_type").toString())
            continue;
        const auto bytes = QJsonDocument(event).toJson(QJsonDocument::Compact);
        const auto id = event.value(profile.value("identity_field").toString()).toString();
        if (records) {
            if (!id.isEmpty() && id == identity && bytes == fingerprint) {
                ++duplicates;
                continue;
            }
            result["terminal_records"] = records + 1;
            return unknown("ambiguous_terminal_records");
        }
        ++records;
        terminal = event;
        fingerprint = bytes;
        identity = id;
    }
    result["terminal_records"] = records;
    result["duplicate_records"] = duplicates;
    if (!records)
        return unknown(complete ? "no_terminal_usage" : "incomplete_capture");
    if (provider == "claude") {
        if (terminal.contains("is_error") && !terminal.value("is_error").isBool())
            return unknown("invalid_error_flag");
        result["provider_failed"] = terminal.value("is_error").toBool();
    }
    auto usage = terminal.value("usage").toObject();
    auto valid = [&](const QString &key) {
        auto v = usage.value(key);
        return v.isDouble() && std::isfinite(v.toDouble()) && v.toDouble() >= 0 &&
               v.toDouble() <= limits.value("counter").toDouble() && std::floor(v.toDouble()) == v.toDouble();
    };
    if (!valid("input_tokens") || !valid("output_tokens"))
        return unknown("missing_or_invalid_counters");
    qint64 input = usage.value("input_tokens").toInteger();
    for (auto field : profile.value("cache_fields").toList()) {
        const auto key = field.toString();
        if (!usage.contains(key) && profile.value("input_mode") == "includes_cache")
            continue;
        if (!valid(key))
            return unknown("missing_or_invalid_cache_counters");
        result[key] = usage.value(key).toInteger();
        if (profile.value("input_mode") == "separate_cache_buckets")
            input += usage.value(key).toInteger();
        else if (usage.value(key).toInteger() > input)
            return unknown("cache_exceeds_input");
    }
    if (input > limits.value("counter").toLongLong())
        return unknown("counter_limit");
    result["provider_input_tokens"] = usage.value("input_tokens").toInteger();
    result["input_tokens"] = input;
    result["output_tokens"] = usage.value("output_tokens").toInteger();
    result["status"] = complete ? "known" : "partial";
    result["reason"] = complete ? "single_terminal_usage" : "incomplete_capture";
    return result;
}
