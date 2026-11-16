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
        let p = std::env::temp_dir().join(format!(
            "hotbox-task-tests-{}-{}",
            std::process::id(),
            NEXT.fetch_add(1, Ordering::SeqCst)
        ));
        fs::create_dir(&p).unwrap();
        Self(p)
    }
    fn engine(&self) -> Engine {
        Engine::open(self.0.join("core.sqlite3")).unwrap()
    }
}
impl Drop for Temp {
    fn drop(&mut self) {
        let _ = fs::remove_dir_all(&self.0);
    }
}
fn request(id: &str) -> NotesRequest {
    NotesRequest {
        id: id.into(),
        actor: "local-user".into(),
        resource: "workspace-1".into(),
        title: "Weekend project".into(),
        notes: "Measure the wall.\n\n- Reuse the lamp.\nMeasure the wall.\n".into(),
    }
}
fn complete(e: &mut Engine, id: &str) -> TaskView {
    let mut last = e.notes_task("local-user", id).unwrap();
    for _ in 0..4 {
        last = e.advance_notes("local-user", id).unwrap();
    }
    last
}
#[test]
fn local_recipe_survives_restart_and_saves_a_checked_private_result() {
    let t = Temp::new();
    let mut e = t.engine();
    e.start_notes(request("wrun_one")).unwrap();
    let first = e.advance_notes("local-user", "wrun_one").unwrap();
    assert_eq!(first.step, 1);
    drop(e);
    let mut e = t.engine();
    let last = complete(&mut e, "wrun_one");
    assert_eq!(last.status, TaskStatus::Complete);
    assert_eq!(last.step, 4);
    assert_eq!(last.note_count, 2);
    let path = last.result_path.unwrap();
    let content = fs::read_to_string(&path).unwrap();
    assert_eq!(
        content,
        "# Weekend project\n\n## Your notes\n\n- Measure the wall.\n- Reuse the lamp.\n\n---\n2 unique notes. Organized locally without AI.\n"
    );
    assert_eq!(content, last.preview);
    assert_eq!(e.advance_notes("local-user", "wrun_one").unwrap().step, 4);
    #[cfg(unix)]
    {
        use std::os::unix::fs::PermissionsExt;
        assert_eq!(
            fs::metadata(path).unwrap().permissions().mode() & 0o777,
            0o600
        );
    }
}
#[test]
fn pause_resume_and_cancel_are_durable_without_publishing_a_stopped_result() {
    let t = Temp::new();
    let mut e = t.engine();
    e.start_notes(request("wrun_one")).unwrap();
    e.advance_notes("local-user", "wrun_one").unwrap();
    e.control_notes("local-user", "wrun_one", "pause").unwrap();
    assert_eq!(e.advance_notes("local-user", "wrun_one").unwrap().step, 1);
    drop(e);
    let mut e = t.engine();
    assert_eq!(
        e.notes_task("local-user", "wrun_one").unwrap().status,
        TaskStatus::Paused
    );
    assert!(e.tick_notes("local-user").unwrap().is_none());
    e.control_notes("local-user", "wrun_one", "resume").unwrap();
    assert_eq!(e.tick_notes("local-user").unwrap().unwrap().step, 2);
    e.control_notes("local-user", "wrun_one", "cancel").unwrap();
    assert_eq!(
        e.advance_notes("local-user", "wrun_one").unwrap().status,
        TaskStatus::Cancelled
    );
    assert!(!t.0.join("task-results/wrun_one/notes.md").exists());
    assert!(e.control_notes("local-user", "wrun_one", "resume").is_err());
}
#[test]
fn repeated_requests_are_idempotent_and_cross_owner_reads_are_denied() {
    let t = Temp::new();
    let mut e = t.engine();
    let r = request("wrun_one");
    e.start_notes(r.clone()).unwrap();
    e.advance_notes("local-user", "wrun_one").unwrap();
    assert_eq!(e.start_notes(r.clone()).unwrap().step, 1);
    let mut changed = r.clone();
    changed.notes = "different".into();
    assert!(matches!(e.start_notes(changed), Err(Error::Conflict(_))));
    assert!(e.notes_task("other", "wrun_one").is_err());
    assert!(e.control_notes("other", "wrun_one", "cancel").is_err());
    let mut other = r;
    other.actor = "other".into();
    other.id = "another".into();
    assert!(matches!(e.start_notes(other), Err(Error::Denied(_))));
    assert!(e.notes_tasks("other", "").unwrap().is_empty());
}
#[test]
fn existing_output_is_preserved_and_a_new_task_can_run_after_failure() {
    let t = Temp::new();
    let mut e = t.engine();
    e.start_notes(request("wrun_one")).unwrap();
    e.advance_notes("local-user", "wrun_one").unwrap();
    let p = t.0.join("task-results/wrun_one/notes.md");
    fs::write(&p, "personal edits").unwrap();
    let last = complete(&mut e, "wrun_one");
    assert_eq!(last.status, TaskStatus::Failed);
    assert_eq!(fs::read_to_string(p).unwrap(), "personal edits");
    e.start_notes(request("wrun_two")).unwrap();
    assert_eq!(complete(&mut e, "wrun_two").status, TaskStatus::Complete);
}
#[test]
fn publication_recovers_a_saved_file_before_the_checkpoint_commit() {
    let t = Temp::new();
    let mut e = t.engine();
    e.start_notes(request("wrun_one")).unwrap();
    for _ in 0..3 {
        e.advance_notes("local-user", "wrun_one").unwrap();
    }
    let content = "# Weekend project\n\n## Your notes\n\n- Measure the wall.\n- Reuse the lamp.\n\n---\n2 unique notes. Organized locally without AI.\n";
    fs::write(t.0.join("task-results/wrun_one/notes.md"), content).unwrap();
    drop(e);
    let mut e = t.engine();
    assert_eq!(
        e.tick_notes("local-user").unwrap().unwrap().status,
        TaskStatus::Complete
    );
}
#[test]
fn queue_is_project_independent_and_list_payloads_do_not_include_note_content() {
    let t = Temp::new();
    let mut e = t.engine();
    let mut a = request("wrun_a");
    a.resource = "workspace-2".into();
    e.start_notes(a).unwrap();
    e.start_notes(request("wrun_b")).unwrap();
    for _ in 0..8 {
        e.tick_notes("local-user").unwrap();
    }
    assert!(e.tick_notes("local-user").unwrap().is_none());
    assert_eq!(e.notes_tasks("local-user", "").unwrap().len(), 2);
    assert_eq!(e.notes_tasks("local-user", "workspace-2").unwrap().len(), 1);
    assert!(
        e.notes_tasks("local-user", "")
            .unwrap()
            .iter()
            .all(|v| v.preview.is_empty())
    );
}
#[test]
fn schema_two_upgrades_without_losing_its_owner_registry() {
    let t = Temp::new();
    let mut e = t.engine();
    e.create_resource("alice", "original").unwrap();
    drop(e);
    let db = rusqlite::Connection::open(t.0.join("core.sqlite3")).unwrap();
    db.execute_batch("DROP TABLE local_tasks; PRAGMA user_version=2;")
        .unwrap();
    drop(db);
    let mut e = t.engine();
    e.start_notes(request("wrun_one")).unwrap();
    assert!(e.create_resource("mallory", "original").is_err());
    assert_eq!(complete(&mut e, "wrun_one").status, TaskStatus::Complete);
}
#[test]
fn oversized_and_empty_inputs_are_rejected_before_creating_tasks() {
    let t = Temp::new();
    let mut e = t.engine();
    for notes in [
        " ".to_string(),
        "x".repeat(65537),
        (0..501).map(|i| format!("{i}\n")).collect(),
    ] {
        let mut r = request("invalid");
        r.notes = notes;
        assert!(e.start_notes(r).is_err());
    }
    assert!(e.notes_tasks("local-user", "").unwrap().is_empty());
}
#[cfg(unix)]
#[test]
fn symlinked_storage_cannot_redirect_personal_results() {
    use std::os::unix::fs::symlink;
    let t = Temp::new();
    let outside = Temp::new();
    let mut e = t.engine();
    symlink(&outside.0, t.0.join("task-results")).unwrap();
    e.start_notes(request("wrun_one")).unwrap();
    assert_eq!(
        e.advance_notes("local-user", "wrun_one").unwrap().status,
        TaskStatus::Failed
    );
    assert_eq!(fs::read_dir(&outside.0).unwrap().count(), 0);
}

#[test]
fn a_changed_recipe_or_checked_digest_cannot_publish_after_restart() {
    for recipe_change in [true, false] {
        let t = Temp::new();
        let mut e = t.engine();
        e.start_notes(request("wrun_one")).unwrap();
        for _ in 0..3 {
            e.advance_notes("local-user", "wrun_one").unwrap();
        }
        drop(e);
        let db = rusqlite::Connection::open(t.0.join("core.sqlite3")).unwrap();
        let field = if recipe_change {
            "$.recipe_version"
        } else {
            "$.result_digest"
        };
        db.execute(
            "UPDATE local_tasks SET document=json_set(document,?1,'changed') WHERE id='wrun_one'",
            [field],
        )
        .unwrap();
        drop(db);
        let mut e = t.engine();
        assert_eq!(
            e.advance_notes("local-user", "wrun_one").unwrap().status,
            TaskStatus::Failed
        );
        assert!(!t.0.join("task-results/wrun_one/notes.md").exists());
    }
}
