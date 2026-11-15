#include "backend.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSqlQuery>

QVariantMap Backend::optimizerReport(int scope) const {
    if ((scope == 0 ? m_current : scope) != 1)
        return {{"error", tr("Open Master chat to inspect the global loop.")}};
    QFile policyFile(":/master-loop-policy.json");
    if(!policyFile.open(QIODevice::ReadOnly))return {{"error",tr("Master-loop policy is unavailable.")}};
    auto policy = QJsonDocument::fromJson(policyFile.readAll()).object();
    QSqlQuery q(m_db);
    q.exec(
        "SELECT a.state,a.prompt,m.root_run,m.usage_known,m.input_tokens,m.output_tokens,m.policy_revision,r.run "
        "FROM agent_runs a LEFT JOIN loop_metrics m ON m.run=a.id LEFT JOIN provider_receipts r ON r.run=a.id ORDER BY a.rowid DESC LIMIT 1000");
    int runs = 0, unknown = 0, attention = 0, active = 0;
    qint64 input = 0, output = 0;
    QSet<QString> roots, prompts;
    int duplicates = 0;
    while (q.next()) {
        ++runs;
        if (q.value(0) == "attention")
            ++attention;
        if (q.value(0) == "running")
            ++active;
        if (!q.value(2).toString().isEmpty())
            roots.insert(q.value(2).toString());
        if (q.value(3).toBool()&&!q.value(7).isNull()) {
            input += q.value(4).toLongLong();
            output += q.value(5).toLongLong();
        } else
            ++unknown;
        const auto text = q.value(1).toString();
        if (prompts.contains(text))
            ++duplicates;
        prompts.insert(text);
    }
    QSqlQuery context(m_db);
    context.exec("SELECT COUNT(*),COALESCE(SUM(length(content)),0) FROM events WHERE kind='context.loaded'");
    context.next();
    QVariantList suggestions;
    auto suggest = [&](const QString &code, const QString &text) {
        suggestions.append(QVariantMap{{"action", code}, {"summary", text}, {"state", "proposed"}});
    };
    if (!runs)
        suggest("collect_baseline",
                tr("Complete a representative task and verification before changing the master tree."));
    if (unknown)
        suggest(
            "complete_usage_evidence",
            tr("%1 runs have unknown token usage. Connect provider receipts before judging token savings.")
                .arg(unknown));
    if (attention)
        suggest("inspect_failures",
                tr("Review %1 runs needing attention before spending more on retries.").arg(attention));
    if (duplicates)
        suggest("review_repeated_requests", tr("%1 repeated prompts may indicate retries or reusable work. "
                                               "Check inputs and freshness before reuse.")
                                                .arg(duplicates));
    if (context.value(1).toLongLong() > 64000)
        suggest("evaluate_context_selection",
                tr("Compare scoped summary/reference selection against a fixed task set; preserve raw "
                   "evidence and verification quality."));
    auto roads=m_build->call(1,"list",{{"schema_version",1}}).value("roadmaps").toList();
    return {{"rollingLedger", QVariantMap{{"mode","reference_only"},{"roadmaps",roads.mid(0,12)},{"visibleCount",roads.size()},{"nextRead","read_build_guidance"},{"dispatchAuthority","existing_rust_controller"}}},
            {"policy", policy.toVariantMap()},
            {"mode", "advisory_only"},
            {"coverage", "latest_1000_agent_runs"},
            {"runs", runs},
            {"parentRuns", roots.size()},
            {"active", active},
            {"attention", attention},
            {"usageUnknown", unknown}, {"usageEvidence","normalized_receipts_only"},
            {"reportedInputTokens", input},
            {"reportedOutputTokens", output},
            {"contextSnapshots", context.value(0)},
            {"contextCharacters", context.value(1)},
            {"proposals", suggestions},
            {"tokenBudgetEnforcement", "reported_usage_before_next_dispatch_only"},
            {"note", tr("Characters are not tokens. Process exit is not verified task completion. Proposals "
                        "never change the active policy or source tree.")}};
}
