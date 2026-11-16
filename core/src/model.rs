use crate::{Error, Result};
use serde::{Deserialize, Serialize};
use sha2::{Digest, Sha256};
use std::collections::{BTreeMap, BTreeSet};

pub const ACTIONS: [&str; 8] = [
    "source.read",
    "architecture.read",
    "storage.plan",
    "source.write",
    "build.run",
    "verify.run",
    "release.publish",
    "runtime.activate",
];

#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Serialize, Deserialize)]
#[serde(rename_all = "snake_case")]
pub enum Stage {
    Discover,
    Document,
    Plan,
    Prepare,
    Build,
    Verify,
    Release,
    Activate,
}
impl Stage {
    pub const ALL: [Self; 8] = [
        Self::Discover,
        Self::Document,
        Self::Plan,
        Self::Prepare,
        Self::Build,
        Self::Verify,
        Self::Release,
        Self::Activate,
    ];
    pub fn action(self) -> &'static str {
        ACTIONS[self as usize]
    }
}

#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
#[serde(rename_all = "snake_case")]
pub enum Executor {
    FixtureOk,
    FixtureFail,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct Segment {
    pub id: String,
    pub stage: Stage,
    pub depends_on: BTreeSet<String>,
    pub executor: Executor,
    pub required_actions: BTreeSet<String>,
    pub required_outputs: BTreeSet<String>,
    pub max_attempts: u32,
    pub timeout_seconds: u64,
    /// An abstract upper-bound reservation, not a measured API price.
    pub cost_units: u64,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct Limits {
    pub max_operations: u32,
    pub max_cost_units: u64,
    pub deadline_seconds: u64,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct Template {
    pub schema_version: u32,
    pub id: String,
    pub version: String,
    pub limits: Limits,
    pub segments: Vec<Segment>,
}

pub(crate) fn identifier(value: &str) -> Result<()> {
    if value.is_empty()
        || value.len() > 96
        || !value
            .bytes()
            .all(|b| b.is_ascii_alphanumeric() || b"-_.".contains(&b))
        || value == "."
        || value == ".."
    {
        return Err(Error::Invalid(
            "identifiers must use 1–96 ASCII letters, digits, dots, underscores or hyphens".into(),
        ));
    }
    Ok(())
}
pub(crate) fn digest(value: &impl Serialize) -> Result<String> {
    Ok(format!("{:x}", Sha256::digest(serde_json::to_vec(value)?)))
}

impl Template {
    pub fn validate(&self) -> Result<()> {
        identifier(&self.id)?;
        let version: Vec<_> = self.version.split('.').collect();
        if self.schema_version != 1
            || version.len() != 3
            || version.iter().any(|v| {
                v.is_empty() || (v.len() > 1 && v.starts_with('0')) || v.parse::<u32>().is_err()
            })
        {
            return Err(Error::Invalid(
                "schema_version must be 1 and version must be major.minor.patch".into(),
            ));
        }
        if self.segments.len() < 8
            || self.segments.len() > 64
            || self.limits.max_operations == 0
            || self.limits.max_operations > 512
            || self.limits.max_cost_units == 0
            || self.limits.max_cost_units > 1_000_000_000
            || self.limits.deadline_seconds == 0
            || self.limits.deadline_seconds > 86400
        {
            return Err(Error::Invalid(
                "template size or run limits are out of range".into(),
            ));
        }
        let mut by_id = BTreeMap::new();
        for s in &self.segments {
            identifier(&s.id)?;
            if by_id.insert(s.id.clone(), s).is_some() {
                return Err(Error::Invalid("duplicate segment ID".into()));
            }
            if !(1..=5).contains(&s.max_attempts)
                || !(1..=300).contains(&s.timeout_seconds)
                || s.cost_units > self.limits.max_cost_units
                || s.required_outputs.is_empty()
                || s.required_outputs.len() > 8
                || s.required_actions.len() > 8
                || !s.required_actions.contains(s.stage.action())
                || s.required_actions
                    .iter()
                    .any(|a| !ACTIONS.contains(&a.as_str()))
            {
                return Err(Error::Invalid(format!(
                    "invalid contract for segment {}",
                    s.id
                )));
            }
            for name in &s.required_outputs {
                identifier(name)?;
            }
        }
        for s in &self.segments {
            for dep in &s.depends_on {
                let other = by_id
                    .get(dep)
                    .ok_or_else(|| Error::Invalid(format!("unknown dependency {dep}")))?;
                if other.stage > s.stage {
                    return Err(Error::Invalid(
                        "dependency reverses mandatory stages".into(),
                    ));
                }
            }
        }
        let mut visited = BTreeSet::new();
        while visited.len() < by_id.len() {
            let ready: Vec<_> = by_id
                .values()
                .filter(|s| !visited.contains(&s.id) && s.depends_on.is_subset(&visited))
                .map(|s| s.id.clone())
                .collect();
            if ready.is_empty() {
                return Err(Error::Invalid("workflow graph contains a cycle".into()));
            }
            visited.extend(ready);
        }
        for stage in Stage::ALL {
            if !self.segments.iter().any(|s| s.stage == stage) {
                return Err(Error::Invalid(format!(
                    "mandatory stage {stage:?} is missing"
                )));
            }
        }
        for s in &self.segments {
            if s.stage == Stage::Discover {
                continue;
            }
            let previous = Stage::ALL[s.stage as usize - 1];
            let mut ancestors = s.depends_on.clone();
            loop {
                let old = ancestors.len();
                for dep in ancestors.clone() {
                    ancestors.extend(by_id[&dep].depends_on.clone());
                }
                if old == ancestors.len() {
                    break;
                }
            }
            if self
                .segments
                .iter()
                .any(|other| other.stage == previous && !ancestors.contains(&other.id))
            {
                return Err(Error::Invalid(format!(
                    "{} must depend on every segment in the previous stage",
                    s.id
                )));
            }
        }
        Ok(())
    }
    pub fn hash(&self) -> Result<String> {
        self.validate()?;
        digest(self)
    }
}

pub type Outputs = BTreeMap<String, String>;
#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
#[serde(rename_all = "snake_case")]
pub enum RunStatus {
    Running,
    Complete,
    Failed,
    Cancelled,
    BudgetExhausted,
    Expired,
}
#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
#[serde(rename_all = "snake_case")]
pub enum SegmentStatus {
    Pending,
    Running,
    Complete,
    Failed,
}
#[derive(Debug, Clone, Serialize, Deserialize)]
pub(crate) struct Checkpoint {
    pub status: SegmentStatus,
    pub attempts: u32,
    pub worker: Option<String>,
    pub token: Option<String>,
    pub expires_at: Option<i64>,
    pub outputs: Outputs,
}
#[derive(Debug, Clone, Serialize, Deserialize)]
pub(crate) struct Run {
    pub id: String,
    pub owner: String,
    pub resource: String,
    pub grant_id: String,
    pub template: Template,
    pub template_hash: String,
    pub inputs: BTreeMap<String, String>,
    pub status: RunStatus,
    pub operations: u32,
    pub reserved_cost_units: u64,
    pub deadline_at: i64,
    pub checkpoints: BTreeMap<String, Checkpoint>,
}
#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct StartRequest {
    pub id: String,
    pub actor: String,
    pub resource: String,
    pub grant_id: String,
    pub template: Template,
    pub inputs: BTreeMap<String, String>,
}
#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct Claim {
    pub run_id: String,
    pub segment: Segment,
    pub template_hash: String,
    pub worker: String,
    pub token: String,
    pub attempt: u32,
    pub expires_at: i64,
    pub inputs: BTreeMap<String, String>,
    pub dependencies: BTreeMap<String, Outputs>,
}
#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(tag = "outcome", rename_all = "snake_case", deny_unknown_fields)]
pub enum WorkerReport {
    Success { outputs: Outputs },
    Failure { error: String },
}
#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(tag = "status", rename_all = "snake_case")]
pub enum ClaimResult {
    Claimed { claim: Box<Claim> },
    Idle { run_status: RunStatus },
}
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct SegmentView {
    pub status: SegmentStatus,
    pub attempts: u32,
    pub outputs: Outputs,
}
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct RunView {
    pub id: String,
    pub template_id: String,
    pub template_version: String,
    pub template_hash: String,
    pub status: RunStatus,
    pub operations: u32,
    pub reserved_cost_units: u64,
    pub deadline_at: i64,
    pub segments: BTreeMap<String, SegmentView>,
}
impl Run {
    pub fn view(&self) -> RunView {
        RunView {
            id: self.id.clone(),
            template_id: self.template.id.clone(),
            template_version: self.template.version.clone(),
            template_hash: self.template_hash.clone(),
            status: self.status,
            operations: self.operations,
            reserved_cost_units: self.reserved_cost_units,
            deadline_at: self.deadline_at,
            segments: self
                .checkpoints
                .iter()
                .map(|(id, c)| {
                    (
                        id.clone(),
                        SegmentView {
                            status: c.status,
                            attempts: c.attempts,
                            outputs: c.outputs.clone(),
                        },
                    )
                })
                .collect(),
        }
    }
}
pub fn fixture_report(claim: &Claim) -> Result<WorkerReport> {
    if claim.segment.executor == Executor::FixtureFail {
        return Ok(WorkerReport::Failure {
            error: "fixture requested failure".into(),
        });
    }
    let payload = (
        &claim.template_hash,
        &claim.segment.id,
        &claim.inputs,
        &claim.dependencies,
    );
    let outputs = claim
        .segment
        .required_outputs
        .iter()
        .map(|key| Ok((key.clone(), digest(&(&payload, key))?)))
        .collect::<Result<Outputs>>()?;
    Ok(WorkerReport::Success { outputs })
}
