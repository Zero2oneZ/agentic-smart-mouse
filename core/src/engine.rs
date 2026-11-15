use crate::{Error, Result, model::*, permissions};
use rusqlite::{Connection, OptionalExtension, TransactionBehavior, params};
use serde_json::{Value, json};
use std::{
    collections::BTreeMap,
    fs,
    path::{Component, Path},
    sync::Arc,
    time::{Duration, SystemTime, UNIX_EPOCH},
};

const APPLICATION_ID: i64 = 0x48425843;
pub trait Clock: Send + Sync {
    fn now(&self) -> i64;
}
pub struct SystemClock;
impl Clock for SystemClock {
    fn now(&self) -> i64 {
        SystemTime::now()
            .duration_since(UNIX_EPOCH)
            .unwrap_or_default()
            .as_secs() as i64
    }
}
pub struct Engine {
    pub(crate) db: Connection,
    clock: Arc<dyn Clock>,
}

pub(crate) fn event(
    db: &Connection,
    run: Option<&str>,
    actor: &str,
    kind: &str,
    detail: &Value,
    now: i64,
) -> Result<()> {
    db.execute(
        "INSERT INTO events(run_id,actor,kind,detail,created_at) VALUES(?1,?2,?3,?4,?5)",
        params![run, actor, kind, serde_json::to_string(detail)?, now],
    )?;
    Ok(())
}
fn load(db: &Connection, id: &str) -> Result<Run> {
    let document: String = db
        .query_row("SELECT document FROM runs WHERE id=?1", [id], |r| r.get(0))
        .optional()?
        .ok_or_else(|| Error::NotFound("run".into()))?;
    Ok(serde_json::from_str(&document)?)
}
fn save(db: &Connection, run: &Run) -> Result<()> {
    db.execute(
        "UPDATE runs SET document=?2 WHERE id=?1",
        params![run.id, serde_json::to_string(run)?],
    )?;
    Ok(())
}
fn owns(run: &Run, actor: &str) -> Result<()> {
    if run.owner != actor {
        return Err(Error::Denied("run belongs to another actor".into()));
    }
    Ok(())
}
fn claim_view(run: &Run, segment: &Segment) -> Claim {
    let c = &run.checkpoints[&segment.id];
    Claim {
        run_id: run.id.clone(),
        segment: segment.clone(),
        template_hash: run.template_hash.clone(),
        worker: c.worker.clone().expect("running worker"),
        token: c.token.clone().expect("running token"),
        attempt: c.attempts,
        expires_at: c.expires_at.expect("running expiry"),
        inputs: run.inputs.clone(),
        dependencies: segment
            .depends_on
            .iter()
            .map(|id| (id.clone(), run.checkpoints[id].outputs.clone()))
            .collect(),
    }
}
fn clear_lease(c: &mut Checkpoint) {
    c.worker = None;
    c.token = None;
    c.expires_at = None;
}
fn check_live(db: &Connection, run: &Run, actor: &str, now: i64) -> Result<()> {
    owns(run, actor)?;
    // Even an empty action set checks the complete current grant lineage.
    permissions::require(
        db,
        &run.grant_id,
        actor,
        &run.resource,
        &Default::default(),
        now,
    )
}

