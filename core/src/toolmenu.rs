//! Owner-scoped draft discovery. Menu metadata never grants execution authority.
use crate::{Engine, Error, Result, ToolVersion, engine::event, lineage::owns, model::identifier};
use rusqlite::{Connection, OptionalExtension, TransactionBehavior, params};
use serde::{Deserialize, Serialize};
use serde_json::{Value, json};

pub fn tool_menu_catalog() -> Value {
    serde_json::from_str(include_str!("../templates/tool-menu-api-v1.json"))
        .expect("bundled tool menu catalog")
}

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct ToolMenuEntry {
    pub tool_id: String,
    pub resource: String,
    pub name: String,
    pub purpose: String,
    pub menu_category: String,
    pub originating_chat: String,
    pub revision: u64,
    pub selected: Option<ToolVersion>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct ToolMenuRegistration {
    pub schema_version: u32,
    pub request_id: String,
    pub actor: String,
    pub tool_id: String,
    pub resource: String,
    pub name: String,
    pub purpose: String,
    pub menu_category: String,
    pub originating_chat: String,
    pub initial: Option<ToolVersion>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct ToolMenuUpdate {
    pub schema_version: u32,
    pub request_id: String,
    pub actor: String,
    pub tool_id: String,
    pub expected_revision: u64,
    pub name: String,
    pub purpose: String,
    pub menu_category: String,
}

fn labels(name: &str, purpose: &str, category: &str) -> Result<()> {
    if name.trim().is_empty()
        || name.len() > 200
        || purpose.len() > 4000
        || category.trim().is_empty()
        || category.len() > 100
        || name.chars().chain(category.chars()).any(char::is_control)
    {
        return Err(Error::Invalid(
            "Supply a name (200 bytes), purpose (4000 bytes) and category (100 bytes)".into(),
        ));
    }
    Ok(())
}

fn read(db: &Connection, actor: &str, tool: &str) -> Result<ToolMenuEntry> {
    let doc: String = db
        .query_row(
            "SELECT document FROM tool_menu WHERE tool_id=?",
            [tool],
            |r| r.get(0),
        )
        .optional()?
        .ok_or_else(|| Error::NotFound("tool menu entry".into()))?;
    let entry: ToolMenuEntry = serde_json::from_str(&doc)?;
    owns(db, actor, &entry.resource)?;
    Ok(entry)
}

fn replay(
    db: &Connection,
    actor: &str,
    id: &str,
    request: &Value,
) -> Result<Option<ToolMenuEntry>> {
    let saved: Option<(String, String, String)> = db
        .query_row(
            "SELECT actor,request,response FROM tool_menu_requests WHERE id=?",
            [id],
            |r| Ok((r.get(0)?, r.get(1)?, r.get(2)?)),
        )
        .optional()?;
    if let Some((owner, document, response)) = saved {
        if actor != owner || serde_json::from_str::<Value>(&document)? != *request {
            return Err(Error::Conflict(
                "request ID already used with different input".into(),
            ));
        }
        let entry: ToolMenuEntry = serde_json::from_str(&response)?;
        owns(db, actor, &entry.resource)?;
        return Ok(Some(entry));
    }
    Ok(None)
}

fn store(
    db: &Connection,
    actor: &str,
    id: &str,
    request: &Value,
    entry: &ToolMenuEntry,
) -> Result<()> {
    db.execute("INSERT INTO tool_menu(tool_id,document) VALUES(?,?) ON CONFLICT(tool_id) DO UPDATE SET document=excluded.document",
        params![entry.tool_id, serde_json::to_string(entry)?])?;
    db.execute(
        "INSERT INTO tool_menu_requests(id,actor,request,response) VALUES(?,?,?,?)",
        params![
            id,
            actor,
            request.to_string(),
            serde_json::to_string(entry)?
        ],
    )?;
    Ok(())
}

impl Engine {
    pub fn register_tool_menu(&mut self, request: ToolMenuRegistration) -> Result<ToolMenuEntry> {
        if request.schema_version != 1 {
            return Err(Error::Invalid("unsupported tool menu schema".into()));
        }
        for id in [
            &request.request_id,
            &request.actor,
            &request.tool_id,
            &request.resource,
            &request.originating_chat,
        ] {
            identifier(id)?;
        }
        labels(&request.name, &request.purpose, &request.menu_category)?;
        if let Some(version) = &request.initial {
            version.validate()?;
            if version.originating_chat != request.originating_chat {
                return Err(Error::Invalid(
                    "source version and development context differ".into(),
                ));
            }
        }
        let now = self.now();
        let tx = self
            .db
            .transaction_with_behavior(TransactionBehavior::Immediate)?;
        let document = json!({"operation":"register","request":request});
        owns(&tx, &request.actor, &request.resource)?;
        if let Some(entry) = replay(&tx, &request.actor, &request.request_id, &document)? {
            return Ok(entry);
        }
        let exists: bool = tx.query_row(
            "SELECT EXISTS(SELECT 1 FROM tools WHERE id=?)",
            [&request.tool_id],
            |r| r.get(0),
        )?;
        if exists {
            let already: bool = tx.query_row(
                "SELECT EXISTS(SELECT 1 FROM tool_menu WHERE tool_id=?)",
                [&request.tool_id],
                |r| r.get(0),
            )?;
            let resource: String = tx.query_row(
                "SELECT resource FROM tools WHERE id=?",
                [&request.tool_id],
                |r| r.get(0),
            )?;
            let initial = request.initial.as_ref().ok_or_else(|| {
                Error::Conflict("Existing tools require their exact immutable version".into())
            })?;
            if already
                || resource != request.resource
                || crate::lineage::version(&tx, &request.tool_id, &initial.version)? != *initial
            {
                return Err(Error::Conflict(
                    "Tool already registered or source/context differs".into(),
                ));
            }
        } else {
            tx.execute(
                "INSERT INTO tools(id,resource) VALUES(?,?)",
                params![request.tool_id, request.resource],
            )?;
            if let Some(version) = &request.initial {
                tx.execute(
                    "INSERT INTO tool_versions(tool_id,version,document) VALUES(?,?,?)",
                    params![
                        request.tool_id,
                        version.version,
                        serde_json::to_string(version)?
                    ],
                )?;
            }
        }
        let entry = ToolMenuEntry {
            tool_id: request.tool_id.clone(),
            resource: request.resource.clone(),
            name: request.name.trim().into(),
            purpose: request.purpose.clone(),
            menu_category: request.menu_category.trim().into(),
            originating_chat: request.originating_chat.clone(),
            revision: 1,
            selected: request.initial.clone(),
        };
        store(&tx, &request.actor, &request.request_id, &document, &entry)?;
        event(
            &tx,
            None,
            &request.actor,
            "tool.menu.registered",
            &json!({"tool_id":entry.tool_id,"request_id":request.request_id}),
            now,
        )?;
        tx.commit()?;
        Ok(entry)
    }

    pub fn update_tool_menu(&mut self, request: ToolMenuUpdate) -> Result<ToolMenuEntry> {
        if request.schema_version != 1 {
            return Err(Error::Invalid("unsupported tool menu schema".into()));
        }
        for id in [&request.request_id, &request.actor, &request.tool_id] {
            identifier(id)?;
        }
        labels(&request.name, &request.purpose, &request.menu_category)?;
        let now = self.now();
        let tx = self
            .db
            .transaction_with_behavior(TransactionBehavior::Immediate)?;
        let mut entry = read(&tx, &request.actor, &request.tool_id)?;
        let document = json!({"operation":"update","request":request});
        if let Some(saved) = replay(&tx, &request.actor, &request.request_id, &document)? {
            return Ok(saved);
        }
        if entry.revision != request.expected_revision {
            return Err(Error::Conflict(
                "tool metadata changed; reload before editing".into(),
            ));
        }
        entry.name = request.name.trim().into();
        entry.purpose = request.purpose.clone();
        entry.menu_category = request.menu_category.trim().into();
        entry.revision = entry
            .revision
            .checked_add(1)
            .ok_or_else(|| Error::Conflict("revision exhausted".into()))?;
        store(&tx, &request.actor, &request.request_id, &document, &entry)?;
        event(
            &tx,
            None,
            &request.actor,
            "tool.menu.updated",
            &json!({"tool_id":entry.tool_id,"request_id":request.request_id,"revision":entry.revision}),
            now,
        )?;
        tx.commit()?;
        Ok(entry)
    }

    pub fn read_tool_menu(&self, actor: &str, tool: &str) -> Result<ToolMenuEntry> {
        identifier(actor)?;
        identifier(tool)?;
        let tx = self.db.unchecked_transaction()?;
        let entry = read(&tx, actor, tool)?;
        tx.commit()?;
        Ok(entry)
    }

    pub fn list_tool_menu(
        &self,
        actor: &str,
        resource: &str,
        after: &str,
    ) -> Result<Vec<ToolMenuEntry>> {
        identifier(actor)?;
        identifier(resource)?;
        if !after.is_empty() {
            identifier(after)?;
        }
        let tx = self.db.unchecked_transaction()?;
        owns(&tx, actor, resource)?;
        let mut query = tx.prepare("SELECT m.document FROM tool_menu m JOIN tools t ON t.id=m.tool_id WHERE t.resource=? AND t.id>? ORDER BY t.id LIMIT 100")?;
        let documents = query
            .query_map(params![resource, after], |r| r.get::<_, String>(0))?
            .collect::<std::result::Result<Vec<_>, _>>()?;
        let entries = documents
            .iter()
            .map(|doc| serde_json::from_str(doc).map_err(Error::from))
            .collect::<Result<Vec<_>>>()?;
        drop(query);
        tx.commit()?;
        Ok(entries)
    }
}
