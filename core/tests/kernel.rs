use hotbox_core::*;
use std::{
    collections::{BTreeMap, BTreeSet},
    fs,
    path::PathBuf,
    sync::{
        Arc,
        atomic::{AtomicI64, AtomicU64, Ordering},
    },
};

struct ManualClock(AtomicI64);
impl Clock for ManualClock {
    fn now(&self) -> i64 {
        self.0.load(Ordering::SeqCst)
    }
}
impl ManualClock {
    fn advance(&self, seconds: i64) {
        self.0.fetch_add(seconds, Ordering::SeqCst);
    }
}
struct Temp(PathBuf);
impl Temp {
    fn new() -> Self {
        static NEXT: AtomicU64 = AtomicU64::new(0);
        let path = std::env::temp_dir().join(format!(
            "hotbox-core-tests-{}-{}",
            std::process::id(),
            NEXT.fetch_add(1, Ordering::SeqCst)
        ));
        fs::create_dir(&path).unwrap();
        #[cfg(unix)]
        {
            use std::os::unix::fs::PermissionsExt;
            fs::set_permissions(&path, fs::Permissions::from_mode(0o700)).unwrap();
        }
        Self(path)
    }
}
impl Drop for Temp {
    fn drop(&mut self) {
        let _ = fs::remove_dir_all(&self.0);
    }
}
fn actions() -> BTreeSet<String> {
    ACTIONS.into_iter().map(String::from).collect()
}
fn template() -> Template {
    serde_json::from_str(include_str!("../templates/segmented-build-v1.json")).unwrap()
}
fn grant(
    id: &str,
    issuer: &str,
    grantee: &str,
    parent: Option<&str>,
    allowed: BTreeSet<String>,
) -> GrantRequest {
    GrantRequest {
        id: id.into(),
        issuer: issuer.into(),
        grantee: grantee.into(),
        resource: "project".into(),
        parent: parent.map(String::from),
        actions: allowed,
        may_delegate: true,
        expires_at: 1_800_010_000,
    }
}
struct Fixture {
    engine: Engine,
    clock: Arc<ManualClock>,
    root: Temp,
}
impl Fixture {
    fn new() -> Self {
        let root = Temp::new();
        let clock = Arc::new(ManualClock(AtomicI64::new(1_800_000_000)));
        let mut engine =
            Engine::open_with_clock(root.0.join("kernel.sqlite3"), clock.clone()).unwrap();
        engine.create_resource("alice", "project").unwrap();
        engine
            .grant(grant("root", "alice", "alice", None, actions()))
            .unwrap();
        Self {
            engine,
            clock,
            root,
        }
    }
    fn start(&mut self, t: Template) -> RunView {
        self.engine
            .start(StartRequest {
                id: "run".into(),
                actor: "alice".into(),
                resource: "project".into(),
                grant_id: "root".into(),
                template: t,
                inputs: BTreeMap::from([("goal".into(), "fixture only".into())]),
            })
            .unwrap()
    }
    fn claim(&mut self) -> Claim {
        match self.engine.claim("alice", "run", "worker").unwrap() {
            ClaimResult::Claimed { claim } => *claim,
            other => panic!("expected claim, got {other:?}"),
        }
    }
    fn finish(&mut self, c: &Claim) -> RunView {
        self.engine
            .finish(
                "alice",
                "run",
                &c.segment.id,
                &c.worker,
                &c.token,
                fixture_report(c).unwrap(),
            )
            .unwrap()
    }
}

