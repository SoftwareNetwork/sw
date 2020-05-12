--------------------------------------------------------------------------------
-- Copyright (C) 2018-2020 Egor Pugin <egor.pugin@gmail.com>
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

CREATE TABLE package (
    package_id INTEGER PRIMARY KEY,
    path TEXT(4096) NOT NULL COLLATE NOCASE,
    flags INTEGER NOT NULL DEFAULT 0
);
CREATE UNIQUE INDEX ux_package_path ON package (path ASC);

--------------------------------------------------------------------------------
--
--------------------------------------------------------------------------------

CREATE TABLE package_version (
    package_version_id INTEGER PRIMARY KEY,
    package_id INTEGER NOT NULL REFERENCES package ON UPDATE CASCADE ON DELETE CASCADE,
    version TEXT NOT NULL,
    target_version_id INTEGER,
    flags INTEGER NOT NULL DEFAULT 0,

    -- archive version shows how package is packed
    archive_version INTEGER NOT NULL,

    prefix INTEGER NOT NULL DEFAULT 2,
    updated TEXT NOT NULL,
    hash TEXT NOT NULL,

    -- local overridden sdir
    sdir TEXT
);
CREATE INDEX ix_package_id ON package_version (package_id COLLATE NOCASE ASC);

--------------------------------------------------------------------------------
--
--------------------------------------------------------------------------------

CREATE TABLE package_version_dependency (
    package_version_id INTEGER REFERENCES package_version (package_version_id) ON UPDATE CASCADE ON DELETE CASCADE,
    package_id INTEGER REFERENCES package (package_id) ON UPDATE CASCADE ON DELETE CASCADE,
    version_range TEXT(1024) NOT NULL,

    PRIMARY KEY (package_version_id, package_id)
);

--------------------------------------------------------------------------------
--
--------------------------------------------------------------------------------

CREATE TABLE config (
    config_id INTEGER PRIMARY KEY,
    hash INTEGER NOT NULL
);

--------------------------------------------------------------------------------
--
--------------------------------------------------------------------------------

CREATE TABLE file (
    file_id INTEGER PRIMARY KEY,
    hash INTEGER NOT NULL,
    size TEXT
);

--------------------------------------------------------------------------------
--
--------------------------------------------------------------------------------

CREATE TABLE package_version_file (
    package_version_file_id INTEGER PRIMARY KEY,
    package_version_id INTEGER NOT NULL REFERENCES package_version (package_version_id) ON UPDATE CASCADE ON DELETE CASCADE,
    file_id INTEGER NOT NULL REFERENCES file (file_id) ON UPDATE CASCADE ON DELETE CASCADE,
    type INTEGER NOT NULL,
    config_id INTEGER NOT NULL REFERENCES config (config_id) ON UPDATE CASCADE ON DELETE CASCADE,
    flags INTEGER NOT NULL DEFAULT 0,
    archive_version INTEGER NOT NULL
);
-- allow only one source archive for package version
-- ssa = single_source_archive
CREATE UNIQUE INDEX package_version_file_package_version_id_ssa_idx
ON package_version_file (package_version_id)
WHERE (type = 1);

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

ALTER TABLE package_version
ADD COLUMN sdir TEXT;

--------------------------------------------------------------------------------
-- %split
--------------------------------------------------------------------------------

CREATE INDEX ix_package_id ON package_version (package_id ASC);

--------------------------------------------------------------------------------
-- %split
--------------------------------------------------------------------------------

-- removed, do not delete empty split, to prevent version downgrade

--------------------------------------------------------------------------------
-- %split
--------------------------------------------------------------------------------

-- removed, do not delete empty split, to prevent version downgrade

--------------------------------------------------------------------------------
-- %split
--------------------------------------------------------------------------------

DROP TABLE data_source;

--------------------------------------------------------------------------------
-- %split
--------------------------------------------------------------------------------

CREATE TABLE config (
    config_id INTEGER PRIMARY KEY,
    hash INTEGER NOT NULL
);

CREATE TABLE file (
    file_id INTEGER PRIMARY KEY,
    hash INTEGER NOT NULL,
    size TEXT
);

CREATE TABLE package_version_file (
    package_version_file_id INTEGER PRIMARY KEY,
    package_version_id INTEGER NOT NULL REFERENCES package_version (package_version_id) ON UPDATE CASCADE ON DELETE CASCADE,
    file_id INTEGER NOT NULL REFERENCES file (file_id) ON UPDATE CASCADE ON DELETE CASCADE,
    type INTEGER NOT NULL,
    config_id INTEGER NOT NULL REFERENCES config (config_id) ON UPDATE CASCADE ON DELETE CASCADE,
    flags INTEGER NOT NULL DEFAULT 0,
    archive_version INTEGER NOT NULL
);

--------------------------------------------------------------------------------
-- %split
--------------------------------------------------------------------------------

-- allow only one source archive for package version
-- ssa = single_source_archive
CREATE UNIQUE INDEX package_version_file_package_version_id_ssa_idx
ON package_version_file (package_version_id)
WHERE (type = 1);

--------------------------------------------------------------------------------
-- % split - merge '%' and 'split' together when patches are available
--------------------------------------------------------------------------------
