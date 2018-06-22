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
    package_id INTEGER NOT NULL PRIMARY KEY,
    path TEXT(4096) NOT NULL,
    flags INTEGER NOT NULL DEFAULT 0
);
CREATE UNIQUE INDEX ux_package_path ON package (path ASC);

--------------------------------------------------------------------------------
--
--------------------------------------------------------------------------------

CREATE TABLE package_version (
    package_version_id INTEGER NOT NULL PRIMARY KEY,
    package_id INTEGER NOT NULL REFERENCES package ON UPDATE CASCADE ON DELETE CASCADE,
    version TEXT NOT NULL,
    flags INTEGER NOT NULL DEFAULT 0,

    -- packages have same group_number when they are came from the same build script
    group_number INTEGER NOT NULL,
    updated TEXT NOT NULL,
    hash TEXT NOT NULL
);

--------------------------------------------------------------------------------
--
--------------------------------------------------------------------------------

CREATE TABLE package_version_dependency (
    package_version_id INTEGER NOT NULL REFERENCES package_version (package_version_id) ON UPDATE CASCADE ON DELETE CASCADE,
    package_id INTEGER NOT NULL REFERENCES package (package_id) ON UPDATE CASCADE ON DELETE CASCADE,
    version_range TEXT(1024) NOT NULL,

    PRIMARY KEY (package_version_id, package_id)
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
-- % split - merge '%' and 'split' together when patches are available
--------------------------------------------------------------------------------
