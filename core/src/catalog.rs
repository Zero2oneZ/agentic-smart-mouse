//! Validated design catalog. Loading it does not provision folders or enable services.
use crate::{
    Error, Result,
    model::{digest, identifier},
};
use serde::{Deserialize, Serialize};
use std::collections::{BTreeMap, BTreeSet};

#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct SystemCatalog {
    pub schema_version: u32,
    pub id: String,
    pub provisioning: String,
    pub categories: Vec<FolderCategory>,
    pub lifecycles: BTreeMap<String, Lifecycle>,
    pub entity_kinds: Vec<EntityKind>,
    pub relations: Vec<RelationKind>,
}
#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct FolderCategory {
    pub id: String,
    pub title: String,
    pub description: String,
    pub definition_path: String,
    pub belongings_path: String,
    pub subdirectories: Vec<String>,
}
#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct Lifecycle {
    pub states: Vec<String>,
    pub initial: String,
    pub terminal: Vec<String>,
    pub transitions: Vec<[String; 2]>,
}
#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct EntityKind {
    pub id: String,
    pub prefix: String,
    pub lifecycle: String,
}
#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(rename_all = "snake_case")]
pub enum GraphKind {
    Containment,
    Execution,
    Provenance,
    Association,
    Connectivity,
}
#[derive(Debug, Clone, Serialize, Deserialize)]
#[serde(deny_unknown_fields)]
pub struct RelationKind {
    pub id: String,
    pub graph: GraphKind,
    pub sources: Vec<String>,
    pub targets: Vec<String>,
    pub acyclic: bool,
}
fn invalid(message: &str) -> Error {
    Error::Invalid(format!("system catalog: {message}"))
}
fn unique<'a>(items: impl IntoIterator<Item = &'a str>) -> Result<BTreeSet<&'a str>> {
    let mut seen = BTreeSet::new();
    for id in items {
        identifier(id)?;
        if !seen.insert(id) {
            return Err(invalid("duplicate identifier"));
        }
    }
    Ok(seen)
}
fn path(value: &str) -> Result<()> {
    if value.len() > 200
        || value.split('/').any(|p| {
            p.is_empty()
                || p == "."
                || p == ".."
                || !p
                    .bytes()
                    .all(|b| b.is_ascii_lowercase() || b.is_ascii_digit() || b == b'-')
        })
    {
        return Err(invalid(
            "paths must be bounded relative lowercase components",
        ));
    }
    Ok(())
}
impl SystemCatalog {
    pub fn bundled() -> Result<Self> {
        let catalog: Self =
            serde_json::from_str(include_str!("../templates/system-catalog-v1.json"))?;
        catalog.validate()?;
        Ok(catalog)
    }
    pub fn validate(&self) -> Result<()> {
        identifier(&self.id)?;
        if self.schema_version != 1 || self.provisioning != "explicit_lazy" {
            return Err(invalid("unsupported schema or provisioning mode"));
        }
        if self.categories.is_empty()
            || self.categories.len() > 64
            || self.entity_kinds.is_empty()
            || self.entity_kinds.len() > 96
            || self.relations.is_empty()
            || self.relations.len() > 128
            || self.lifecycles.is_empty()
            || self.lifecycles.len() > 32
        {
            return Err(invalid("catalog size is out of bounds"));
        }
        unique(self.categories.iter().map(|c| c.id.as_str()))?;
        let mut locations = BTreeSet::new();
        for c in &self.categories {
            if c.title.trim().is_empty()
                || c.title.len() > 96
                || c.description.trim().is_empty()
                || c.description.len() > 512
                || c.subdirectories.len() > 32
            {
                return Err(invalid("invalid category description or size"));
            }
            path(&c.definition_path)?;
            path(&c.belongings_path)?;
            if c.definition_path != format!("catalog/{}", c.id)
                || !(c.belongings_path.starts_with("libraries/") || c.belongings_path == "media")
            {
                return Err(invalid("category paths must use the declared roots"));
            }
            for existing in &locations {
                let existing: &String = existing;
                if existing == &c.belongings_path
                    || existing.starts_with(&(c.belongings_path.clone() + "/"))
                    || c.belongings_path.starts_with(&(existing.clone() + "/"))
                {
                    return Err(invalid("category runtime roots overlap"));
                }
            }
            locations.insert(c.belongings_path.clone());
            unique(c.subdirectories.iter().map(String::as_str))?;
            for sub in &c.subdirectories {
                path(sub)?;
            }
        }
        for (id, lifecycle) in &self.lifecycles {
            identifier(id)?;
            if lifecycle.states.is_empty()
                || lifecycle.states.len() > 32
                || lifecycle.transitions.len() > 128
            {
                return Err(invalid("lifecycle size is out of bounds"));
            }
            let states = unique(lifecycle.states.iter().map(String::as_str))?;
            let terminal = unique(lifecycle.terminal.iter().map(String::as_str))?;
            if !states.contains(lifecycle.initial.as_str())
                || terminal.is_empty()
                || !terminal.is_subset(&states)
            {
                return Err(invalid("unknown initial or terminal state"));
            }
            let mut edges = BTreeSet::new();
            for [from, to] in &lifecycle.transitions {
                if !states.contains(from.as_str())
                    || !states.contains(to.as_str())
                    || terminal.contains(from.as_str())
                    || from == to
                    || !edges.insert((from, to))
                {
                    return Err(invalid("invalid lifecycle transition"));
                }
            }
            let mut reachable = BTreeSet::from([lifecycle.initial.as_str()]);
            loop {
                let before = reachable.len();
                for [from, to] in &lifecycle.transitions {
                    if reachable.contains(from.as_str()) {
                        reachable.insert(to.as_str());
                    }
                }
                if reachable.len() == before {
                    break;
                }
            }
            if reachable != states {
                return Err(invalid("unreachable lifecycle state"));
            }
            let mut can_finish = terminal.clone();
            loop {
                let before = can_finish.len();
                for [from, to] in &lifecycle.transitions {
                    if can_finish.contains(to.as_str()) {
                        can_finish.insert(from.as_str());
                    }
                }
                if can_finish.len() == before {
                    break;
                }
            }
            if can_finish != states {
                return Err(invalid("lifecycle contains a state with no terminal path"));
            }
        }
        let kinds = unique(self.entity_kinds.iter().map(|e| e.id.as_str()))?;
        unique(self.entity_kinds.iter().map(|e| e.prefix.as_str()))?;
        for kind in &self.entity_kinds {
            if kind.prefix.len() > 8
                || !kind.prefix.bytes().all(|b| b.is_ascii_lowercase())
                || !self.lifecycles.contains_key(&kind.lifecycle)
            {
                return Err(invalid("invalid entity prefix or lifecycle"));
            }
        }
        unique(self.relations.iter().map(|r| r.id.as_str()))?;
        for r in &self.relations {
            if r.sources.is_empty()
                || r.targets.is_empty()
                || r.sources.len() > 96
                || r.targets.len() > 96
            {
                return Err(invalid("relation endpoints are out of bounds"));
            }
            for endpoints in [&r.sources, &r.targets] {
                let endpoints = unique(endpoints.iter().map(String::as_str))?;
                if !endpoints.is_subset(&kinds) {
                    return Err(invalid("unknown relation endpoint kind"));
                }
            }
            if matches!(
                r.graph,
                GraphKind::Containment | GraphKind::Execution | GraphKind::Provenance
            ) && !r.acyclic
            {
                return Err(invalid(
                    "containment, execution and provenance must forbid cycles",
                ));
            }
        }
        Ok(())
    }
    pub fn hash(&self) -> Result<String> {
        self.validate()?;
        digest(self)
    }
}
