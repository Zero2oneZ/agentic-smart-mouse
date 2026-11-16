use hotbox_core::*;
use std::{
    fs,
    path::PathBuf,
    sync::atomic::{AtomicU64, Ordering},
};

struct Temp(PathBuf);
impl Temp {
    fn new() -> Self {
        static NEXT: AtomicU64 = AtomicU64::new(0);
        let path = std::env::temp_dir().join(format!(
            "hotbox-tool-menu-{}-{}",
            std::process::id(),
            NEXT.fetch_add(1, Ordering::SeqCst)
        ));
        fs::create_dir(&path).unwrap();
        Self(path)
    }
}
impl Drop for Temp {
    fn drop(&mut self) {
        let _ = fs::remove_dir_all(&self.0);
    }
}
fn registration() -> ToolMenuRegistration {
    ToolMenuRegistration {
        schema_version: 1,
        request_id: "create-1".into(),
        actor: "alice".into(),
        tool_id: "formatter".into(),
        resource: "project".into(),
        name: "Brief formatter".into(),
        purpose: "Format fixture notes".into(),
        menu_category: "Writing".into(),
        originating_chat: "qt-install-chat".into(),
        initial: None,
    }
}
fn update() -> ToolMenuUpdate {
    ToolMenuUpdate {
        schema_version: 1,
        request_id: "rename-1".into(),
        actor: "alice".into(),
        tool_id: "formatter".into(),
        expected_revision: 1,
        name: "Brief writer".into(),
        purpose: "Format fixture notes".into(),
        menu_category: "Editing".into(),
    }
}

#[test]
fn registration_replays_without_new_tools_and_survives_restart() {
    let root = Temp::new();
    let path = root.0.join("core.sqlite3");
    let mut engine = Engine::open(&path).unwrap();
    engine.create_resource("alice", "project").unwrap();
    let first = engine.register_tool_menu(registration()).unwrap();
    assert!(first.selected.is_none());
    assert_eq!(engine.register_tool_menu(registration()).unwrap(), first);
    let mut changed = registration();
    changed.name = "Different".into();
    assert!(matches!(
        engine.register_tool_menu(changed),
        Err(Error::Conflict(_))
    ));
    drop(engine);
    let engine = Engine::open(&path).unwrap();
    assert_eq!(engine.read_tool_menu("alice", "formatter").unwrap(), first);
    assert_eq!(
        engine.list_tool_menu("alice", "project", "").unwrap(),
        vec![first]
    );
    assert!(
        engine
            .list_tool_menu("alice", "project", "formatter")
            .unwrap()
            .is_empty()
    );
    let db = rusqlite::Connection::open(path).unwrap();
    assert_eq!(
        db.query_row("SELECT COUNT(*) FROM tools", [], |r| r.get::<_, i64>(0))
            .unwrap(),
        1
    );
    assert_eq!(
        db.query_row("SELECT COUNT(*) FROM tool_menu_requests", [], |r| r
            .get::<_, i64>(0))
            .unwrap(),
        1
    );
}

#[test]
fn metadata_revisions_preserve_context_and_replay_does_not_overwrite_newer_edits() {
    let root = Temp::new();
    let mut engine = Engine::open(root.0.join("core.sqlite3")).unwrap();
    engine.create_resource("alice", "project").unwrap();
    engine.register_tool_menu(registration()).unwrap();
    let second = engine.update_tool_menu(update()).unwrap();
    assert_eq!(second.revision, 2);
    assert_eq!(second.originating_chat, "qt-install-chat");
    let mut third = update();
    third.request_id = "rename-2".into();
    third.expected_revision = 2;
    third.name = "Newest".into();
    assert_eq!(engine.update_tool_menu(third).unwrap().revision, 3);
    assert_eq!(engine.update_tool_menu(update()).unwrap(), second);
    assert_eq!(
        engine.read_tool_menu("alice", "formatter").unwrap().name,
        "Newest"
    );
    let mut stale = update();
    stale.request_id = "stale".into();
    assert!(matches!(
        engine.update_tool_menu(stale),
        Err(Error::Conflict(_))
    ));
    assert_eq!(
        engine.register_tool_menu(registration()).unwrap().revision,
        1
    );
    assert_eq!(
        engine
            .read_tool_menu("alice", "formatter")
            .unwrap()
            .revision,
        3
    );
}