#[test]
fn mandatory_stages_cycles_unknown_dependencies_and_version_are_rejected() {
    template().validate().unwrap();
    let mut t = template();
    t.segments.pop();
    assert!(t.validate().is_err());
    let mut t = template();
    t.segments[1].depends_on.clear();
    assert!(t.validate().is_err());
    let mut t = template();
    t.segments[0].depends_on.insert("missing".into());
    assert!(t.validate().is_err());
    let mut t = template();
    t.segments[0].depends_on.insert("document".into());
    assert!(t.validate().is_err());
    let mut t = template();
    t.segments[0].depends_on.insert("discover".into());
    assert!(t.validate().is_err());
    let mut t = template();
    t.version = "01.0.0".into();
    assert!(t.validate().is_err());
    let mut t = template();
    t.segments[0].required_actions.clear();
    assert!(t.validate().is_err());
}
#[test]
fn every_previous_stage_segment_is_a_required_barrier() {
    let mut t = template();
    let mut extra = t.segments[4].clone();
    extra.id = "build-second".into();
    t.segments.push(extra);
    assert!(t.validate().is_err());
    t.segments[5].depends_on.insert("build-second".into());
    t.validate().unwrap();
}
#[test]
fn unknown_executors_and_unknown_template_fields_fail_closed() {
    let raw = include_str!("../templates/segmented-build-v1.json");
    assert!(serde_json::from_str::<Template>(&raw.replace("fixture_ok", "shell")).is_err());
    let mut value: serde_json::Value = serde_json::from_str(raw).unwrap();
    value["skip_permissions"] = true.into();
    assert!(serde_json::from_value::<Template>(value).is_err());
}
#[test]
fn full_fixture_pipeline_completes_with_pinned_context_and_no_lease_in_views() {
    let mut f = Fixture::new();
    let initial = f.start(template());
    assert_eq!(initial.template_hash, template().hash().unwrap());
    for stage in Stage::ALL {
        let c = f.claim();
        assert_eq!(c.segment.stage, stage);
        assert_eq!(c.inputs.len(), 1);
        assert!(c.dependencies.len() <= 1);
        f.finish(&c);
    }
    let view = f.engine.status("alice", "run").unwrap();
    assert_eq!(view.status, RunStatus::Complete);
    assert_eq!(view.operations, 8);
    assert_eq!(view.reserved_cost_units, 8);
    assert!(
        view.segments
            .values()
            .all(|s| s.status == SegmentStatus::Complete && s.outputs.len() == 1)
    );
    let serial = serde_json::to_string(&view).unwrap();
    assert!(!serial.contains("token"));
    let events = f.engine.events("alice", "run", 0).unwrap();
    assert_eq!(
        events
            .iter()
            .filter(|e| e["kind"] == "segment.verified")
            .count(),
        8
    );
    assert!(!serde_json::to_string(&events).unwrap().contains("token"));
}
#[test]
fn duplicate_start_is_idempotent_and_cannot_rewrite_the_template() {
    let mut f = Fixture::new();
    f.start(template());
    let c = f.claim();
    f.finish(&c);
    let view = f.start(template());
    assert_eq!(view.operations, 1);
    let mut changed = template();
    changed.version = "1.0.1".into();
    let result = f.engine.start(StartRequest {
        id: "run".into(),
        actor: "alice".into(),
        resource: "project".into(),
        grant_id: "root".into(),
        template: changed,
        inputs: BTreeMap::new(),
    });
    assert!(matches!(result, Err(Error::Conflict(_))));
    assert_eq!(
        f.engine.status("alice", "run").unwrap().template_version,
        "1.0.0"
    );
}
#[test]
fn checkpoints_survive_reopening_without_repeating_completed_steps() {
    let mut f = Fixture::new();
    f.start(template());
    let c = f.claim();
    f.finish(&c);
    let mut other =
        Engine::open_with_clock(f.root.0.join("kernel.sqlite3"), f.clock.clone()).unwrap();
    match other.claim("alice", "run", "second").unwrap() {
        ClaimResult::Claimed { claim } => assert_eq!(claim.segment.stage, Stage::Document),
        x => panic!("{x:?}"),
    }
}
#[test]
fn concurrent_connections_do_not_double_claim_and_stale_workers_cannot_finish() {
    let mut f = Fixture::new();
    f.start(template());
    let first = f.claim();
    let mut other =
        Engine::open_with_clock(f.root.0.join("kernel.sqlite3"), f.clock.clone()).unwrap();
    assert!(matches!(
        other.claim("alice", "run", "second").unwrap(),
        ClaimResult::Idle {
            run_status: RunStatus::Running
        }
    ));
    f.clock.advance(30);
    let second = match other.claim("alice", "run", "second").unwrap() {
        ClaimResult::Claimed { claim } => claim,
        x => panic!("{x:?}"),
    };
    assert_eq!(second.attempt, 2);
    assert_ne!(first.token, second.token);
    assert!(
        f.engine
            .finish(
                "alice",
                "run",
                &first.segment.id,
                &first.worker,
                &first.token,
                fixture_report(&first).unwrap()
            )
            .is_err()
    );
    other
        .finish(
            "alice",
            "run",
            &second.segment.id,
            &second.worker,
            &second.token,
            fixture_report(&second).unwrap(),
        )
        .unwrap();
    assert_eq!(f.engine.status("alice", "run").unwrap().operations, 2);
}
#[test]
fn exhausted_expired_leases_fail_instead_of_looping_forever() {
    let mut f = Fixture::new();
    f.start(template());
    f.claim();
    f.clock.advance(30);
    f.claim();
    f.clock.advance(30);
    assert!(matches!(
        f.engine.claim("alice", "run", "worker").unwrap(),
        ClaimResult::Idle {
            run_status: RunStatus::Failed
        }
    ));
    assert_eq!(f.engine.status("alice", "run").unwrap().operations, 2);
}
#[test]
fn fixture_failure_retries_only_up_to_the_segment_limit() {
    let mut f = Fixture::new();
    let mut t = template();
    t.segments[0].executor = Executor::FixtureFail;
    f.start(t);
    let first = f.claim();
    assert_eq!(f.finish(&first).status, RunStatus::Running);
    let second = f.claim();
    assert_eq!(f.finish(&second).status, RunStatus::Failed);
    assert!(matches!(
        f.engine.claim("alice", "run", "worker").unwrap(),
        ClaimResult::Idle {
            run_status: RunStatus::Failed
        }
    ));
}
#[test]
fn operation_and_cost_budgets_stop_work_before_another_claim() {
    for cost_limit in [false, true] {
        let mut f = Fixture::new();
        let mut t = template();
        if cost_limit {
            t.limits.max_cost_units = 1;
        } else {
            t.limits.max_operations = 1;
        }
        f.start(t);
        let c = f.claim();
        f.finish(&c);
        assert!(matches!(
            f.engine.claim("alice", "run", "worker").unwrap(),
            ClaimResult::Idle {
                run_status: RunStatus::BudgetExhausted
            }
        ));
        assert_eq!(f.engine.status("alice", "run").unwrap().operations, 1);
    }
}
#[test]
fn global_deadline_and_cancellation_invalidate_outstanding_work() {
    let mut f = Fixture::new();
    f.start(template());
    let c = f.claim();
    f.clock.advance(300);
    assert!(
        f.engine
            .finish(
                "alice",
                "run",
                &c.segment.id,
                &c.worker,
                &c.token,
                fixture_report(&c).unwrap()
            )
            .is_err()
    );
    assert!(matches!(
        f.engine.claim("alice", "run", "worker").unwrap(),
        ClaimResult::Idle {
            run_status: RunStatus::Expired
        }
    ));
    let mut f = Fixture::new();
    f.start(template());
    let c = f.claim();
    assert_eq!(
        f.engine.cancel("alice", "run").unwrap().status,
        RunStatus::Cancelled
    );
    assert!(
        f.engine
            .finish(
                "alice",
                "run",
                &c.segment.id,
                &c.worker,
                &c.token,
                fixture_report(&c).unwrap()
            )
            .is_err()
    );
}
#[test]
fn fabricated_output_evidence_is_rejected_without_completing_the_step() {
    let mut f = Fixture::new();
    f.start(template());
    let c = f.claim();
    let forged = WorkerReport::Success {
        outputs: BTreeMap::from([("evidence".into(), "forged".into())]),
    };
    assert!(
        f.engine
            .finish("alice", "run", &c.segment.id, &c.worker, &c.token, forged)
            .is_err()
    );
    assert_eq!(
        f.engine.status("alice", "run").unwrap().segments["discover"].status,
        SegmentStatus::Running
    );
    f.finish(&c);
}
#[test]
fn delegation_can_only_narrow_actions_expiry_and_resource() {
    let mut f = Fixture::new();
    let mut root = grant(
        "alice-bob",
        "alice",
        "bob",
        None,
        BTreeSet::from(["source.read".into()]),
    );
    root.expires_at = 1_800_000_100;
    f.engine.grant(root).unwrap();
    let mut child = grant("bob-carol", "bob", "carol", Some("alice-bob"), actions());
    child.expires_at = 1_800_000_090;
    assert!(f.engine.grant(child.clone()).is_err());
    child.actions = BTreeSet::from(["source.read".into()]);
    child.expires_at = 1_800_000_101;
    assert!(f.engine.grant(child.clone()).is_err());
    child.expires_at = 1_800_000_090;
    child.resource = "another".into();
    assert!(f.engine.grant(child.clone()).is_err());
    child.resource = "project".into();
    f.engine.grant(child).unwrap();
    f.engine
        .check_access(
            "carol",
            "project",
            "bob-carol",
            &BTreeSet::from(["source.read".into()]),
        )
        .unwrap();
    assert!(
        f.engine
            .check_access("carol", "project", "bob-carol", &actions())
            .is_err()
    );
}
#[test]
fn delegation_requires_explicit_authority_and_cannot_mint_root_grants() {
    let mut f = Fixture::new();
    let mut parent = grant("limited", "alice", "bob", None, actions());
    parent.may_delegate = false;
    f.engine.grant(parent).unwrap();
    assert!(
        f.engine
            .grant(grant("child", "bob", "carol", Some("limited"), actions()))
            .is_err()
    );
    assert!(
        f.engine
            .grant(grant("fake-root", "bob", "carol", None, actions()))
            .is_err()
    );
    assert!(f.engine.revoke("carol", "limited").is_err());
    assert!(f.engine.create_resource("bob", "project").is_err());
}
#[test]
fn ancestor_revocation_is_transitive_and_independent_grants_survive() {
    let mut f = Fixture::new();
    f.engine
        .grant(grant("alice-bob", "alice", "bob", None, actions()))
        .unwrap();
    f.engine
        .grant(grant(
            "bob-carol",
            "bob",
            "carol",
            Some("alice-bob"),
            actions(),
        ))
        .unwrap();
    f.engine
        .grant(grant("direct-carol", "alice", "carol", None, actions()))
        .unwrap();
    f.engine
        .check_access("carol", "project", "bob-carol", &actions())
        .unwrap();
    f.engine.revoke("alice", "alice-bob").unwrap();
    assert!(
        f.engine
            .check_access("carol", "project", "bob-carol", &actions())
            .is_err()
    );
    f.engine
        .check_access("carol", "project", "direct-carol", &actions())
        .unwrap();
    assert!(
        f.engine
            .grant(grant(
                "late-child",
                "carol",
                "dave",
                Some("bob-carol"),
                actions()
            ))
            .is_err()
    );
}
#[test]
fn revocation_blocks_active_completion_and_future_claims_but_allows_cancellation() {
    let mut f = Fixture::new();
    f.start(template());
    let c = f.claim();
    f.engine.revoke("alice", "root").unwrap();
    assert!(
        f.engine
            .finish(
                "alice",
                "run",
                &c.segment.id,
                &c.worker,
                &c.token,
                fixture_report(&c).unwrap()
            )
            .is_err()
    );
    assert!(f.engine.claim("alice", "run", "worker").is_err());
    assert!(f.engine.status("alice", "run").is_err());
    assert_eq!(
        f.engine.cancel("alice", "run").unwrap().status,
        RunStatus::Cancelled
    );
}
#[test]
fn grants_expire_and_other_actors_cannot_read_or_change_runs() {
    let mut f = Fixture::new();
    f.start(template());
    assert!(f.engine.status("bob", "run").is_err());
    assert!(f.engine.events("bob", "run", 0).is_err());
    assert!(f.engine.cancel("bob", "run").is_err());
    assert!(f.engine.claim("bob", "run", "worker").is_err());
    f.clock.advance(10_000);
    assert!(
        f.engine
            .check_access("alice", "project", "root", &actions())
            .is_err()
    );
}
#[test]
fn delegation_depth_is_bounded() {
    let mut f = Fixture::new();
    let mut parent = "root".to_string();
    let mut issuer = "alice".to_string();
    for n in 1..32 {
        let id = format!("grant-{n}");
        let grantee = format!("person-{n}");
        f.engine
            .grant(grant(&id, &issuer, &grantee, Some(&parent), actions()))
            .unwrap();
        parent = id;
        issuer = grantee;
    }
    assert!(
        f.engine
            .grant(grant("too-deep", &issuer, "last", Some(&parent), actions()))
            .is_err()
    );
}
#[test]
fn unrelated_databases_are_not_migrated() {
    let root = Temp::new();
    let path = root.0.join("unrelated.sqlite3");
    let db = rusqlite::Connection::open(&path).unwrap();
    db.execute_batch("CREATE TABLE belongings(name TEXT);")
        .unwrap();
    drop(db);
    assert!(Engine::open(&path).is_err());
    let db = rusqlite::Connection::open(&path).unwrap();
    assert_eq!(
        db.query_row("PRAGMA application_id", [], |r| r.get::<_, i64>(0))
            .unwrap(),
        0
    );
    assert_eq!(
        db.query_row(
            "SELECT count(*) FROM sqlite_master WHERE type='table'",
            [],
            |r| r.get::<_, i64>(0)
        )
        .unwrap(),
        1
    );
}
#[cfg(unix)]
#[test]
fn symlinked_database_locations_are_rejected() {
    let root = Temp::new();
    let target = root.0.join("target");
    fs::create_dir(&target).unwrap();
    std::os::unix::fs::symlink(&target, root.0.join("link")).unwrap();
    assert!(Engine::open(root.0.join("link/kernel.sqlite3")).is_err());
    assert!(!target.join("kernel.sqlite3").exists());
}

