-- Additive rolling-ledger contracts; API remains version 1.
CREATE TABLE IF NOT EXISTS build_contracts(id TEXT PRIMARY KEY,road TEXT NOT NULL REFERENCES build_roads(id),node TEXT NOT NULL REFERENCES build_nodes(id),parent_contract TEXT REFERENCES build_contracts(id),acceptance TEXT NOT NULL,implementation TEXT NOT NULL,verification TEXT NOT NULL,digest TEXT NOT NULL,created TEXT NOT NULL);

-- statement
CREATE INDEX IF NOT EXISTS build_contracts_road ON build_contracts(road);

-- statement
CREATE TRIGGER IF NOT EXISTS build_contracts_no_update BEFORE UPDATE ON build_contracts BEGIN SELECT RAISE(ABORT,'immutable build contract'); END;

-- statement
CREATE TRIGGER IF NOT EXISTS build_contracts_no_delete BEFORE DELETE ON build_contracts BEGIN SELECT RAISE(ABORT,'immutable build contract'); END;

-- statement
INSERT OR IGNORE INTO build_schema(version,applied) VALUES(2,strftime('%Y-%m-%dT%H:%M:%fZ','now'));