#[test]
fn another_owner_invalid_metadata_and_version_context_changes_are_rejected() {
    let root = Temp::new();
    let mut engine = Engine::open(root.0.join("core.sqlite3")).unwrap();
    engine.create_resource("alice", "project").unwrap();
    let mut denied = registration();
    denied.actor = "bob".into();
    assert!(matches!(
        engine.register_tool_menu(denied),
        Err(Error::Denied(_))
    ));
    let mut invalid = registration();
    invalid.menu_category = "Bad\nCategory".into();
    assert!(engine.register_tool_menu(invalid).is_err());
    let mut mismatched = registration();
    mismatched.initial = Some(ToolVersion {
        version: "v1".into(),
        source_commit: "a".repeat(40),
        architecture_version: "arch-v1".into(),
        originating_chat: "other-chat".into(),
    });
    assert!(engine.register_tool_menu(mismatched).is_err());
    engine.register_tool_menu(registration()).unwrap();
    assert!(matches!(
        engine.read_tool_menu("bob", "formatter"),
        Err(Error::Denied(_))
    ));
    assert!(matches!(
        engine.list_tool_menu("bob", "project", ""),
        Err(Error::Denied(_))
    ));
    let mut denied = update();
    denied.actor = "bob".into();
    assert!(matches!(
        engine.update_tool_menu(denied),
        Err(Error::Denied(_))
    ));
    assert_eq!(
        engine
            .read_tool_menu("alice", "formatter")
            .unwrap()
            .revision,
        1
    );
}

#[test]
fn existing_immutable_tool_version_can_gain_menu_metadata_without_new_identity() {
    let root = Temp::new();
    let mut engine = Engine::open(root.0.join("core.sqlite3")).unwrap();
    engine.create_resource("alice", "project").unwrap();
    let version = ToolVersion {
        version: "v1".into(),
        source_commit: "b".repeat(40),
        architecture_version: "arch-v1".into(),
        originating_chat: "qt-install-chat".into(),
    };
    engine
        .register_tool(RegisterTool {
            actor: "alice".into(),
            tool_id: "formatter".into(),
            resource: "project".into(),
            initial: version.clone(),
        })
        .unwrap();
    assert!(engine.register_tool_menu(registration()).is_err());
    let mut request = registration();
    request.initial = Some(version.clone());
    let first = engine.register_tool_menu(request.clone()).unwrap();
    assert_eq!(first.selected, Some(version));
    assert_eq!(engine.register_tool_menu(request).unwrap(), first);
    assert_eq!(
        engine.update_tool_menu(update()).unwrap().selected,
        first.selected
    );
}

#[test]
fn schema_three_upgrade_preserves_lineage_and_catalog_never_enables_runtime() {
    let root = Temp::new();
    let path = root.0.join("core.sqlite3");
    let mut engine = Engine::open(&path).unwrap();
    engine.create_resource("alice", "project").unwrap();
    drop(engine);
    let db = rusqlite::Connection::open(&path).unwrap();
    db.execute_batch("DROP TABLE tool_menu_requests; DROP TABLE tool_menu; PRAGMA user_version=3;")
        .unwrap();
    drop(db);
    let mut engine = Engine::open(&path).unwrap();
    engine.register_tool_menu(registration()).unwrap();
    let catalog = tool_menu_catalog();
    assert_eq!(catalog["actions"][1]["id"], "run");
    assert_eq!(catalog["actions"][1]["available"], false);
    let db = rusqlite::Connection::open(&path).unwrap();
    assert_eq!(
        db.query_row("PRAGMA user_version", [], |r| r.get::<_, i64>(0))
            .unwrap(),
        4
    );
    assert_eq!(
        db.query_row("SELECT COUNT(*) FROM runs", [], |r| r.get::<_, i64>(0))
            .unwrap(),
        0
    );
}
