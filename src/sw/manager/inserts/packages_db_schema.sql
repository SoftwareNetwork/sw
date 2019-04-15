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

CREATE TABLE package (
    package_id INTEGER PRIMARY KEY,
    path TEXT(4096) NOT NULL COLLATE NOCASE,
    flags INTEGER NOT NULL DEFAULT 0
);
CREATE UNIQUE INDEX ux_package_path ON package (path COLLATE NOCASE ASC);

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

    -- packages have same group_number when they are came from the same build script
    group_number INTEGER NOT NULL,
    prefix INTEGER NOT NULL DEFAULT 2,
    updated TEXT NOT NULL,
    hash TEXT NOT NULL,

    -- local overridden sdir
    sdir TEXT
);

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

CREATE TABLE data_source (
    data_source_id INTEGER PRIMARY KEY,
    data_source_name TEXT(255) NOT NULL,
    url TEXT(4096) NOT NULL,
    flags INTEGER NOT NULL,
    location TEXT(255),
    created TEXT NOT NULL
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

ALTER TABLE package_version
ADD COLUMN sdir TEXT;

--------------------------------------------------------------------------------
-- % split - merge '%' and 'split' together when patches are available
--------------------------------------------------------------------------------
