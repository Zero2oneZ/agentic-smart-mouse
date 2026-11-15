# Supervised provider receipts: implemented adapter, partial delivery gate

The Qt process supervisor now writes immutable version-1 receipts for local
Codex/Claude attempts. This normalizes observed process/usage evidence. It does
not connect general Rust leases or certify coding artifacts, provider billing,
authenticated model/version identity or prepared environments.

The [source contract](../../src/provider-receipt-v1.json) drives structured CLI
flags, terminal record recognition, cache accounting and parser bounds. The
[generated reference](PROVIDER_RECEIPTS_REFERENCE.md) lists fields and source/test
links. `bin/check-provider-contracts --write` regenerates it;
`bin/check-provider-contracts --binary build/hotbox` detects source/reference and
compiled-catalog drift. Both build and CLI validation include this gate.

```text
Pinned request / loop policy / ledger context / receipt adapter
                            |
                  Supervised CLI attempt
                     /             \
             stdout JSONL        stderr diagnostics
                  |                    |
          provider.jsonl            run.log
                  +--------+-----------+
                           |
               One normalized observation
                           |
         immutable receipt + loop counters [SQL transaction]
                           |
          scoped reads / advisory loop / feed notification
                           :
                 general Rust acceptance bridge [planned]
```

## Capture and normalization

Codex receives `--json`; Claude receives `--output-format stream-json --verbose`.
Existing permissions, private starter directories, time limits and parent policy
pins stay in force. Structured modes are documented by
[Codex](https://learn.chatgpt.com/docs/non-interactive-mode) and
[Claude Code](https://code.claude.com/docs/en/headless), and were checked against
local CLI help. No authenticated provider run was made to validate this adapter.

Only captured stdout is parsed for usage. Stderr cannot impersonate a terminal
usage record. Both streams appear in the bounded private `run.log`; stdout also
appears in private `provider.jsonl`. A capture write failure or limit kills the
process and prevents complete usage classification. Final unread pipe data is
drained before making the receipt. Process/stdio capture is not a sandbox.

This adapter accepts one terminal usage record per invocation. Exact repetitions
are deduplicated only when the provider supplies the same explicit terminal ID
and identical contents. Multiple records without that identity, differing bodies
or differing IDs are ambiguous and remain unknown; they are never blindly summed.
Malformed stdout, oversized lines, fractional/negative/oversized accounted counters and
missing required usage fields remain unknown. Nonterminal valid JSON events are
ignored for accounting. Terminal records nested in model text are not parsed.

Codex input includes its cached-input subset, so cached tokens are not added a
second time. Claude's input total adds its separate noncached input, cache-read
and cache-creation buckets. Missing Claude cache buckets remain unknown, rather
than being treated as zero. See the provider's
[cache accounting definition](https://platform.claude.com/docs/en/build-with-claude/prompt-caching).
Reasoning/output subsets are not added to output again. Client cost estimates are
not converted into authoritative billing; normalized cost stays null.

## Durable state and recovery

`provider_receipts` lives in private `hotbox.sqlite3`, keyed uniquely by run ID.
Its JSON body, digest and timestamp reject SQL updates/deletes through immutability
triggers. The first accepted receipt and compatibility loop counters commit in
one transaction; repeated finalization cannot charge the same run again. The
supervisor refuses a new launch when the receipt schema cannot be initialized.
This protects application invariants, not against arbitrary privileged DB edits.

Receipts pin the parser contract digest and carry the existing run family,
parent/policy refs, prompt digest and bounded ledger context references. Missing
model, executable version, environment, lease and cost identity stays null.
`receipt_digest` is read-response metadata hashing the stored JSON body; it is not
a self-referential field in that body.

Cancellation, timeout, capture failure/limits and interruption record their actual
outcome. If a terminal usage observation exists but capture was interrupted, it
is partial and cannot become an accounted complete total. Restart synthesizes an
interrupted receipt for unfinished runs without relaunching a provider. Legacy
runs with no matching pinned adapter stay unknown; old loop counters are preserved
in SQL but are not promoted to normalized usage evidence.

The receipt is authoritative for this observation. Feed notification happens after
commit and can be absent during constructor recovery; scoped receipt lookup still
works. A durable cross-database outbox remains part of B3, not this adapter.

## Read and loop behavior

```sh
hotbox provider-schema --data-dir /path/to/private/profile
hotbox agent-receipt --category ID --id RUN_ID
```

The receipt command and read-only chat tool `read_agent_receipt` enforce the
existing workspace ancestry scope. Agents UI shows receipt outcome and usage
status. No receipt/log body grants execution authority.

New roots pin `master-loop-v2`. Follow-ups retain their family's original policy
and budget; they do not receive a fresh budget. A family with unknown/partial or
legacy unnormalized usage cannot automatically follow up. A missing pinned parent
policy also fails closed. The user must review evidence before explicitly starting
new approved work. Known normalized input/output totals still enforce the existing
limits before later dispatch, not as a provider hard cap during a run.

The optimizer counts normalized known receipts only. Missing receipts and legacy
counters remain unknown. `succeeded` means the observed process/provider reported
success; every receipt still states `verification: unverified`.

## Verification and remaining gates

Temporary tests cover Codex cache subsets, Claude cache buckets, duplicate and
conflicting terminal records, malformed/oversized data, partial capture, stdout
vs stderr, failed attempts with known consumption, scope denial, immutable SQL,
restart, cancellation, timeout, output limits and unknown-family follow-up denial.
The existing parent-budget and headless UI tests remain part of regression checks.

B0 still needs Cargo/Rust and a host allowing TCP mocks. B2 needs usable uv tooling
and reproducible environment preparation. B1 still needs authenticated provider
fixtures/version provenance and the larger receipt/runner integration. B3-B7 keep
their existing dependencies; this adapter does not enable an autonomous swarm.
