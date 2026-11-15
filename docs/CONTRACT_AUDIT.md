# Contract audit and normalization

Audit checkpoint: 2026-11-15. This pass covers the build ledger, its supervised
worker context path, and the boundaries with the Rust kernel, storage and planned
swarm. It is not a line-by-line certification of every design document or an
independent functionality acceptance receipt. Subsequent audits append evidence
and versioned contracts in the private build ledger.

## Authority and direct references

```text
Requirement / idea / addition                 private rolling ledger
                |
Immutable step acceptance contract            private SQL + version/digest
                |
Canonical interface definition                version-controlled source
                |
     +----------+------------------+
     |                             |
Runtime validation / GUI       Generated reference
     |                             |
     +---------- drift check ------+
                |
Behavior tests + observed command receipts
                |
Independent artifact acceptance               general bridge still planned
```

For the first normalized interface, [build-ledger-api-v1.json](../src/build-ledger-api-v1.json)
is the authority for operation fields, required fields, enums and read/mutation
classification. [The generated reference](../core/docs/BUILD_LEDGER_REFERENCE.md)
links to implementation symbols and executable checks. The
[behavior guide](../core/docs/BUILD_LEDGER_API.md) explains transaction/scoping,
conditional requirements and semantic limits that remain implemented in code.
Do not treat the field catalog as a complete formal specification of SQL effects.

`bin/build` checks source/reference consistency before compiling. `bin/test-cli`
also compares the compiled catalog using a temporary database and checks that
stale documents, invalid modes, missing source symbols and compiled drift fail.
These checks establish reference consistency; behavior tests remain necessary.

## Current capability map

Implemented means code exists in the named component. Partial means an implemented
slice exists but the larger product contract is incomplete. Planned means the
runtime connection is absent. Test references identify evidence to run, not a
claim that every referenced toolchain was available at this checkpoint.

