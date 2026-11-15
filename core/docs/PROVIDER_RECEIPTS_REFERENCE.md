# Provider receipts reference (generated)

Generated from [provider-receipt-v1.json](../../src/provider-receipt-v1.json) with `bin/check-provider-contracts --write`. See the [behavior and delivery boundary](PROVIDER_RECEIPTS.md).

Contract: `hotbox.provider-receipt.v1`. SHA-256: `fbaecb4df742889b02b49e42f30a3205ba3ea161fea7d2a13e18c31833ee00f7`.

| Provider | Structured flags | Terminal type | Input accounting | Cache fields |
| --- | --- | --- | --- | --- |
| claude | `--output-format stream-json --verbose` | `result` | `separate_cache_buckets` | `cache_read_input_tokens`, `cache_creation_input_tokens` |
| codex | `--json` | `turn.completed` | `includes_cache` | `cached_input_tokens` |

Usage states: `known`, `unknown`, `partial`.

Outcomes: `succeeded`, `failed`, `cancelled`, `timed_out`, `output_limit`, `capture_error`, `interrupted`, `not_started`.

Limits: source_bytes = 8388608, line_bytes = 1048576, counter = 1000000000000.

| Receipt field | Meaning |
| --- | --- |
| `schema_version` | integer, exactly 1 |
| `contract_id` | versioned normalized receipt identity |
| `run` | local supervised attempt ID; immutable unique key |
| `provider` | codex or claude |
| `adapter_revision` | SHA-256 of the adapter contract used for normalization |
| `pinned_adapter_revision` | adapter revision captured before launch; null for legacy runs |
| `root_run` | existing parent-budget family |
| `parent_run` | previous local attempt or null |
| `policy_revision` | existing pinned loop policy digest |
| `build_context` | roadmap/node/revision/digest references or null |
| `prompt_digest` | SHA-256 of the persisted redacted prompt/context input |
| `outcome` | process/provider observation; never functionality acceptance |
| `exit_code` | observed exit code or null |
| `usage` | status, reason, provider buckets and normalized input/output totals; missing fields remain null |
| `source_digest` | SHA-256 of captured stdout bytes, including when partial |
| `source_bytes` | captured stdout size |
| `model` | unknown/null until supplied by a verified adapter |
| `provider_version` | unknown/null until supplied by a verified adapter |
| `environment` | unknown/null until an environment registry is connected |
| `lease` | unknown/null until the Rust leased runner is connected |
| `cost` | unknown/null; no price or billing inference |
| `created` | UTC receipt creation timestamp |
| `verification` | always unverified |

## Implementation

- [src/providerreceipts.cpp](../../src/providerreceipts.cpp): `ProviderReceipts::normalize`.
- [src/agentservice.cpp](../../src/agentservice.cpp): `AgentService::recordReceipt`.

## Verification

- [tests/hotbox_tests.cpp](../../tests/hotbox_tests.cpp): `providerReceiptsNormalizeWithoutDoubleCounting`.
- [tests/hotbox_tests.cpp](../../tests/hotbox_tests.cpp): `providerReceiptsPersistAndBlockUnknownFollowups`.

## Provider references

- [Provider source 1](https://learn.chatgpt.com/docs/non-interactive-mode).
- [Provider source 2](https://code.claude.com/docs/en/headless).
- [Provider source 3](https://platform.claude.com/docs/en/build-with-claude/prompt-caching).