fn tool_version() -> ToolVersion {
    ToolVersion {
        version: "1.0.0".into(),
        source_commit: "a".repeat(40),
        architecture_version: "architecture-v1".into(),
        originating_chat: "chat-original".into(),
    }
}
fn register(f: &mut Fixture) {
    f.engine
        .register_tool(RegisterTool {
            actor: "alice".into(),
            tool_id: "player".into(),
            resource: "project".into(),
            initial: tool_version(),
        })
        .unwrap();
}
#[test]
fn tool_versions_are_immutable_and_context_survives_restart() {
    let mut f = Fixture::new();
    register(&mut f);
    f.engine
        .add_tool_version("alice", "player", tool_version())
        .unwrap();
    let mut changed = tool_version();
    changed.source_commit = "b".repeat(40);
    assert!(matches!(
        f.engine
            .add_tool_version("alice", "player", changed.clone()),
        Err(Error::Conflict(_))
    ));
    changed.version = "2.0.0".into();
    f.engine
        .add_tool_version("alice", "player", changed.clone())
        .unwrap();
    let reopened =
        Engine::open_with_clock(f.root.0.join("kernel.sqlite3"), f.clock.clone()).unwrap();
    assert_eq!(
        reopened
            .tool_context("alice", "root", "player", "1.0.0")
            .unwrap()
            .selected,
        tool_version()
    );
    assert_eq!(
        reopened
            .tool_context("alice", "root", "player", "2.0.0")
            .unwrap()
            .selected,
        changed
    );
}
#[test]
fn forks_have_independent_identity_versions_and_access() {
    let mut f = Fixture::new();
    register(&mut f);
    f.engine.create_resource("bob", "fork-project").unwrap();
    f.engine
        .grant(grant(
            "read",
            "alice",
            "bob",
            Some("root"),
            ["source.read".into()].into(),
        ))
        .unwrap();
    let request = ForkTool {
        actor: "bob".into(),
        grant_id: "read".into(),
        source: ToolOrigin {
            tool_id: "player".into(),
            version: "1.0.0".into(),
        },
        tool_id: "fork".into(),
        resource: "fork-project".into(),
        originating_chat: "chat-fork".into(),
    };
    let mut same_chat = request.clone();
    same_chat.originating_chat = "chat-original".into();
    assert!(matches!(
        f.engine.fork_tool(same_chat),
        Err(Error::Invalid(_))
    ));
    let context = f.engine.fork_tool(request.clone()).unwrap();
    assert_eq!(context.forked_from, Some(request.source.clone()));
    assert_eq!(context.selected.originating_chat, "chat-fork");
    assert_eq!(context.selected.source_commit, tool_version().source_commit);
    // Original grants cannot read the fork; the fork owner needs an independent grant.
    assert!(
        f.engine
            .tool_context("bob", "read", "fork", "1.0.0")
            .is_err()
    );
    let mut fork_grant = grant("fork-root", "bob", "bob", None, actions());
    fork_grant.resource = "fork-project".into();
    f.engine.grant(fork_grant).unwrap();
    let mut changed = context.selected.clone();
    changed.version = "2.0.0".into();
    changed.source_commit = "b".repeat(40);
    f.engine.add_tool_version("bob", "fork", changed).unwrap();
    assert!(
        f.engine
            .add_tool_version("bob", "player", tool_version())
            .is_err()
    );
    f.engine.revoke("alice", "read").unwrap();
    let mut denied = request.clone();
    denied.tool_id = "denied".into();
    assert!(f.engine.fork_tool(denied.clone()).is_err());
    assert!(
        f.engine
            .tool_context("bob", "read", "player", "1.0.0")
            .is_err()
    );
    assert!(
        f.engine
            .tool_context("bob", "fork-root", "fork", "2.0.0")
            .is_ok()
    );
    assert_eq!(
        f.engine
            .tool_context("alice", "root", "player", "1.0.0")
            .unwrap()
            .selected,
        tool_version()
    );
    // A denied fork must leave no new tool record.
    let db = rusqlite::Connection::open(f.root.0.join("kernel.sqlite3")).unwrap();
    let count: i64 = db
        .query_row("SELECT count(*) FROM tools WHERE id='denied'", [], |r| {
            r.get(0)
        })
        .unwrap();
    assert_eq!(count, 0);
}
#[test]
fn invalid_tool_metadata_and_unowned_resources_are_rejected() {
    let mut f = Fixture::new();
    let mut request = RegisterTool {
        actor: "bob".into(),
        tool_id: "player".into(),
        resource: "project".into(),
        initial: tool_version(),
    };
    assert!(matches!(
        f.engine.register_tool(request.clone()),
        Err(Error::Denied(_))
    ));
    request.actor = "alice".into();
    request.initial.source_commit = "main".into();
    assert!(matches!(
        f.engine.register_tool(request.clone()),
        Err(Error::Invalid(_))
    ));
    request.initial = tool_version();
    f.engine.register_tool(request.clone()).unwrap();
    assert!(matches!(
        f.engine.register_tool(request),
        Err(Error::Conflict(_))
    ));
}
#[test]
fn schema_one_migration_preserves_inflight_work_and_grants() {
    let mut f = Fixture::new();
    f.start(template());
    let claim = f.claim();
    // Reconstruct the v1 schema while retaining a real checkpoint and permission tree.
    let db = rusqlite::Connection::open(f.root.0.join("kernel.sqlite3")).unwrap();
    db.execute_batch("DROP TABLE tool_versions; DROP TABLE tools; DROP TABLE local_tasks; PRAGMA user_version=1;")
        .unwrap();
    drop(db);
    drop(f.engine);
    f.engine = Engine::open_with_clock(f.root.0.join("kernel.sqlite3"), f.clock.clone()).unwrap();
    f.finish(&claim);
    register(&mut f);
    assert!(
        f.engine
            .tool_context("alice", "root", "player", "1.0.0")
            .is_ok()
    );
    let db = rusqlite::Connection::open(f.root.0.join("kernel.sqlite3")).unwrap();
    let version: i64 = db
        .query_row("PRAGMA user_version", [], |r| r.get(0))
        .unwrap();
    assert_eq!(version, 3);
}
