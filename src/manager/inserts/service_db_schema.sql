--------------------------------------------------------------------------------
-- Copyright (C) 2018 Egor Pugin <egor.pugin@gmail.com>
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

CREATE TABLE startup_action (
    startup_action_id INTEGER NOT NULL,
    action INTEGER NOT NULL,

    PRIMARY KEY (startup_action_id, action)
);

--------------------------------------------------------------------------------
--
--------------------------------------------------------------------------------

CREATE TABLE override_remote_package (
    override_remote_package_id INTEGER PRIMARY KEY,
    path TEXT NOT NULL UNIQUE,
    sdir TEXT NOT NULL
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
-- % split - merge '%' and 'split' together when patches are available
--------------------------------------------------------------------------------
