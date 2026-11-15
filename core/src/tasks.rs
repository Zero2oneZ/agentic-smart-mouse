//! Bounded local notes recipe. No models, network, shell, or user-file mutation.
use crate::{
    Engine, Error, Result,
    engine::event,
    model::{digest, identifier},
};
use rusqlite::{Connection, OptionalExtension, TransactionBehavior, params};
use serde::{Deserialize, Serialize};
use serde_json::json;
use sha2::{Digest, Sha256};
use std::{
    fs,
    io::Write,
    path::{Path, PathBuf},
};

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct NotesRequest {
    pub id: String,
    pub actor: String,
    pub resource: String,
    pub title: String,
    pub notes: String,
}
#[derive(Debug, Clone, Copy, PartialEq, Eq, Serialize, Deserialize)]
#[serde(rename_all = "snake_case")]
pub enum TaskStatus {
    Queued,
    Working,
    Paused,
    Complete,
    Cancelled,
    Failed,
}
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct NotesTask {
    pub recipe_version: String,
    pub request: NotesRequest,
    pub status: TaskStatus,
    pub step: u32,
    pub created_at: i64,
    pub updated_at: i64,
    pub error: String,
    pub result_path: Option<String>,
    pub result_digest: Option<String>,
}
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct TaskView {
    pub id: String,
    pub title: String,
    pub resource: String,
    pub status: TaskStatus,
    pub step: u32,
    pub total_steps: u32,
    pub created_at: i64,
    pub updated_at: i64,
    pub error: String,
    pub result_path: Option<String>,
    pub preview: String,
    pub note_count: usize,
}
impl NotesTask {
    fn view(&self) -> TaskView {
        TaskView {
            id: self.request.id.clone(),
            title: self.request.title.clone(),
            resource: self.request.resource.clone(),
            status: self.status,
            step: self.step,
            total_steps: 4,
            created_at: self.created_at,
            updated_at: self.updated_at,
            error: self.error.clone(),
            result_path: self.result_path.clone(),
            preview: if self.status == TaskStatus::Complete {
                report(&self.request)
            } else {
                String::new()
            },
            note_count: lines(&self.request.notes).len(),
        }
    }
}
fn lines(notes: &str) -> Vec<String> {
    let mut seen = std::collections::BTreeSet::new();
    notes
        .lines()
        .map(|l| l.trim().trim_start_matches(['-', '*', '•']).trim())
        .filter(|l| !l.is_empty())
        .filter_map(|l| {
            if seen.insert(l.to_owned()) {
                Some(l.to_owned())
            } else {
                None
            }
        })
        .collect()
}
fn report(request: &NotesRequest) -> String {
    let items = lines(&request.notes);
    let mut text = format!("# {}\n\n## Your notes\n\n", request.title);
    for line in &items {
        text.push_str(&format!("- {line}\n"));
    }
    text.push_str(&format!(
        "\n---\n{} unique notes. Organized locally without AI.\n",
        items.len()
    ));
    text
}
fn load(db: &Connection, id: &str) -> Result<NotesTask> {
    let doc: String = db
        .query_row("SELECT document FROM local_tasks WHERE id=?1", [id], |r| {
            r.get(0)
        })
        .optional()?
        .ok_or_else(|| Error::NotFound("task".into()))?;
    Ok(serde_json::from_str(&doc)?)
}
fn owns(db: &Connection, actor: &str, task: &NotesTask) -> Result<()> {
    let owner: String = db.query_row(
        "SELECT owner FROM resources WHERE id=?1",
        [&task.request.resource],
        |r| r.get(0),
    )?;
    if task.request.actor != actor || owner != actor {
        return Err(Error::Denied("task belongs to another owner".into()));
    }
    Ok(())
}
fn save(db: &Connection, task: &NotesTask) -> Result<()> {
    db.execute(
        "UPDATE local_tasks SET document=?2 WHERE id=?1",
        params![task.request.id, serde_json::to_string(task)?],
    )?;
    Ok(())
}
fn safe_directory(path: &Path) -> Result<()> {
    let mut part = PathBuf::new();
    for component in path.components() {
        part.push(component);
        match fs::symlink_metadata(&part) {
            Ok(meta) if meta.file_type().is_symlink() || !meta.is_dir() => {
                return Err(Error::Invalid(
                    "Task storage contains a link or non-directory. Choose a private data folder."
                        .into(),
                ));
            }
            Ok(_) => (),
            Err(e) if e.kind() == std::io::ErrorKind::NotFound => {
                fs::create_dir(&part)?;
                #[cfg(unix)]
                {
                    use std::os::unix::fs::PermissionsExt;
                    fs::set_permissions(&part, fs::Permissions::from_mode(0o700))?;
                }
            }
            Err(e) => return Err(e.into()),
        }
    }
    Ok(())
}
fn destination(db: &Connection, id: &str) -> Result<PathBuf> {
    let parent = Path::new(
        db.path()
            .ok_or_else(|| Error::Invalid("Tasks need a private on-disk database".into()))?,
    )
    .parent()
    .ok_or_else(|| Error::Invalid("Missing data folder".into()))?;
    if parent
        .ancestors()
        .any(|p| p.join(".git").is_file() || p.join(".git/HEAD").is_file())
    {
        return Err(Error::Invalid(
            "Save personal task results outside source repositories.".into(),
        ));
    }
    let folder = parent.join("task-results").join(id);
    safe_directory(&folder)?;
    Ok(folder.join("notes.md"))
}
fn publish(path: &Path, bytes: &[u8]) -> Result<()> {
    // Publication is create-only. An interrupted write is recoverable via the .pending file.
    let pending = path.with_extension("pending");
    if path.exists() || fs::symlink_metadata(path).is_ok() {
        let meta = fs::symlink_metadata(path)?;
        if meta.file_type().is_symlink()
            || !meta.is_file()
            || meta.len() > 200_000
            || fs::read(path)? != bytes
        {
            return Err(Error::Conflict(
                "The result file changed. Your file was kept; start a new task.".into(),
            ));
        }
        return Ok(());
    }
    // Only the trusted private task directory may contain a recoverable staging file.
    if let Ok(meta) = fs::symlink_metadata(&pending) {
        if meta.file_type().is_symlink() || !meta.is_file() {
            return Err(Error::Invalid("Task staging file is unsafe".into()));
        }
        fs::remove_file(&pending)?;
    }
    let mut options = fs::OpenOptions::new();
    options.write(true).create_new(true);
    #[cfg(unix)]
    {
        use std::os::unix::fs::OpenOptionsExt;
        options.mode(0o600);
    }
    let mut file = options.open(&pending)?;
    file.write_all(bytes)?;
    file.sync_all()?;
    // hard_link publishes atomically without replacing an existing destination.
    fs::hard_link(&pending, path)?;
    fs::remove_file(&pending)?;
    fs::File::open(path.parent().expect("parent directory"))?.sync_all()?;
    Ok(())
}
impl Engine {
    pub fn start_notes(&mut self, request: NotesRequest) -> Result<TaskView> {
        for id in [&request.id, &request.actor, &request.resource] {
            identifier(id)?;
        }
        if request.title.trim().is_empty()
            || request.title.chars().count() > 100
            || request.title.chars().any(char::is_control)
            || request.notes.len() > 65536
            || lines(&request.notes).is_empty()
            || lines(&request.notes).len() > 500
        {
            return Err(Error::Invalid(
                "Add a title up to 100 characters and 1–500 notes (up to 64 KiB).".into(),
            ));
        }
        let now = self.now();
        let tx = self
            .db
            .transaction_with_behavior(TransactionBehavior::Immediate)?;
        tx.execute(
            "INSERT OR IGNORE INTO resources(id,owner) VALUES(?1,?2)",
            params![request.resource, request.actor],
        )?;
        let owner: String = tx.query_row(
            "SELECT owner FROM resources WHERE id=?1",
            [&request.resource],
            |r| r.get(0),
        )?;
        if owner != request.actor {
            return Err(Error::Denied("project belongs to another owner".into()));
        }
        if let Some(doc) = tx
            .query_row(
                "SELECT document FROM local_tasks WHERE id=?1",
                [&request.id],
                |r| r.get::<_, String>(0),
            )
            .optional()?
        {
            let task: NotesTask = serde_json::from_str(&doc)?;
            if task.request != request {
                return Err(Error::Conflict(
                    "task reference already used for a different request".into(),
                ));
            }
            return Ok(task.view());
        }
        let task = NotesTask {
            recipe_version: "organize-notes-v1".into(),
            request,
            status: TaskStatus::Queued,
            step: 0,
            created_at: now,
            updated_at: now,
            error: String::new(),
            result_path: None,
            result_digest: None,
        };
        tx.execute(
            "INSERT INTO local_tasks(id,document) VALUES(?1,?2)",
            params![task.request.id, serde_json::to_string(&task)?],
        )?;
        event(
            &tx,
            None,
            &task.request.actor,
            "task.created",
            &json!({"task":task.request.id,"recipe":"organize-notes-v1","input_hash":digest(&task.request)?}),
            now,
        )?;
        tx.commit()?;
        Ok(task.view())
    }
    pub fn notes_tasks(&self, actor: &str, resource: &str) -> Result<Vec<TaskView>> {
        identifier(actor)?;
        if !resource.is_empty() {
            identifier(resource)?;
        }
        let mut statement=self.db.prepare("SELECT document FROM local_tasks WHERE json_extract(document,'$.request.actor')=?1 AND (?2='' OR json_extract(document,'$.request.resource')=?2) ORDER BY CASE WHEN json_extract(document,'$.status') IN ('queued','working') THEN 0 ELSE 1 END,json_extract(document,'$.created_at') DESC,id DESC LIMIT 50")?;
        statement
            .query_map(params![actor, resource], |r| r.get::<_, String>(0))?
            .map(|row| {
                let task: NotesTask = serde_json::from_str(&row?)?;
                owns(&self.db, actor, &task)?;
                let mut view = task.view();
                view.preview.clear();
                Ok(view)
            })
            .collect()
    }
    pub fn tick_notes(&mut self, actor: &str) -> Result<Option<TaskView>> {
        identifier(actor)?;
        let id: Option<String> = self.db.query_row("SELECT id FROM local_tasks WHERE json_extract(document,'$.request.actor')=?1 AND json_extract(document,'$.status') IN ('queued','working') ORDER BY json_extract(document,'$.created_at'),id LIMIT 1", [actor], |r| r.get(0)).optional()?;
        id.map(|id| self.advance_notes(actor, &id)).transpose()
    }
    pub fn notes_task(&self, actor: &str, id: &str) -> Result<TaskView> {
        let task = load(&self.db, id)?;
        owns(&self.db, actor, &task)?;
        Ok(task.view())
    }
    pub fn control_notes(&mut self, actor: &str, id: &str, action: &str) -> Result<TaskView> {
        let now = self.now();
        let tx = self
            .db
            .transaction_with_behavior(TransactionBehavior::Immediate)?;
        let mut task = load(&tx, id)?;
        owns(&tx, actor, &task)?;
        let terminal = matches!(
            task.status,
            TaskStatus::Complete | TaskStatus::Cancelled | TaskStatus::Failed
        );
        match action {
            "pause" if !terminal => task.status = TaskStatus::Paused,
            "resume" if task.status == TaskStatus::Paused => task.status = TaskStatus::Queued,
            "cancel" if !terminal => task.status = TaskStatus::Cancelled,
            _ => {
                return Err(Error::Conflict(
                    "This task cannot perform that action in its current state.".into(),
                ));
            }
        }
        task.updated_at = now;
        save(&tx, &task)?;
        event(
            &tx,
            None,
            actor,
            "task.control",
            &json!({"task":id,"action":action}),
            now,
        )?;
        tx.commit()?;
        Ok(task.view())
    }
    pub fn advance_notes(&mut self, actor: &str, id: &str) -> Result<TaskView> {
        let now = self.now();
        let tx = self
            .db
            .transaction_with_behavior(TransactionBehavior::Immediate)?;
        let mut task = load(&tx, id)?;
        owns(&tx, actor, &task)?;
        if !matches!(task.status, TaskStatus::Queued | TaskStatus::Working) {
            return Ok(task.view());
        }
        task.status = TaskStatus::Working;
        let step_result = (|| -> Result<()> {
            if task.recipe_version != "organize-notes-v1" {
                return Err(Error::Conflict(
                    "This task needs the recipe version that created it. Start a new task.".into(),
                ));
            }
            match task.step {
                0 => {
                    destination(&tx, id)?;
                }
                1 => {
                    let output = report(&task.request);
                    task.result_digest = Some(format!("{:x}", Sha256::digest(output.as_bytes())));
                }
                2 => {
                    let output = report(&task.request);
                    if output.len() > 100_000
                        || task.result_digest.as_deref()
                            != Some(format!("{:x}", Sha256::digest(output.as_bytes())).as_str())
                        || lines(&task.request.notes)
                            .iter()
                            .any(|l| !output.contains(l))
                    {
                        return Err(Error::Invalid("The result did not pass its checks".into()));
                    }
                }
                3 => {
                    let output = report(&task.request);
                    let hash = format!("{:x}", Sha256::digest(output.as_bytes()));
                    if task.result_digest.as_deref() != Some(hash.as_str()) {
                        return Err(Error::Conflict(
                            "The checked result changed before saving. Start a new task.".into(),
                        ));
                    }
                    let path = destination(&tx, id)?;
                    publish(&path, report(&task.request).as_bytes())?;
                    task.result_path = Some(path.to_string_lossy().into_owned());
                    task.status = TaskStatus::Complete;
                }
                _ => return Err(Error::Invalid("Unknown task checkpoint".into())),
            }
            task.step += 1;
            Ok(())
        })();
        if let Err(error) = step_result {
            task.status = TaskStatus::Failed;
            task.error = error.to_string();
        }
        task.updated_at = now;
        save(&tx, &task)?;
        event(
            &tx,
            None,
            actor,
            "task.step",
            &json!({"task":id,"step":task.step,"status":task.status}),
            now,
        )?;
        tx.commit()?;
        Ok(task.view())
    }
}
