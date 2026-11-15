-- Private build-planning and evidence schema. Execution authority remains in the Rust core.
CREATE TABLE IF NOT EXISTS build_schema(version INTEGER PRIMARY KEY,applied TEXT NOT NULL);

-- statement
CREATE TABLE IF NOT EXISTS build_roads(id TEXT PRIMARY KEY,category INTEGER NOT NULL REFERENCES categories(id),master_chat INTEGER NOT NULL DEFAULT 1 REFERENCES categories(id),title TEXT NOT NULL,revision INTEGER NOT NULL,base_road TEXT,base_revision INTEGER,branch_ref TEXT NOT NULL,commit_ref TEXT NOT NULL,created TEXT NOT NULL);

-- statement
CREATE TABLE IF NOT EXISTS build_nodes(id TEXT PRIMARY KEY,road TEXT NOT NULL REFERENCES build_roads(id),parent TEXT REFERENCES build_nodes(id),kind TEXT NOT NULL,title TEXT NOT NULL,body TEXT NOT NULL,state TEXT NOT NULL,created TEXT NOT NULL);

-- statement
CREATE TABLE IF NOT EXISTS build_changes(id INTEGER PRIMARY KEY,road TEXT NOT NULL REFERENCES build_roads(id),revision INTEGER NOT NULL,operation TEXT NOT NULL,payload TEXT NOT NULL,created TEXT NOT NULL,event INTEGER NOT NULL REFERENCES events(id),UNIQUE(road,revision));

-- statement
CREATE TABLE IF NOT EXISTS build_docs(id TEXT PRIMARY KEY,road TEXT NOT NULL REFERENCES build_roads(id),node TEXT REFERENCES build_nodes(id),title TEXT NOT NULL);

-- statement
CREATE TABLE IF NOT EXISTS build_doc_versions(id TEXT PRIMARY KEY,doc TEXT NOT NULL REFERENCES build_docs(id),parent_version TEXT REFERENCES build_doc_versions(id),number INTEGER NOT NULL,content TEXT NOT NULL,digest TEXT NOT NULL,branch_ref TEXT NOT NULL,commit_ref TEXT NOT NULL,created TEXT NOT NULL,UNIQUE(doc,number));

-- statement
CREATE TABLE IF NOT EXISTS build_links(id TEXT PRIMARY KEY,road TEXT NOT NULL REFERENCES build_roads(id),node TEXT REFERENCES build_nodes(id),kind TEXT NOT NULL,ref TEXT NOT NULL,created TEXT NOT NULL);

-- statement
CREATE TABLE IF NOT EXISTS build_checkpoints(id TEXT PRIMARY KEY,road TEXT NOT NULL REFERENCES build_roads(id),revision INTEGER NOT NULL,snapshot TEXT NOT NULL,digest TEXT NOT NULL,created TEXT NOT NULL);

-- statement
CREATE TABLE IF NOT EXISTS build_requests(scope INTEGER NOT NULL,key TEXT NOT NULL,fingerprint TEXT NOT NULL,response TEXT NOT NULL,PRIMARY KEY(scope,key));

-- statement
CREATE INDEX IF NOT EXISTS build_nodes_road ON build_nodes(road);

-- statement
CREATE INDEX IF NOT EXISTS build_docs_road ON build_docs(road);

-- statement
CREATE INDEX IF NOT EXISTS build_links_road ON build_links(road);

-- statement
INSERT OR IGNORE INTO build_schema(version,applied) VALUES(1,strftime('%Y-%m-%dT%H:%M:%fZ','now'));

-- statement
CREATE TRIGGER IF NOT EXISTS build_doc_versions_no_update BEFORE UPDATE ON build_doc_versions BEGIN SELECT RAISE(ABORT,'immutable build evidence'); END;

-- statement
CREATE TRIGGER IF NOT EXISTS build_doc_versions_no_delete BEFORE DELETE ON build_doc_versions BEGIN SELECT RAISE(ABORT,'immutable build evidence'); END;

-- statement
CREATE TRIGGER IF NOT EXISTS build_checkpoints_no_update BEFORE UPDATE ON build_checkpoints BEGIN SELECT RAISE(ABORT,'immutable build evidence'); END;

-- statement
CREATE TRIGGER IF NOT EXISTS build_checkpoints_no_delete BEFORE DELETE ON build_checkpoints BEGIN SELECT RAISE(ABORT,'immutable build evidence'); END;

-- statement
CREATE TRIGGER IF NOT EXISTS build_changes_no_update BEFORE UPDATE ON build_changes BEGIN SELECT RAISE(ABORT,'immutable build evidence'); END;

-- statement
CREATE TRIGGER IF NOT EXISTS build_changes_no_delete BEFORE DELETE ON build_changes BEGIN SELECT RAISE(ABORT,'immutable build evidence'); END;

-- statement
CREATE TRIGGER IF NOT EXISTS build_requests_no_update BEFORE UPDATE ON build_requests BEGIN SELECT RAISE(ABORT,'immutable build evidence'); END;

-- statement
CREATE TRIGGER IF NOT EXISTS build_requests_no_delete BEFORE DELETE ON build_requests BEGIN SELECT RAISE(ABORT,'immutable build evidence'); END;
