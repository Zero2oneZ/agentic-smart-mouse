use hotbox_core::SystemCatalog;

#[test]
fn bundled_catalog_is_pinned_without_loading_a_database() {
    let catalog = SystemCatalog::bundled().unwrap();
    let serialized = serde_json::to_string(&catalog).unwrap();
    let roundtrip: SystemCatalog = serde_json::from_str(&serialized).unwrap();
    assert_eq!(catalog.hash().unwrap(), roundtrip.hash().unwrap());
    let mut revised = roundtrip;
    revised.categories[0].description.push_str(" Revised.");
    assert_ne!(catalog.hash().unwrap(), revised.hash().unwrap());
}
#[test]
fn folder_roots_cannot_escape_overlap_or_repoint_definition_identity() {
    for path in [
        "/tmp/private",
        "libraries/../secrets",
        "libraries//agents",
        "libraries/agents/",
        "libraries/agents\\outside",
        "libraries/agents\0",
    ] {
        let mut catalog = SystemCatalog::bundled().unwrap();
        catalog.categories[0].belongings_path = path.into();
        assert!(catalog.validate().is_err(), "accepted {path:?}");
    }
    let mut catalog = SystemCatalog::bundled().unwrap();
    catalog.categories[0].belongings_path =
        catalog.categories[1].belongings_path.clone() + "/nested";
    assert!(catalog.validate().is_err());
    let mut catalog = SystemCatalog::bundled().unwrap();
    catalog.categories[0].definition_path = "catalog/another-id".into();
    assert!(catalog.validate().is_err());
}
#[test]
fn duplicate_identity_unknown_lifecycle_and_unknown_relation_types_fail_closed() {
    let mut catalog = SystemCatalog::bundled().unwrap();
    catalog.entity_kinds[0].prefix = catalog.entity_kinds[1].prefix.clone();
    assert!(catalog.validate().is_err());
    let mut catalog = SystemCatalog::bundled().unwrap();
    catalog.categories.push(catalog.categories[0].clone());
    assert!(catalog.validate().is_err());
    let mut catalog = SystemCatalog::bundled().unwrap();
    catalog.entity_kinds[0].lifecycle = "unreviewed".into();
    assert!(catalog.validate().is_err());
    let mut catalog = SystemCatalog::bundled().unwrap();
    catalog.relations[0].targets.push("invented_kind".into());
    assert!(catalog.validate().is_err());
    let mut json = serde_json::to_value(SystemCatalog::bundled().unwrap()).unwrap();
    json["relations"][0]["graph"] = "unreviewed".into();
    assert!(serde_json::from_value::<SystemCatalog>(json).is_err());
}
#[test]
fn lifecycle_declarations_cannot_reopen_terminal_states_or_hide_dead_ends() {
    let mut catalog = SystemCatalog::bundled().unwrap();
    catalog
        .lifecycles
        .get_mut("execution")
        .unwrap()
        .transitions
        .push(["succeeded".into(), "running".into()]);
    assert!(catalog.validate().is_err());
    let mut catalog = SystemCatalog::bundled().unwrap();
    catalog
        .lifecycles
        .get_mut("execution")
        .unwrap()
        .states
        .push("orphan".into());
    assert!(catalog.validate().is_err());
    let mut catalog = SystemCatalog::bundled().unwrap();
    let lifecycle = catalog.lifecycles.get_mut("execution").unwrap();
    lifecycle.states.push("stuck".into());
    lifecycle
        .transitions
        .push(["running".into(), "stuck".into()]);
    assert!(catalog.validate().is_err());
}
#[test]
fn unsupported_catalogs_and_cycle_permitting_ancestry_are_rejected() {
    let mut catalog = SystemCatalog::bundled().unwrap();
    catalog.schema_version = 2;
    assert!(catalog.validate().is_err());
    let mut catalog = SystemCatalog::bundled().unwrap();
    catalog.provisioning = "automatic".into();
    assert!(catalog.validate().is_err());
    let mut catalog = SystemCatalog::bundled().unwrap();
    catalog.relations[0].acyclic = false;
    assert!(catalog.validate().is_err());
    let mut json = serde_json::to_value(SystemCatalog::bundled().unwrap()).unwrap();
    json["unknown_policy"] = true.into();
    assert!(serde_json::from_value::<SystemCatalog>(json).is_err());
}
