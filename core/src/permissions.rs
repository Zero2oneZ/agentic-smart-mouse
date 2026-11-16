use crate::{ACTIONS, Error, Result, engine::event, model::identifier};
use rusqlite::{Connection, OptionalExtension, TransactionBehavior, params};
use serde::{Deserialize, Serialize};
use std::collections::BTreeSet;

#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct GrantRequest {
    pub id: String,
    pub issuer: String,
    pub grantee: String,
    pub resource: String,
    pub parent: Option<String>,
    pub actions: BTreeSet<String>,
    pub may_delegate: bool,
    pub expires_at: i64,
}
#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct Grant {
    #[serde(flatten)]
    pub request: GrantRequest,
    pub revoked: bool,
}
pub(crate) fn load(db: &Connection, id: &str) -> Result<Grant> {
    let document: String = db
        .query_row("SELECT document FROM grants WHERE id=?1", [id], |r| {
            r.get(0)
        })
        .optional()?
        .ok_or_else(|| Error::NotFound("grant".into()))?;
    Ok(serde_json::from_str(&document)?)
}
fn owner(db: &Connection, resource: &str) -> Result<String> {
    db.query_row("SELECT owner FROM resources WHERE id=?1", [resource], |r| {
        r.get(0)
    })
    .optional()?
    .ok_or_else(|| Error::NotFound("resource".into()))
}
pub(crate) fn chain(db: &Connection, id: &str, now: i64) -> Result<Vec<Grant>> {
    let mut result: Vec<Grant> = Vec::new();
    let mut next = Some(id.to_owned());
    let mut visited = BTreeSet::new();
    while let Some(id) = next {
        if !visited.insert(id.clone()) || result.len() >= 32 {
            return Err(Error::Denied(
                "invalid or excessive delegation lineage".into(),
            ));
        }
        let grant = load(db, &id)?;
        let req = &grant.request;
        if grant.revoked || now >= req.expires_at {
            return Err(Error::Denied(
                "grant or ancestor has expired or been revoked".into(),
            ));
        }
        if let Some(child) = result.last() {
            let child = &child.request;
            if child.issuer != req.grantee
                || child.resource != req.resource
                || !req.may_delegate
                || !child.actions.is_subset(&req.actions)
                || child.expires_at > req.expires_at
            {
                return Err(Error::Denied(
                    "delegation exceeds ancestor authority".into(),
                ));
            }
        }
        if req.parent.is_none() && req.issuer != owner(db, &req.resource)? {
            return Err(Error::Denied(
                "root grant was not issued by the resource owner".into(),
            ));
        }
        next = req.parent.clone();
        result.push(grant);
    }
    Ok(result)
}
pub(crate) fn require(
    db: &Connection,
    grant_id: &str,
    actor: &str,
    resource: &str,
    actions: &BTreeSet<String>,
    now: i64,
) -> Result<()> {
    let lineage = chain(db, grant_id, now)?;
    let grant = &lineage[0].request;
    if grant.grantee != actor || grant.resource != resource || !actions.is_subset(&grant.actions) {
        return Err(Error::Denied(
            "actor, resource or requested actions do not match the grant".into(),
        ));
    }
    Ok(())
}

impl crate::Engine {
    /// Trusted local bootstrap. An authenticated service must authorize resource creation before calling.
    pub fn create_resource(&mut self, actor: &str, resource: &str) -> Result<()> {
        identifier(actor)?;
        identifier(resource)?;
        let now = self.now();
        let tx = self
            .db
            .transaction_with_behavior(TransactionBehavior::Immediate)?;
        let existing: Option<String> = tx
            .query_row("SELECT owner FROM resources WHERE id=?1", [resource], |r| {
                r.get(0)
            })
            .optional()?;
        if let Some(existing) = existing {
            if existing != actor {
                return Err(Error::Conflict("resource already has another owner".into()));
            }
        } else {
            tx.execute(
                "INSERT INTO resources(id,owner) VALUES(?1,?2)",
                params![resource, actor],
            )?;
            event(
                &tx,
                None,
                actor,
                "resource.created",
                &serde_json::json!({"resource":resource}),
                now,
            )?;
        }
        tx.commit()?;
        Ok(())
    }
    pub fn grant(&mut self, request: GrantRequest) -> Result<Grant> {
        for id in [
            &request.id,
            &request.issuer,
            &request.grantee,
            &request.resource,
        ] {
            identifier(id)?;
        }
        let now = self.now();
        if request.actions.is_empty()
            || request
                .actions
                .iter()
                .any(|a| !ACTIONS.contains(&a.as_str()))
            || request.expires_at <= now
            || request.expires_at > now.saturating_add(31_536_000)
        {
            return Err(Error::Invalid(
                "grant needs known actions and an expiry within one year".into(),
            ));
        }
        let tx = self
            .db
            .transaction_with_behavior(TransactionBehavior::Immediate)?;
        if let Some(parent) = &request.parent {
            let lineage = chain(&tx, parent, now)?;
            if lineage.len() >= 32 {
                return Err(Error::Denied("delegation depth limit reached".into()));
            }
            let p = &lineage[0].request;
            if !p.may_delegate
                || p.grantee != request.issuer
                || p.resource != request.resource
                || !request.actions.is_subset(&p.actions)
                || request.expires_at > p.expires_at
            {
                return Err(Error::Denied(
                    "delegation exceeds the originating grant".into(),
                ));
            }
        } else if owner(&tx, &request.resource)? != request.issuer {
            return Err(Error::Denied(
                "only the resource owner can issue a root grant".into(),
            ));
        }
        let grant = Grant {
            request,
            revoked: false,
        };
        let duplicate: bool = tx.query_row(
            "SELECT EXISTS(SELECT 1 FROM grants WHERE id=?1)",
            [&grant.request.id],
            |r| r.get(0),
        )?;
        if duplicate {
            return Err(Error::Conflict(
                "grant IDs are immutable; issue a new grant".into(),
            ));
        }
        tx.execute(
            "INSERT INTO grants(id,document) VALUES(?1,?2)",
            params![grant.request.id, serde_json::to_string(&grant)?],
        )?;
        event(
            &tx,
            None,
            &grant.request.issuer,
            "grant.issued",
            &serde_json::to_value(&grant)?,
            now,
        )?;
        tx.commit()?;
        Ok(grant)
    }
    pub fn revoke(&mut self, actor: &str, id: &str) -> Result<()> {
        let now = self.now();
        let tx = self
            .db
            .transaction_with_behavior(TransactionBehavior::Immediate)?;
        let mut grant = load(&tx, id)?;
        if actor != grant.request.issuer && actor != owner(&tx, &grant.request.resource)? {
            return Err(Error::Denied(
                "only the grant issuer or resource owner may revoke it".into(),
            ));
        }
        if !grant.revoked {
            grant.revoked = true;
            tx.execute(
                "UPDATE grants SET document=?2 WHERE id=?1",
                params![id, serde_json::to_string(&grant)?],
            )?;
            event(
                &tx,
                None,
                actor,
                "grant.revoked",
                &serde_json::json!({"grant_id":id}),
                now,
            )?;
        }
        tx.commit()?;
        Ok(())
    }
    pub fn check_access(
        &self,
        actor: &str,
        resource: &str,
        grant_id: &str,
        actions: &BTreeSet<String>,
    ) -> Result<()> {
        require(&self.db, grant_id, actor, resource, actions, self.now())
    }
}
