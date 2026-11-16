//! Portable environment requirements and non-executable plans. No runner or authorization.
use crate::{
    Error, Result,
    model::{digest, identifier},
};
use serde::{Deserialize, Serialize};
use std::collections::{BTreeMap, BTreeSet};

#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Serialize, Deserialize)]
#[serde(rename_all = "snake_case")]
pub enum Backend {
    Simulation,
    Bubblewrap,
    Oci,
    Microvm,
}
#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
#[serde(rename_all = "snake_case")]
pub enum TrustZone {
    Simulation,
    Ingress,
    Quarantine,
    Trusted,
}
#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Serialize, Deserialize)]
#[serde(rename_all = "snake_case")]
pub enum Ability {
    ObserveFixture,
    FetchSource,
    ReadArtifact,
    WriteArtifact,
    ParseDocument,
    ValidateSchema,
    TransformData,
    ExecuteBuild,
}
#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Serialize, Deserialize)]
#[serde(rename_all = "snake_case")]
pub enum StorageRole {
    CodeRead,
    InboxRead,
    InboxWrite,
    QuarantineRead,
    QuarantineWrite,
    StagingWrite,
    TrustedRead,
    TrustedWrite,
    Scratch,
}
#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct StorageRequirement {
    pub binding_ref: String,
    pub role: StorageRole,
}
#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
#[serde(rename_all = "snake_case")]
pub enum NetworkMode {
    Deny,
    Brokered,
}
#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct NetworkRequirement {
    pub mode: NetworkMode,
    /// Registry references, never caller-supplied URLs, addresses or credentials.
    pub endpoint_refs: BTreeSet<String>,
}
#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct EnvironmentLimits {
    pub cpu_millis: u32,
    pub memory_mib: u32,
    pub pids: u32,
    pub wall_seconds: u32,
    pub scratch_mib: u32,
    pub output_bytes: u64,
}
#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct Backpack {
    pub schema_version: u32,
    pub id: String,
    pub version: String,
    pub architecture_ref: String,
    pub harness_ref: String,
    pub agent_ref: Option<String>,
    pub zone: TrustZone,
    pub abilities: BTreeSet<Ability>,
    /// Required rootfs/image/kernel/tool artifacts. Digests are declared, not fetched here.
    pub artifacts: BTreeMap<String, String>,
    pub storage: Vec<StorageRequirement>,
    pub network: NetworkRequirement,
    pub limits: EnvironmentLimits,
}
fn invalid(message: &str) -> Error {
    Error::Invalid(format!("environment: {message}"))
}
fn sha256(value: &str) -> bool {
    value.strip_prefix("sha256:").is_some_and(|s| {
        s.len() == 64
            && s.bytes()
                .all(|b| b.is_ascii_digit() || (b'a'..=b'f').contains(&b))
    })
}
impl Backpack {
    pub fn validate(&self) -> Result<()> {
        if self.schema_version != 1 {
            return Err(invalid("unsupported backpack schema"));
        }
        for id in [
            &self.id,
            &self.version,
            &self.architecture_ref,
            &self.harness_ref,
        ] {
            identifier(id)?;
        }
        if let Some(agent) = &self.agent_ref {
            identifier(agent)?;
        }
        if self.abilities.is_empty()
            || self.abilities.len() > 8
            || self.artifacts.is_empty()
            || self.artifacts.len() > 32
            || self.storage.is_empty()
            || self.storage.len() > 16
            || self.network.endpoint_refs.len() > 16
        {
            return Err(invalid("backpack requirements exceed contract bounds"));
        }
        for (id, digest) in &self.artifacts {
            identifier(id)?;
            if !sha256(digest) {
                return Err(invalid("artifacts require full lowercase sha256 digests"));
            }
        }
        let limits = &self.limits;
        if !(1..=64000).contains(&limits.cpu_millis)
            || !(16..=262144).contains(&limits.memory_mib)
            || !(1..=4096).contains(&limits.pids)
            || !(1..=86400).contains(&limits.wall_seconds)
            || !(1..=1048576).contains(&limits.scratch_mib)
            || !(1..=1073741824).contains(&limits.output_bytes)
        {
            return Err(invalid(
                "positive bounded CPU, memory, process, wall, scratch and output limits required",
            ));
        }
        let mut bindings = BTreeSet::new();
        let mut roles = BTreeSet::new();
        for requirement in &self.storage {
            identifier(&requirement.binding_ref)?;
            if !bindings.insert(&requirement.binding_ref) {
                return Err(invalid("a binding may have only one declared role"));
            }
            roles.insert(requirement.role);
            let allowed = match self.zone {
                TrustZone::Simulation => matches!(
                    requirement.role,
                    StorageRole::CodeRead | StorageRole::Scratch
                ),
                TrustZone::Ingress => matches!(
                    requirement.role,
                    StorageRole::CodeRead | StorageRole::InboxWrite | StorageRole::Scratch
                ),
                TrustZone::Quarantine => matches!(
                    requirement.role,
                    StorageRole::CodeRead
                        | StorageRole::InboxRead
                        | StorageRole::QuarantineRead
                        | StorageRole::QuarantineWrite
                        | StorageRole::StagingWrite
                        | StorageRole::Scratch
                ),
                TrustZone::Trusted => matches!(
                    requirement.role,
                    StorageRole::CodeRead
                        | StorageRole::TrustedRead
                        | StorageRole::TrustedWrite
                        | StorageRole::Scratch
                ),
            };
            if !allowed {
                return Err(invalid("storage role crosses the declared trust boundary"));
            }
        }
        for endpoint in &self.network.endpoint_refs {
            identifier(endpoint)?;
        }
        if self.network.mode == NetworkMode::Deny && !self.network.endpoint_refs.is_empty() {
            return Err(invalid("denied networking cannot declare endpoints"));
        }
        if self.network.mode == NetworkMode::Brokered
            && (self.network.endpoint_refs.is_empty() || self.zone != TrustZone::Ingress)
        {
            return Err(invalid(
                "brokered networking requires an ingress zone and explicit endpoint references",
            ));
        }
        if self.abilities.contains(&Ability::FetchSource)
            && (self.zone != TrustZone::Ingress
                || self.network.mode != NetworkMode::Brokered
                || !roles.contains(&StorageRole::InboxWrite))
        {
            return Err(invalid(
                "source fetching requires brokered ingress and an inbox-write binding",
            ));
        }
        if self.zone == TrustZone::Ingress
            && (self.abilities.contains(&Ability::ExecuteBuild)
                || self.abilities.contains(&Ability::TransformData))
        {
            return Err(invalid(
                "ingress cannot combine fetching with build or transformation execution",
            ));
        }
        if self.zone == TrustZone::Simulation && self.abilities.contains(&Ability::ExecuteBuild) {
            return Err(invalid("simulation cannot request real build execution"));
        }
        if self.abilities.contains(&Ability::ObserveFixture) && self.zone != TrustZone::Simulation {
            return Err(invalid("fixture observation requires the simulation zone"));
        }
        if !roles.contains(&StorageRole::Scratch) {
            return Err(invalid(
                "each instance requires a private bounded scratch binding",
            ));
        }
        Ok(())
    }
    pub fn hash(&self) -> Result<String> {
        self.validate()?;
        digest(self)
    }
    pub fn example(name: &str) -> Result<Self> {
        let text = match name {
            "simulation" => include_str!("../templates/backpacks/simulation-v1.json"),
            "ingress" => include_str!("../templates/backpacks/ingress-v1.json"),
            "quarantine" => include_str!("../templates/backpacks/quarantine-v1.json"),
            "trusted" => include_str!("../templates/backpacks/trusted-v1.json"),
            _ => {
                return Err(invalid(
                    "unknown example; choose simulation, ingress, quarantine or trusted",
                ));
            }
        };
        let backpack: Self = serde_json::from_str(text)?;
        backpack.validate()?;
        Ok(backpack)
    }
}
#[derive(Debug, Clone, Copy, Serialize, Deserialize)]
#[serde(rename_all = "snake_case")]
pub enum DeploymentTarget {
    Local,
    Cloud,
}
#[derive(Debug, Clone, Copy, Serialize, Deserialize)]
#[serde(rename_all = "snake_case")]
pub enum OperatingSystem {
    Linux,
}
#[derive(Debug, Clone, Copy, Serialize, Deserialize)]
#[serde(rename_all = "snake_case")]
pub enum Architecture {
    Amd64,
    Arm64,
}
#[derive(Debug, Clone, Copy, Serialize, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct Platform {
    pub os: OperatingSystem,
    pub architecture: Architecture,
}
#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct EnvironmentRequest {
    pub schema_version: u32,
    pub instance_id: String,
    pub scope_ref: String,
    pub target: DeploymentTarget,
    pub platform: Platform,
    pub backend: Backend,
    pub backpack: Backpack,
}
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct EnvironmentPlan {
    pub request_hash: String,
    pub backpack_hash: String,
    pub instance_id: String,
    pub scope_ref: String,
    pub target: DeploymentTarget,
    pub platform: Platform,
    pub backend: Backend,
    pub isolation_requirement: String,
    pub launchable: bool,
    pub required_checks: Vec<String>,
}
impl EnvironmentRequest {
    pub fn plan(&self) -> Result<EnvironmentPlan> {
        if self.schema_version != 1 {
            return Err(invalid("unsupported request schema"));
        }
        identifier(&self.instance_id)?;
        identifier(&self.scope_ref)?;
        self.backpack.validate()?;
        let b = &self.backpack;
        if (self.backend == Backend::Simulation) != (b.zone == TrustZone::Simulation) {
            return Err(invalid(
                "simulation backend and simulation zone must be selected together",
            ));
        }
        // Each backend needs separately pinned assets; no fallback or tag resolution.
        let required: &[&str] = match self.backend {
            Backend::Simulation => &["fixture"],
            Backend::Bubblewrap => &["rootfs", "seccomp"],
            Backend::Oci => &["image", "seccomp"],
            Backend::Microvm => &["kernel", "rootfs", "machine-config"],
        };
        if required.iter().any(|key| !b.artifacts.contains_key(*key)) {
            return Err(invalid("selected backend lacks required pinned artifacts"));
        }
        let isolation = match self.backend {
            Backend::Simulation => "none_fixture_only",
            Backend::Bubblewrap | Backend::Oci => "shared_host_kernel",
            Backend::Microvm => "separate_guest_kernel",
        };
        let mut checks: Vec<String> = [
            "runner_not_implemented",
            "resolve_and_authorize_scope_identity_and_current_grants",
            "resolve_harness_agent_architecture_and_policy_revisions",
            "verify_artifact_digests_and_trust_policy",
            "bind_scoped_storage_without_aliases_or_path_escape",
            "enforce_resource_limits_and_bounded_redacted_logs",
            "persist_instance_operation_attempt_and_lease_fencing",
        ]
        .into_iter()
        .map(String::from)
        .collect();
        if self.backend != Backend::Simulation {
            checks.push("attest_platform_and_backend_controls_or_refuse_without_fallback".into());
            checks.push("enforce_network_policy_and_cancel_process_tree".into());
        } else {
            checks.push("use_fixture_inputs_only_no_external_effects".into());
        }
        if b.zone == TrustZone::Ingress {
            checks.push("enforce_endpoint_resolution_redirect_and_broker_policy".into());
        }
        if b.zone == TrustZone::Trusted {
            checks.push("accept_only_artifacts_with_current_admission_receipts".into());
        }
        Ok(EnvironmentPlan {
            request_hash: digest(self)?,
            backpack_hash: b.hash()?,
            instance_id: self.instance_id.clone(),
            scope_ref: self.scope_ref.clone(),
            target: self.target,
            platform: self.platform,
            backend: self.backend,
            isolation_requirement: isolation.into(),
            launchable: false,
            required_checks: checks,
        })
    }
}
