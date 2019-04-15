--------------------------------------------------------------------------------
-- Copyright (C) 2018-2019 Egor Pugin <egor.pugin@gmail.com>
--
-- This Source Code Form is subject to the terms of the Mozilla Public
-- License, v. 2.0. If a copy of the MPL was not distributed with this
-- file, You can obtain one at http://mozilla.org/MPL/2.0/.
--------------------------------------------------------------------------------

--------------------------------------------------------------------------------
--
--
-- IMPORTANT!
-- When making changes here, do not forget to add patch scripts
-- at the end of the file!
--
--
--------------------------------------------------------------------------------

--------------------------------------------------------------------------------
-- sqlite3 schema
--------------------------------------------------------------------------------

CREATE TABLE config (
    config_id INTEGER PRIMARY KEY,
    config_name TEXT NOT NULL UNIQUE,
    settings TEXT NOT NULL DEFAULT '{}',
    hash TEXT NOT NULL
);

--------------------------------------------------------------------------------
--
--------------------------------------------------------------------------------

-- do not use the same id as for packages db,
-- because we may install packages from different sources,
-- so those ids won't be available from pkgs db
CREATE TABLE installed_package (
    installed_package_id INTEGER PRIMARY KEY,
    path TEXT NOT NULL,
    version TEXT NOT NULL,
    hash TEXT NOT NULL,
    group_number INTEGER NOT NULL,

    UNIQUE (path, version)
);

--------------------------------------------------------------------------------
--
--------------------------------------------------------------------------------

CREATE TABLE override_remote_package (
    override_remote_package_id INTEGER PRIMARY KEY,
    path TEXT NOT NULL UNIQUE
);

--------------------------------------------------------------------------------
--
--------------------------------------------------------------------------------

CREATE TABLE override_remote_package_version (
    override_remote_package_version_id INTEGER PRIMARY KEY,
    override_remote_package_id INTEGER NOT NULL REFERENCES override_remote_package (override_remote_package_id) ON UPDATE CASCADE ON DELETE CASCADE,
    version TEXT NOT NULL,
    prefix INTEGER NOT NULL,
    sdir TEXT NOT NULL
);

--------------------------------------------------------------------------------
--
--------------------------------------------------------------------------------

CREATE TABLE override_remote_package_version_dependency (
    override_remote_package_version_id INTEGER NOT NULL REFERENCES override_remote_package_version (override_remote_package_version_id) ON UPDATE CASCADE ON DELETE CASCADE,
    dependency TEXT NOT NULL
);

--------------------------------------------------------------------------------
--
--------------------------------------------------------------------------------

--------------------------------------------------------------------------------
--
--
-- PATCHES SECTION
--
--
--------------------------------------------------------------------------------

--------------------------------------------------------------------------------
-- %split
--------------------------------------------------------------------------------

CREATE TABLE override_remote_package (
    override_remote_package_id INTEGER PRIMARY KEY,
    path TEXT NOT NULL UNIQUE,
    sdir TEXT NOT NULL
);

--------------------------------------------------------------------------------
-- %split
--------------------------------------------------------------------------------

CREATE TABLE override_remote_package_version (
    override_remote_package_version_id INTEGER PRIMARY KEY,
    override_remote_package_id INTEGER NOT NULL REFERENCES override_remote_package (override_remote_package_id) ON UPDATE CASCADE ON DELETE CASCADE,
    version TEXT NOT NULL
);

CREATE TABLE override_remote_package_version_dependency (
    override_remote_package_version_id INTEGER NOT NULL REFERENCES override_remote_package_version (override_remote_package_version_id) ON UPDATE CASCADE ON DELETE CASCADE,
    dependency TEXT NOT NULL
);

--------------------------------------------------------------------------------
-- %split
--------------------------------------------------------------------------------

ALTER TABLE override_remote_package
RENAME COLUMN sdir TO _removed_sdir;
ALTER TABLE override_remote_package_version
ADD COLUMN sdir TEXT NOT NULL DEFAULT ':';

--------------------------------------------------------------------------------
-- %split
--------------------------------------------------------------------------------

CREATE TEMPORARY TABLE t1_backup
(
    override_remote_package_id INTEGER PRIMARY KEY,
    path TEXT NOT NULL UNIQUE
);
INSERT INTO t1_backup SELECT override_remote_package_id, path FROM override_remote_package;
DROP TABLE override_remote_package;
CREATE TABLE override_remote_package (
    override_remote_package_id INTEGER PRIMARY KEY,
    path TEXT NOT NULL UNIQUE
);
INSERT INTO override_remote_package SELECT override_remote_package_id, path FROM t1_backup;
DROP TABLE t1_backup;

--------------------------------------------------------------------------------
-- %split
--------------------------------------------------------------------------------

DELETE FROM override_remote_package;

--------------------------------------------------------------------------------
-- %split
--------------------------------------------------------------------------------

ALTER TABLE override_remote_package_version
ADD COLUMN prefix INTEGER NOT NULL DEFAULT 2;

--------------------------------------------------------------------------------
-- %split
--------------------------------------------------------------------------------

DROP TABLE startup_action;

--------------------------------------------------------------------------------
-- % split - merge '%' and 'split' together when patches are available
--------------------------------------------------------------------------------
