//! Immutable tool metadata. This registry neither executes Git nor copies runtime bindings.
use crate::{Engine, Error, Result, engine::event, model::identifier, permissions};
use rusqlite::{Connection, OptionalExtension, TransactionBehavior, params};
use serde::{Deserialize, Serialize};
use serde_json::json;

#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct ToolVersion {
    pub version: String,
    /// Full lowercase Git object ID, never a movable branch name.
    pub source_commit: String,
    pub architecture_version: String,
    pub originating_chat: String,
}
impl ToolVersion {
    fn validate(&self) -> Result<()> {
        for id in [
            &self.version,
            &self.architecture_version,
            &self.originating_chat,
        ] {
            identifier(id)?;
        }
        if ![40, 64].contains(&self.source_commit.len())
            || !self
                .source_commit
                .bytes()
                .all(|b| b.is_ascii_digit() || (b'a'..=b'f').contains(&b))
        {
            return Err(Error::Invalid(
                "source_commit must be a full lowercase Git object ID".into(),
            ));
        }
        Ok(())
    }
}
#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct ToolOrigin {
    pub tool_id: String,
    pub version: String,
}
#[derive(Debug, Clone, PartialEq, Eq, Serialize, Deserialize)]
pub struct ToolContext {
    pub tool_id: String,
    pub resource: String,
    pub forked_from: Option<ToolOrigin>,
    pub selected: ToolVersion,
}
#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct RegisterTool {
    pub actor: String,
    pub tool_id: String,
    pub resource: String,
    pub initial: ToolVersion,
}
#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct ForkTool {
    pub actor: String,
    pub grant_id: String,
    pub source: ToolOrigin,
    pub tool_id: String,
    pub resource: String,
    pub originating_chat: String,
}
fn resource(db: &Connection, tool: &str) -> Result<String> {
    db.query_row("SELECT resource FROM tools WHERE id=?1", [tool], |r| {
        r.get(0)
    })
    .optional()?
    .ok_or_else(|| Error::NotFound("tool".into()))
}
fn owns(db: &Connection, actor: &str, resource: &str) -> Result<()> {
    let owner: String = db
        .query_row("SELECT owner FROM resources WHERE id=?1", [resource], |r| {
            r.get(0)
        })
        .optional()?
        .ok_or_else(|| Error::NotFound("resource".into()))?;
    if actor != owner {
        return Err(Error::Denied(
            "tool mutation requires resource ownership".into(),
        ));
    }
    Ok(())
}
fn version(db: &Connection, tool: &str, selected: &str) -> Result<ToolVersion> {
    let doc: String = db
        .query_row(
            "SELECT document FROM tool_versions WHERE tool_id=?1 AND version=?2",
            params![tool, selected],
            |r| r.get(0),
        )
        .optional()?
        .ok_or_else(|| Error::NotFound("tool version".into()))?;
    Ok(serde_json::from_str(&doc)?)
}
fn insert_version(db: &Connection, tool: &str, value: &ToolVersion) -> Result<()> {
    let existing: Option<String> = db
        .query_row(
            "SELECT document FROM tool_versions WHERE tool_id=?1 AND version=?2",
            params![tool, value.version],
            |r| r.get(0),
        )
        .optional()?;
    if let Some(doc) = existing {
        if serde_json::from_str::<ToolVersion>(&doc)? == *value {
            return Ok(());
        }
        return Err(Error::Conflict(
            "tool versions are immutable; choose a new version".into(),
        ));
    }
    db.execute(
        "INSERT INTO tool_versions(tool_id,version,document) VALUES(?1,?2,?3)",
        params![tool, value.version, serde_json::to_string(value)?],
    )?;
    Ok(())
}
impl Engine {
    pub fn register_tool(&mut self, request: RegisterTool) -> Result<ToolContext> {
        for id in [&request.actor, &request.tool_id, &request.resource] {
            identifier(id)?;
        }
        request.initial.validate()?;
        let now = self.now();
        let tx = self
            .db
            .transaction_with_behavior(TransactionBehavior::Immediate)?;
        owns(&tx, &request.actor, &request.resource)?;
        let exists: bool = tx.query_row(
            "SELECT EXISTS(SELECT 1 FROM tools WHERE id=?1)",
            [&request.tool_id],
            |r| r.get(0),
        )?;
        if exists {
            return Err(Error::Conflict("tool ID already exists".into()));
        }
        tx.execute(
            "INSERT INTO tools(id,resource) VALUES(?1,?2)",
            params![request.tool_id, request.resource],
        )?;
        insert_version(&tx, &request.tool_id, &request.initial)?;
        event(
            &tx,
            None,
            &request.actor,
            "tool_registered",
            &json!(request),
            now,
        )?;
        tx.commit()?;
        Ok(ToolContext {
            tool_id: request.tool_id,
            resource: request.resource,
            forked_from: None,
            selected: request.initial,
        })
    }
    pub fn add_tool_version(&mut self, actor: &str, tool: &str, value: ToolVersion) -> Result<()> {
        identifier(actor)?;
        value.validate()?;
        let now = self.now();
        let tx = self
            .db
            .transaction_with_behavior(TransactionBehavior::Immediate)?;
        owns(&tx, actor, &resource(&tx, tool)?)?;
        insert_version(&tx, tool, &value)?;
        event(
            &tx,
            None,
            actor,
            "tool_version_recorded",
            &json!({"tool_id":tool,"version":value}),
            now,
        )?;
        tx.commit()?;
        Ok(())
    }
    /// Read a selected version's development context using a currently live source.read grant.
    pub fn tool_context(
        &self,
        actor: &str,
        grant_id: &str,
        tool: &str,
        selected: &str,
    ) -> Result<ToolContext> {
        // One read transaction keeps grant checks and metadata reads on the same snapshot.
        let tx = self.db.unchecked_transaction()?;
        let resource = resource(&tx, tool)?;
        permissions::require(
            &tx,
            grant_id,
            actor,
            &resource,
            &["source.read".into()].into(),
            self.now(),
        )?;
        let origin: Option<String> =
            tx.query_row("SELECT origin FROM tools WHERE id=?1", [tool], |r| r.get(0))?;
        let context = ToolContext {
            tool_id: tool.into(),
            resource,
            forked_from: origin.map(|s| serde_json::from_str(&s)).transpose()?,
            selected: version(&tx, tool, selected)?,
        };
        tx.commit()?;
        Ok(context)
    }
    pub fn fork_tool(&mut self, request: ForkTool) -> Result<ToolContext> {
        for id in [
            &request.actor,
            &request.tool_id,
            &request.resource,
            &request.originating_chat,
        ] {
            identifier(id)?;
        }
        let now = self.now();
        let tx = self
            .db
            .transaction_with_behavior(TransactionBehavior::Immediate)?;
        let source_resource = resource(&tx, &request.source.tool_id)?;
        permissions::require(
            &tx,
            &request.grant_id,
            &request.actor,
            &source_resource,
            &["source.read".into()].into(),
            now,
        )?;
        owns(&tx, &request.actor, &request.resource)?;
        if source_resource == request.resource {
            return Err(Error::Invalid(
                "fork requires a distinct destination resource".into(),
            ));
        }
        let exists: bool = tx.query_row(
            "SELECT EXISTS(SELECT 1 FROM tools WHERE id=?1)",
            [&request.tool_id],
            |r| r.get(0),
        )?;
        if exists {
            return Err(Error::Conflict("fork tool ID already exists".into()));
        }
        let mut selected = version(&tx, &request.source.tool_id, &request.source.version)?;
        if selected.originating_chat == request.originating_chat {
            return Err(Error::Invalid(
                "fork requires a distinct development chat reference".into(),
            ));
        }
        selected.originating_chat = request.originating_chat.clone();
        tx.execute(
            "INSERT INTO tools(id,resource,origin) VALUES(?1,?2,?3)",
            params![
                request.tool_id,
                request.resource,
                serde_json::to_string(&request.source)?
            ],
        )?;
        insert_version(&tx, &request.tool_id, &selected)?;
        event(
            &tx,
            None,
            &request.actor,
            "tool_forked",
            &json!({"tool_id":request.tool_id,"resource":request.resource,"source":request.source,"selected":selected}),
            now,
        )?;
        tx.commit()?;
        Ok(ToolContext {
            tool_id: request.tool_id,
            resource: request.resource,
            forked_from: Some(request.source),
            selected,
        })
    }
}
