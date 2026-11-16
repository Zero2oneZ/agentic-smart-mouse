use hotbox_core::{
    ClaimResult, Engine, GrantRequest, Result, StartRequest, Template, WorkerReport, fixture_report,
};
use serde::Deserialize;
use serde_json::{Value, json};
use std::{
    io::{self, Read},
    process::ExitCode,
};

#[derive(Deserialize)]
#[serde(tag = "command", rename_all = "snake_case", deny_unknown_fields)]
enum Command {
    ToolMenuRegister {
        request: hotbox_core::ToolMenuRegistration,
    },
    ToolMenuUpdate {
        request: hotbox_core::ToolMenuUpdate,
    },
    ToolMenuRead {
        actor: String,
        tool_id: String,
    },
    ToolMenuList {
        actor: String,
        resource: String,
        after: String,
    },
    NotesTick {
        actor: String,
    },
    NotesStart {
        request: hotbox_core::NotesRequest,
    },
    NotesList {
        actor: String,
        resource: String,
    },
    NotesRead {
        actor: String,
        id: String,
    },
    NotesAdvance {
        actor: String,
        id: String,
    },
    NotesControl {
        actor: String,
        id: String,
        action: String,
    },
    RegisterTool {
        request: hotbox_core::RegisterTool,
    },
    ToolVersion {
        actor: String,
        tool_id: String,
        version: hotbox_core::ToolVersion,
    },
    ToolContext {
        actor: String,
        grant_id: String,
        tool_id: String,
        version: String,
    },
    ForkTool {
        request: hotbox_core::ForkTool,
    },
    Resource {
        actor: String,
        resource: String,
    },
    Grant {
        request: GrantRequest,
    },
    Revoke {
        actor: String,
        grant_id: String,
    },
    Start {
        request: StartRequest,
    },
    Status {
        actor: String,
        run_id: String,
    },
    Claim {
        actor: String,
        run_id: String,
        worker: String,
    },
    Finish {
        actor: String,
        run_id: String,
        segment_id: String,
        worker: String,
        token: String,
        report: WorkerReport,
    },
    Cancel {
        actor: String,
        run_id: String,
    },
    Events {
        actor: String,
        run_id: String,
        after: i64,
    },
    FixtureLoop {
        actor: String,
        run_id: String,
        worker: String,
        max_steps: u32,
    },
}
fn bounded_read(reader: impl Read) -> Result<Vec<u8>> {
    let mut bytes = Vec::new();
    reader.take(262145).read_to_end(&mut bytes)?;
    if bytes.len() > 262144 {
        return Err(hotbox_core::Error::Invalid(
            "request exceeds 256 KiB".into(),
        ));
    }
    Ok(bytes)
}
fn execute() -> Result<Value> {
    let args: Vec<_> = std::env::args().skip(1).collect();
    if args.as_slice() == ["tool-menu-catalog"] {
        return Ok(hotbox_core::tool_menu_catalog());
    }
    if args.len() == 2 && args[0] == "backpack-example" {
        let backpack = hotbox_core::Backpack::example(&args[1])?;
        return Ok(
            json!({"backpack_hash":backpack.hash()?,"backpack":backpack,"example_only":true}),
        );
    }
    if args.len() == 2 && args[0] == "validate-backpack" {
        let backpack: hotbox_core::Backpack =
            serde_json::from_slice(&bounded_read(std::fs::File::open(&args[1])?)?)?;
        return Ok(json!({"backpack_id":backpack.id,"backpack_hash":backpack.hash()?}));
    }
    if args.len() == 2 && args[0] == "plan-environment" {
        let request: hotbox_core::EnvironmentRequest =
            serde_json::from_slice(&bounded_read(std::fs::File::open(&args[1])?)?)?;
        return Ok(serde_json::to_value(request.plan()?)?);
    }
    if args.as_slice() == ["catalog"] {
        let catalog = hotbox_core::SystemCatalog::bundled()?;
        return Ok(json!({"catalog_hash":catalog.hash()?, "catalog":catalog}));
    }
    if args.len() == 2 && args[0] == "validate-catalog" {
        let catalog: hotbox_core::SystemCatalog =
            serde_json::from_slice(&bounded_read(std::fs::File::open(&args[1])?)?)?;
        return Ok(json!({"catalog_id":catalog.id,"catalog_hash":catalog.hash()?}));
    }
    if args.len() == 2 && args[0] == "validate" {
        let template: Template =
            serde_json::from_slice(&bounded_read(std::fs::File::open(&args[1])?)?)?;
        return Ok(
            json!({"template_id":template.id,"template_version":template.version,"template_hash":template.hash()?}),
        );
    }
    if args.len() != 1 {
        return Err(hotbox_core::Error::Invalid(
            "usage: hotbox-core DATABASE < request.json | hotbox-core validate TEMPLATE.json | hotbox-core catalog | hotbox-core validate-catalog CATALOG.json | hotbox-core backpack-example NAME | hotbox-core validate-backpack FILE | hotbox-core plan-environment FILE"
                .into(),
        ));
    }
    // Parse before opening the database: malformed requests have no database side effects.
    let command: Command = serde_json::from_slice(&bounded_read(io::stdin().lock())?)?;
    let mut engine = Engine::open(&args[0])?;
    Ok(match command {
        Command::ToolMenuRegister { request } => {
            serde_json::to_value(engine.register_tool_menu(request)?)?
        }
        Command::ToolMenuUpdate { request } => {
            serde_json::to_value(engine.update_tool_menu(request)?)?
        }
        Command::ToolMenuRead { actor, tool_id } => {
            serde_json::to_value(engine.read_tool_menu(&actor, &tool_id)?)?
        }
        Command::ToolMenuList {
            actor,
            resource,
            after,
        } => serde_json::to_value(engine.list_tool_menu(&actor, &resource, &after)?)?,
        Command::NotesTick { actor } => serde_json::to_value(engine.tick_notes(&actor)?)?,
        Command::NotesStart { request } => serde_json::to_value(engine.start_notes(request)?)?,
        Command::NotesList { actor, resource } => {
            serde_json::to_value(engine.notes_tasks(&actor, &resource)?)?
        }
        Command::NotesRead { actor, id } => serde_json::to_value(engine.notes_task(&actor, &id)?)?,
        Command::NotesAdvance { actor, id } => {
            serde_json::to_value(engine.advance_notes(&actor, &id)?)?
        }
        Command::NotesControl { actor, id, action } => {
            serde_json::to_value(engine.control_notes(&actor, &id, &action)?)?
        }
        Command::RegisterTool { request } => serde_json::to_value(engine.register_tool(request)?)?,
        Command::ToolVersion {
            actor,
            tool_id,
            version,
        } => {
            engine.add_tool_version(&actor, &tool_id, version)?;
            json!({})
        }
        Command::ToolContext {
            actor,
            grant_id,
            tool_id,
            version,
        } => serde_json::to_value(engine.tool_context(&actor, &grant_id, &tool_id, &version)?)?,
        Command::ForkTool { request } => serde_json::to_value(engine.fork_tool(request)?)?,
        Command::Resource { actor, resource } => {
            engine.create_resource(&actor, &resource)?;
            json!({})
        }
        Command::Grant { request } => serde_json::to_value(engine.grant(request)?)?,
        Command::Revoke { actor, grant_id } => {
            engine.revoke(&actor, &grant_id)?;
            json!({})
        }
        Command::Start { request } => serde_json::to_value(engine.start(request)?)?,
        Command::Status { actor, run_id } => serde_json::to_value(engine.status(&actor, &run_id)?)?,
        Command::Claim {
            actor,
            run_id,
            worker,
        } => serde_json::to_value(engine.claim(&actor, &run_id, &worker)?)?,
        Command::Finish {
            actor,
            run_id,
            segment_id,
            worker,
            token,
            report,
        } => serde_json::to_value(engine.finish(
            &actor,
            &run_id,
            &segment_id,
            &worker,
            &token,
            report,
        )?)?,
        Command::Cancel { actor, run_id } => serde_json::to_value(engine.cancel(&actor, &run_id)?)?,
        Command::Events {
            actor,
            run_id,
            after,
        } => json!({"events":engine.events(&actor, &run_id, after)?}),
        Command::FixtureLoop {
            actor,
            run_id,
            worker,
            max_steps,
        } => {
            if !(1..=512).contains(&max_steps) {
                return Err(hotbox_core::Error::Invalid(
                    "max_steps must be 1–512".into(),
                ));
            }
            let mut executed = 0;
            for _ in 0..max_steps {
                match engine.claim(&actor, &run_id, &worker)? {
                    ClaimResult::Claimed { claim } => {
                        let report = fixture_report(&claim)?;
                        engine.finish(
                            &actor,
                            &run_id,
                            &claim.segment.id,
                            &worker,
                            &claim.token,
                            report,
                        )?;
                        executed += 1;
                    }
                    ClaimResult::Idle { .. } => break,
                }
            }
            json!({"executed":executed,"run":engine.status(&actor, &run_id)?})
        }
    })
}
fn main() -> ExitCode {
    match execute() {
        Ok(result) => {
            println!("{}", json!({"ok":true,"result":result}));
            ExitCode::SUCCESS
        }
        Err(error) => {
            println!("{}", json!({"ok":false,"error":error.to_string()}));
            ExitCode::FAILURE
        }
    }
}
