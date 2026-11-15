# Ollama review team: connection pending

The user requested a primary-assistant coordinator with several Ollama cloud
reviewers. The [review batch definition](../templates/review-batches/ollama-review-v1.json)
prepares three bounded roles: contracts/code, data/runtime, and tests/failures.
It remains disabled until a connection and model assignments are supplied. No
Ollama worker has been launched and the general B6 swarm gate remains unchanged.

```text
User objective + pinned repository context
                    |
            Primary assistant
                    |
       +------------+-------------+
       |            |             |
   Contracts    Data/runtime   Tests/failures
   reviewer      reviewer        reviewer
       |            |             |
       +------------+-------------+
                    |
       Findings with file references
                    |
       Primary assistant reconciles
                    |
        Bounded changes + real checks
                    |
       Ledger evidence + next decision
```

Each reviewer gets explicit source files at a recorded revision and makes one
proposal-only pass. Raw chats, personal belongings, credentials and runtime logs
are not part of the default context. Model responses cannot execute tools or
approve their own suggestions. Review text is not independent artifact acceptance.

The definition is a review handoff, not an executable scheduler. The existing
Rust controller/lease/verification contracts remain the execution authority.
There are no recursive calls or automatic retries, and unknown usage requires
review before further spending. The initial team has at most three reviewers;
additional roles should follow measured usefulness rather than model count.

## Prepare bounded packets

`bin/prepare-review --output-dir /absolute/private/review-directory` creates three
text packets and an integrity manifest. It reads only the definition's explicit
repository references from the current **Git HEAD**, not uncommitted working
files. Definition and source/excerpt hashes are recorded separately. Each source
excerpt is capped at 2,400 characters and each packet at 24,000 characters, with
clipping disclosed. These are character bounds, not token measurements.

Output must be a new directory outside the source repository, with owner-only
permissions. Existing output is never overwritten. Preparation makes no model or
network calls, and does not execute source or stored scripts. Full files can be
retrieved by exact revision/reference if a reviewer needs more context later.

## Connection choices

Direct cloud access can use `https://ollama.com/api` or its compatible `/v1`
interface with a bearer API key; it does not require an installed local Ollama
server. An existing local/server proxy is another option. See the official
[authentication reference](https://github.com/ollama/ollama/blob/main/docs/api/authentication.mdx)
and [cloud overview](https://github.com/ollama/ollama/blob/main/docs/cloud.mdx).
Do not assume cloud structured-output support; the current cloud overview states
that it is unsupported. Findings must be parsed/reviewed as untrusted text rather
than claiming provider-enforced schemas.

This workspace currently has no Ollama CLI, saved Hotbox model endpoint or exposed
`OLLAMA_API_KEY`. The coding shell also has network restrictions. Required next
inputs are the direct-cloud/proxy route, model identifiers and the key environment
variable name. Actual secrets stay outside source and SQL. No API key should be
pasted into chat or committed.

Once connected, run one small source-review packet first, verify response/usage
capture and scope, then run the other independent packets. Preserve each model,
request/context digest, response, usage status and finding provenance in the ledger.
Do not call the batch a verified autonomous coding swarm until the general runner
and independent acceptance bridge exist.