impl Engine {
    pub fn open(path: impl AsRef<Path>) -> Result<Self> {
        Self::open_with_clock(path, Arc::new(SystemClock))
    }
    pub fn open_with_clock(path: impl AsRef<Path>, clock: Arc<dyn Clock>) -> Result<Self> {
        let path = path.as_ref();
        let absolute = if path.is_absolute() {
            path.to_owned()
        } else {
            std::env::current_dir()?.join(path)
        };
        if absolute
            .components()
            .any(|c| matches!(c, Component::ParentDir))
        {
            return Err(Error::Invalid(
                "database path must not contain parent traversal".into(),
            ));
        }
        let mut part = std::path::PathBuf::new();
        for component in absolute.components() {
            part.push(component);
            if let Ok(meta) = fs::symlink_metadata(&part)
                && meta.file_type().is_symlink()
            {
                return Err(Error::Invalid(
                    "database paths must not traverse symlinks".into(),
                ));
            }
        }
        if !absolute.exists() {
            let mut options = fs::OpenOptions::new();
            options.write(true).create_new(true);
            #[cfg(unix)]
            {
                use std::os::unix::fs::OpenOptionsExt;
                options.mode(0o600);
            }
            options.open(&absolute)?;
        }
        let mut db = Connection::open(&absolute)?;
        db.busy_timeout(Duration::from_secs(3))?;
        let app_id: i64 = db.query_row("PRAGMA application_id", [], |r| r.get(0))?;
        let tables: i64 = db.query_row(
            "SELECT count(*) FROM sqlite_master WHERE name NOT LIKE 'sqlite_%'",
            [],
            |r| r.get(0),
        )?;
        let version: i64 = db.query_row("PRAGMA user_version", [], |r| r.get(0))?;
        if (app_id != 0 && app_id != APPLICATION_ID)
            || (app_id == 0 && (tables != 0 || version != 0))
            || (app_id == APPLICATION_ID && version != 1)
        {
            return Err(Error::Invalid(
                "database belongs to another application or unsupported schema".into(),
            ));
        }
        // This is a separate database; never migrate the Qt chat database implicitly.
        #[cfg(unix)]
        {
            use std::os::unix::fs::PermissionsExt;
            fs::set_permissions(&absolute, fs::Permissions::from_mode(0o600))?;
        }
        db.execute_batch(
            "PRAGMA foreign_keys=ON; PRAGMA journal_mode=WAL; PRAGMA synchronous=FULL;",
        )?;
        let tx = db.transaction_with_behavior(TransactionBehavior::Immediate)?;
        tx.execute_batch("CREATE TABLE IF NOT EXISTS resources(id TEXT PRIMARY KEY,owner TEXT NOT NULL);
            CREATE TABLE IF NOT EXISTS grants(id TEXT PRIMARY KEY,document TEXT NOT NULL CHECK(json_valid(document)));
            CREATE TABLE IF NOT EXISTS runs(id TEXT PRIMARY KEY,document TEXT NOT NULL CHECK(json_valid(document)));
            CREATE TABLE IF NOT EXISTS events(id INTEGER PRIMARY KEY,run_id TEXT,actor TEXT NOT NULL,kind TEXT NOT NULL,
                detail TEXT NOT NULL CHECK(json_valid(detail)),created_at INTEGER NOT NULL);
            CREATE INDEX IF NOT EXISTS events_run ON events(run_id,id);
            PRAGMA application_id=1212307523; PRAGMA user_version=1;")?;
        tx.commit()?;
        Ok(Self { db, clock })
    }
    pub(crate) fn now(&self) -> i64 {
        self.clock.now()
    }
    pub fn start(&mut self, request: StartRequest) -> Result<RunView> {
        for id in [
            &request.id,
            &request.actor,
            &request.resource,
            &request.grant_id,
        ] {
            identifier(id)?;
        }
        let hash = request.template.hash()?;
        if request.inputs.len() > 32
            || request
                .inputs
                .iter()
                .any(|(k, v)| identifier(k).is_err() || v.len() > 4096)
            || serde_json::to_vec(&request.inputs)?.len() > 32768
        {
            return Err(Error::Invalid(
                "input context exceeds the contract limits".into(),
            ));
        }
        let now = self.now();
        let tx = self
            .db
            .transaction_with_behavior(TransactionBehavior::Immediate)?;
        let actions = request
            .template
            .segments
            .iter()
            .flat_map(|s| s.required_actions.clone())
            .collect();
        permissions::require(
            &tx,
            &request.grant_id,
            &request.actor,
            &request.resource,
            &actions,
            now,
        )?;
        let exists: bool = tx.query_row(
            "SELECT EXISTS(SELECT 1 FROM runs WHERE id=?1)",
            [&request.id],
            |r| r.get(0),
        )?;
        if exists {
            let old = load(&tx, &request.id)?;
            if old.owner != request.actor
                || old.resource != request.resource
                || old.grant_id != request.grant_id
                || old.template_hash != hash
                || old.inputs != request.inputs
            {
                return Err(Error::Conflict(
                    "run ID already pins a different request".into(),
                ));
            }
            tx.commit()?;
            return Ok(old.view());
        }
        let checkpoints = request
            .template
            .segments
            .iter()
            .map(|s| {
                (
                    s.id.clone(),
                    Checkpoint {
                        status: SegmentStatus::Pending,
                        attempts: 0,
                        worker: None,
                        token: None,
                        expires_at: None,
                        outputs: BTreeMap::new(),
                    },
                )
            })
            .collect();
        let run = Run {
            id: request.id,
            owner: request.actor,
            resource: request.resource,
            grant_id: request.grant_id,
            deadline_at: now.saturating_add(request.template.limits.deadline_seconds as i64),
            template: request.template,
            template_hash: hash,
            inputs: request.inputs,
            status: RunStatus::Running,
            operations: 0,
            reserved_cost_units: 0,
            checkpoints,
        };
        tx.execute(
            "INSERT INTO runs(id,document) VALUES(?1,?2)",
            params![run.id, serde_json::to_string(&run)?],
        )?;
        event(
            &tx,
            Some(&run.id),
            &run.owner,
            "run.started",
            &json!({"template_hash":run.template_hash}),
            now,
        )?;
        tx.commit()?;
        Ok(run.view())
    }
    pub fn status(&self, actor: &str, run_id: &str) -> Result<RunView> {
        let run = load(&self.db, run_id)?;
        check_live(&self.db, &run, actor, self.now())?;
        Ok(run.view())
    }
    pub fn claim(&mut self, actor: &str, run_id: &str, worker: &str) -> Result<ClaimResult> {
        identifier(worker)?;
        let now = self.now();
        let tx = self
            .db
            .transaction_with_behavior(TransactionBehavior::Immediate)?;
        let mut run = load(&tx, run_id)?;
        check_live(&tx, &run, actor, now)?;
        if run.status != RunStatus::Running {
            return Ok(ClaimResult::Idle {
                run_status: run.status,
            });
        }
        if now >= run.deadline_at {
            run.status = RunStatus::Expired;
        }
        if run.status == RunStatus::Running {
            for s in &run.template.segments {
                let c = run.checkpoints.get_mut(&s.id).expect("validated segment");
                if c.status == SegmentStatus::Running
                    && now >= c.expires_at.expect("running expiry")
                {
                    c.status = if c.attempts >= s.max_attempts {
                        SegmentStatus::Failed
                    } else {
                        SegmentStatus::Pending
                    };
                    clear_lease(c);
                    event(
                        &tx,
                        Some(run_id),
                        actor,
                        "segment.lease_expired",
                        &json!({"segment":s.id}),
                        now,
                    )?;
                }
            }
            if run
                .checkpoints
                .values()
                .any(|c| c.status == SegmentStatus::Failed)
            {
                run.status = RunStatus::Failed;
            }
        }
        if run.status != RunStatus::Running {
            for c in run.checkpoints.values_mut() {
                clear_lease(c);
            }
            save(&tx, &run)?;
            event(
                &tx,
                Some(run_id),
                actor,
                "run.stopped",
                &json!({"status":run.status}),
                now,
            )?;
            tx.commit()?;
            return Ok(ClaimResult::Idle {
                run_status: run.status,
            });
        }
        // Serialize execution for v1; DAG contracts are retained for future parallel runners.
        if run
            .checkpoints
            .values()
            .any(|c| c.status == SegmentStatus::Running)
        {
            tx.commit()?;
            return Ok(ClaimResult::Idle {
                run_status: run.status,
            });
        }
        let segment = run
            .template
            .segments
            .iter()
            .filter(|s| {
                run.checkpoints[&s.id].status == SegmentStatus::Pending
                    && s.depends_on
                        .iter()
                        .all(|id| run.checkpoints[id].status == SegmentStatus::Complete)
            })
            .min_by_key(|s| (s.stage, &s.id))
            .cloned();
        let Some(segment) = segment else {
            return Err(Error::Conflict(
                "no executable segment in a running workflow".into(),
            ));
        };
        permissions::require(
            &tx,
            &run.grant_id,
            actor,
            &run.resource,
            &segment.required_actions,
            now,
        )?;
        if run.operations >= run.template.limits.max_operations
            || run.reserved_cost_units.saturating_add(segment.cost_units)
                > run.template.limits.max_cost_units
        {
            run.status = RunStatus::BudgetExhausted;
            save(&tx, &run)?;
            event(
                &tx,
                Some(run_id),
                actor,
                "run.budget_exhausted",
                &json!({}),
                now,
            )?;
            tx.commit()?;
            return Ok(ClaimResult::Idle {
                run_status: run.status,
            });
        }
        run.operations += 1;
        run.reserved_cost_units += segment.cost_units;
        let token: String = tx.query_row("SELECT lower(hex(randomblob(32)))", [], |r| r.get(0))?;
        let c = run
            .checkpoints
            .get_mut(&segment.id)
            .expect("validated segment");
        c.status = SegmentStatus::Running;
        c.attempts += 1;
        c.worker = Some(worker.into());
        c.token = Some(token);
        c.expires_at = Some(
            run.deadline_at
                .min(now.saturating_add(segment.timeout_seconds as i64)),
        );
        let claim = claim_view(&run, &segment);
        save(&tx, &run)?;
        event(
            &tx,
            Some(run_id),
            actor,
            "segment.claimed",
            &json!({"segment":segment.id,"worker":worker,"attempt":claim.attempt}),
            now,
        )?;
        tx.commit()?;
        Ok(ClaimResult::Claimed {
            claim: Box::new(claim),
        })
    }
    pub fn finish(
        &mut self,
        actor: &str,
        run_id: &str,
        segment_id: &str,
        worker: &str,
        token: &str,
        report: WorkerReport,
    ) -> Result<RunView> {
        let now = self.now();
        let tx = self
            .db
            .transaction_with_behavior(TransactionBehavior::Immediate)?;
        let mut run = load(&tx, run_id)?;
        check_live(&tx, &run, actor, now)?;
        if run.status != RunStatus::Running || now >= run.deadline_at {
            return Err(Error::Conflict(
                "run is terminal or its deadline has elapsed".into(),
            ));
        }
        let spec = run
            .template
            .segments
            .iter()
            .find(|s| s.id == segment_id)
            .ok_or_else(|| Error::NotFound("segment".into()))?
            .clone();
        let checkpoint = &run.checkpoints[segment_id];
        if checkpoint.status != SegmentStatus::Running
            || checkpoint.worker.as_deref() != Some(worker)
            || checkpoint.token.as_deref() != Some(token)
            || now >= checkpoint.expires_at.unwrap_or(0)
        {
            return Err(Error::Denied(
                "worker lease is stale, expired or does not match".into(),
            ));
        }
        permissions::require(
            &tx,
            &run.grant_id,
            actor,
            &run.resource,
            &spec.required_actions,
            now,
        )?;
        let expected = fixture_report(&claim_view(&run, &spec))?;
        let checkpoint = run
            .checkpoints
            .get_mut(segment_id)
            .expect("validated segment");
        match report {
            WorkerReport::Success { outputs } => {
                let WorkerReport::Success { outputs: expected } = expected else {
                    return Err(Error::Invalid(
                        "fixture failure cannot be reported as successful".into(),
                    ));
                };
                if outputs != expected {
                    return Err(Error::Invalid(
                        "output evidence does not match the pinned fixture contract".into(),
                    ));
                }
                checkpoint.outputs = outputs;
                checkpoint.status = SegmentStatus::Complete;
                event(
                    &tx,
                    Some(run_id),
                    actor,
                    "segment.verified",
                    &json!({"segment":segment_id}),
                    now,
                )?;
            }
            WorkerReport::Failure { error } => {
                if error.len() > 1024 {
                    return Err(Error::Invalid("failure summary exceeds 1024 bytes".into()));
                }
                checkpoint.status = if checkpoint.attempts >= spec.max_attempts {
                    SegmentStatus::Failed
                } else {
                    SegmentStatus::Pending
                };
                // Store a bounded status, not arbitrary worker stderr or credentials.
                event(
                    &tx,
                    Some(run_id),
                    actor,
                    "segment.failed",
                    &json!({"segment":segment_id}),
                    now,
                )?;
            }
        }
        clear_lease(checkpoint);
        if run
            .checkpoints
            .values()
            .all(|c| c.status == SegmentStatus::Complete)
        {
            run.status = RunStatus::Complete;
        } else if run
            .checkpoints
            .values()
            .any(|c| c.status == SegmentStatus::Failed)
        {
            run.status = RunStatus::Failed;
        }
        if run.status != RunStatus::Running {
            event(
                &tx,
                Some(run_id),
                actor,
                "run.stopped",
                &json!({"status":run.status}),
                now,
            )?;
        }
        save(&tx, &run)?;
        tx.commit()?;
        Ok(run.view())
    }
    pub fn cancel(&mut self, actor: &str, run_id: &str) -> Result<RunView> {
        let now = self.now();
        let tx = self
            .db
            .transaction_with_behavior(TransactionBehavior::Immediate)?;
        let mut run = load(&tx, run_id)?;
        // Owners may stop their own work even after an access grant is revoked.
        owns(&run, actor)?;
        if run.status == RunStatus::Running {
            run.status = RunStatus::Cancelled;
            for c in run.checkpoints.values_mut() {
                clear_lease(c);
            }
            save(&tx, &run)?;
            event(&tx, Some(run_id), actor, "run.cancelled", &json!({}), now)?;
        }
        tx.commit()?;
        Ok(run.view())
    }
    pub fn events(&self, actor: &str, run_id: &str, after: i64) -> Result<Vec<Value>> {
        if after < 0 {
            return Err(Error::Invalid("event cursor must be non-negative".into()));
        }
        let run = load(&self.db, run_id)?;
        check_live(&self.db, &run, actor, self.now())?;
        let mut stmt = self.db.prepare("SELECT id,kind,detail,created_at FROM events WHERE run_id=?1 AND id>?2 ORDER BY id LIMIT 100")?;
        let rows = stmt.query_map(params![run_id, after], |r| {
            Ok((
                r.get::<_, i64>(0)?,
                r.get::<_, String>(1)?,
                r.get::<_, String>(2)?,
                r.get::<_, i64>(3)?,
            ))
        })?;
        rows.map(|row| {
            let (id, kind, detail, created_at) = row?;
            Ok(json!({"id":id,"kind":kind,
            "detail":serde_json::from_str::<Value>(&detail)?,"created_at":created_at}))
        })
        .collect()
    }
}
