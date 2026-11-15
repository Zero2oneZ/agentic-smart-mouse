use hotbox_core::*;
fn request(example: &str, backend: Backend) -> EnvironmentRequest {
    EnvironmentRequest {
        schema_version: 1,
        instance_id: "instance-example".into(),
        scope_ref: "project-example".into(),
        target: DeploymentTarget::Local,
        platform: Platform {
            os: OperatingSystem::Linux,
            architecture: Architecture::Amd64,
        },
        backend,
        backpack: Backpack::example(example).unwrap(),
    }
}
#[test]
fn portable_plans_are_hashed_but_never_launchable_or_authorized() {
    for (example, backend, isolation) in [
        ("simulation", Backend::Simulation, "none_fixture_only"),
        ("ingress", Backend::Oci, "shared_host_kernel"),
        ("quarantine", Backend::Bubblewrap, "shared_host_kernel"),
        ("trusted", Backend::Oci, "shared_host_kernel"),
    ] {
        let local = request(example, backend);
        let plan = local.plan().unwrap();
        assert!(!plan.launchable);
        assert_eq!(plan.isolation_requirement, isolation);
        assert!(
            plan.required_checks
                .iter()
                .any(|v| v == "runner_not_implemented")
        );
        let mut cloud = local.clone();
        cloud.target = DeploymentTarget::Cloud;
        let cloud_plan = cloud.plan().unwrap();
        assert_eq!(plan.backpack_hash, cloud_plan.backpack_hash);
        assert_ne!(plan.request_hash, cloud_plan.request_hash);
        assert!(!cloud_plan.launchable);
        cloud.instance_id = "another-instance".into();
        assert_ne!(cloud_plan.request_hash, cloud.plan().unwrap().request_hash);
    }
}
#[test]
fn ingress_cannot_mount_trusted_data_or_execute_transformations() {
    let mut b = Backpack::example("ingress").unwrap();
    b.storage.push(StorageRequirement {
        binding_ref: "trusted-library".into(),
        role: StorageRole::TrustedRead,
    });
    assert!(b.validate().is_err());
    let mut b = Backpack::example("ingress").unwrap();
    b.abilities.insert(Ability::TransformData);
    assert!(b.validate().is_err());
    let mut b = Backpack::example("ingress").unwrap();
    b.abilities.insert(Ability::ExecuteBuild);
    assert!(b.validate().is_err());
    let mut b = Backpack::example("ingress").unwrap();
    b.storage.retain(|v| v.role != StorageRole::InboxWrite);
    assert!(b.validate().is_err());
}
#[test]
fn quarantine_has_no_network_or_trusted_store_access() {
    let mut b = Backpack::example("quarantine").unwrap();
    b.network.mode = NetworkMode::Brokered;
    b.network.endpoint_refs.insert("endpoint".into());
    assert!(b.validate().is_err());
    let mut b = Backpack::example("quarantine").unwrap();
    b.storage.push(StorageRequirement {
        binding_ref: "trusted-output".into(),
        role: StorageRole::TrustedWrite,
    });
    assert!(b.validate().is_err());
    let mut b = Backpack::example("trusted").unwrap();
    b.storage.push(StorageRequirement {
        binding_ref: "unreviewed-inbox".into(),
        role: StorageRole::InboxRead,
    });
    assert!(b.validate().is_err());
}
#[test]
fn mounts_and_endpoints_are_references_without_host_paths_or_secret_fields() {
    let mut b = Backpack::example("ingress").unwrap();
    b.storage[0].binding_ref = "/home/tom".into();
    assert!(b.validate().is_err());
    let mut b = Backpack::example("ingress").unwrap();
    b.network
        .endpoint_refs
        .insert("https://example.org/private?token=secret".into());
    assert!(b.validate().is_err());
    let mut b = Backpack::example("quarantine").unwrap();
    b.storage.push(b.storage[0].clone());
    assert!(b.validate().is_err());
    let mut json = serde_json::to_value(Backpack::example("ingress").unwrap()).unwrap();
    json["credentials"] = "plaintext".into();
    assert!(serde_json::from_value::<Backpack>(json).is_err());
}
#[test]
fn missing_pins_and_unknown_backends_cannot_downgrade_isolation() {
    let mut r = request("quarantine", Backend::Microvm);
    assert!(r.plan().is_err());
    for key in ["kernel", "machine-config"] {
        r.backpack
            .artifacts
            .insert(key.into(), format!("sha256:{}", "a".repeat(64)));
    }
    assert_eq!(
        r.plan().unwrap().isolation_requirement,
        "separate_guest_kernel"
    );
    let mut r = request("ingress", Backend::Oci);
    r.backpack
        .artifacts
        .insert("image".into(), "example:latest".into());
    assert!(r.plan().is_err());
    let mut r = request("quarantine", Backend::Bubblewrap);
    r.backpack.artifacts.remove("seccomp");
    assert!(r.plan().is_err());
    let mut json = serde_json::to_value(request("ingress", Backend::Oci)).unwrap();
    json["backend"] = "host_shell".into();
    assert!(serde_json::from_value::<EnvironmentRequest>(json).is_err());
}
#[test]
fn simulation_cannot_be_mistaken_for_a_real_sandbox_or_fetcher() {
    let mut r = request("simulation", Backend::Oci);
    assert!(r.plan().is_err());
    r.backend = Backend::Simulation;
    r.backpack.abilities.insert(Ability::FetchSource);
    assert!(r.plan().is_err());
    let mut r = request("simulation", Backend::Simulation);
    r.backpack.abilities.insert(Ability::ExecuteBuild);
    assert!(r.plan().is_err());
    let mut r = request("ingress", Backend::Simulation);
    assert!(r.plan().is_err());
    r.backpack = Backpack::example("simulation").unwrap();
    assert!(r.plan().is_ok());
}
#[test]
fn limits_and_schema_versions_fail_closed_before_planning() {
    let mut r = request("quarantine", Backend::Bubblewrap);
    r.backpack.limits.memory_mib = 0;
    assert!(r.plan().is_err());
    let mut r = request("quarantine", Backend::Bubblewrap);
    r.backpack.limits.output_bytes = u64::MAX;
    assert!(r.plan().is_err());
    let mut r = request("quarantine", Backend::Bubblewrap);
    r.schema_version = 2;
    assert!(r.plan().is_err());
    let mut r = request("quarantine", Backend::Bubblewrap);
    r.backpack.schema_version = 2;
    assert!(r.plan().is_err());
}