| Capability | Status and owner | Direct implementation | Verification / remaining boundary |
| --- | --- | --- | --- |
| Local build API definitions | Implemented, shared source catalog | [catalog](../src/build-ledger-api-v1.json), `BuildLedger::catalog`, `BuildLedger::call`, `BuildLedger::isRead` in [buildledger.cpp](../src/buildledger.cpp); [Builds.qml](../src/Builds.qml) | [check-contracts](../bin/check-contracts), [contract_reference_smoke.py](../tests/contract_reference_smoke.py), [cli_smoke.py](../tests/cli_smoke.py) |
| Rolling plans, documents and forks | Implemented, Qt private SQL | [v1 schema](../src/build-ledger-v1.sql), [v2 contracts](../src/build-ledger-v2.sql), `BuildLedger::read` / `call` | `buildLedgerVersionsForksAndScopes`, `buildLedgerProtectsImmutableEvidenceAndObjects`, `rollingLedgerMigratesVersionOneAndProtectsContracts` in [Qt tests](../tests/hotbox_tests.cpp) |
| Bounded worker guidance | Implemented, Qt reference context | `BuildLedger::guide`, `Backend::contextTool` in [backend.cpp](../src/backend.cpp) | `rollingLedgerContractsGuideAndForkEvidence`; no dependency solver or approved dispatch decision |
| Normalized provider observations | Implemented Qt adapter, partial B1 gate | [provider contract](../src/provider-receipt-v1.json), [normalizer](../src/providerreceipts.cpp), `AgentService::recordReceipt` | [behavior and tests](../core/docs/PROVIDER_RECEIPTS.md); model/environment/lease/cost provenance remain unknown, no independent acceptance |
| Agent supervision and context pinning | Partial product path, Qt process supervisor | `AgentService::start` in [agentservice.cpp](../src/agentservice.cpp), Backend build reader/binder | `agentRunsBindToBuildStepsBeforeLaunch`, `headlessAgentsPersistOutputAndStatus`; fake provider only, private starter directories, no general source worktree dispatch |
| Recorded command invocation | Implemented, explicit CLI action | `run` branch and `buildContext` in [main.cpp](../src/main.cpp) | [CLI smoke](../tests/cli_smoke.py): success/failure links, issue creation, contract match; not an interactive PTY or independent verifier |
| Token accounting / loop inspection | Partial, Qt advisory loop | [master-loop-policy.json](../src/master-loop-policy.json), [loopoptimizer.cpp](../src/loopoptimizer.cpp), `AgentService::recordReceipt` | `masterLoopTracksReportedUsageAndParentBudgets`; reported usage limits later dispatch, not a provider hard cap; missing usage remains unknown |
| General leases and fixture acceptance | Implemented, Rust kernel | `Kernel::claim` / `finish` in [engine.rs](../core/src/engine.rs), [model.rs](../core/src/model.rs) | [kernel.rs](../core/tests/kernel.rs): duplicate claims, stale workers, fabricated evidence; fresh Rust verification unavailable here without Cargo |
| Fixed notes execution/recovery | Implemented, bounded Qt/Rust bridge | [taskservice.cpp](../src/taskservice.cpp), [tasks.rs](../core/src/tasks.rs) | `notesTasksRecoverAndFinishWithoutAWindow`, `taskScreenRunsFromFormAndSurvivesWindowHide`; deterministic recipe, not arbitrary coding |
| Source vs belongings separation | Implemented, Qt storage registry | [architecturestore.cpp](../src/architecturestore.cpp), [runtime.cpp](../src/runtime.cpp) | `architectureReusesLibrariesAndPreservesBelongings`, `architectureRejectsUnsafePathsAndScopesModelTools`; logical ownership, not an OS/container sandbox |
| Portable environment declarations | Partial, Rust validation/plans | [environment.rs](../core/src/environment.rs), [environment design](../core/docs/ENVIRONMENTS.md) | Declared environment plans do not prepare uv environments or launch sandboxes |
| General Qt-to-Rust agent dispatch | Planned | [B3 contract](../core/docs/BUILD_ROADMAP.md#b3-one-durable-control-owner) | Needs idempotent requests/receipts, leases/fences and replayable cross-database projection |
| Independent coding acceptance | Planned | [build roadmap](../core/docs/BUILD_ROADMAP.md), [kernel contracts](../core/docs/CONTRACTS.md) | Needs exact artifact/source identity and independent checks; zero exit and recorded work do not suffice |
| Swarm roles and master optimization | Planned | [role contracts](../core/docs/PLANNED_SWARM_AGENTS.md), [planner](../core/docs/ORCHESTRATION_LOOP_PLANNER.md) | Disabled definitions; no general autonomous swarm, model planner or automatic promotion |

## Findings and disposition

| Finding | Disposition |
| --- | --- |
| API guide's main operation table omitted newer guidance/contract operations; enums were described in multiple places | Resolved for this interface: generated operation/enum reference replaces the duplicate table |
| C++ accepted fields/enums, backend read classification and QML node kinds were maintained independently | Resolved: all consume the shared catalog; issue selection uses its kind name rather than an array position |
| README repeated an older 29-check count | Resolved: Master-loop section links to the central verification profile |
| Core documentation index implied all implemented capabilities live in the Rust crate | Resolved: index now identifies the owning component and distinguishes Qt evidence from Rust acceptance |
| Plans can be mistaken for live agent capabilities | Boundary retained and linked here: proposed swarm roles remain disabled; worker guidance is reference-only |
| Most planned roadmap steps lack their own versioned acceptance contract | Open: fill contracts one capability at a time, keeping exact implementation/test references |
| General leased dispatch and verifier bridge are absent | Open: preserve B3/B4 gates; do not normalize runtime code to unimplemented target prose |
| Other subsystem interfaces still require individual canonical-contract audits | Open: extend the pattern after auditing each interface; build API and supervised provider-receipt catalog are now normalized; other interfaces remain |
| Fresh Rust, TCP mocks and authenticated provider behavior lack current evidence | Open validation gates; offline Qt/helper checks do not substitute for them |

## Change workflow

1. Record the request as an idea/addition/issue under the relevant roadmap step.
2. Set acceptance criteria, implementation references and verification instructions
   in a versioned contract. Separate current behavior from the requested change.
3. Change the canonical interface and implementation together. For the build API,
   run `bin/check-contracts --write`; never edit the generated reference manually.
4. Run the scoped checks and save command receipts, source identity and new doc
   versions against the same step. If the contract changed, review older checks.
5. Fork alternatives with provenance; preserve prior proposals, failures and
   immutable versions. `recorded` is an outcome claim, not verified completion.
6. Require the existing controller/verifier authority for acceptance when the
   relevant runtime bridge exists. Until then, state that the evidence is unverified.

Run `bin/test --desktop-only --offline`, `bin/test-cli` and
`python tests/install_local_smoke.py` for this desktop slice. The offline profile
excludes three TCP mocks and reuses the existing Rust helper. Full Rust/provider
checks and the general build acceptance gate remain separate.
