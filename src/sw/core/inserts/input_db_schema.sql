--------------------------------------------------------------------------------
-- SPDX-License-Identifier: GPL-3.0-or-later
-- Copyright (C) 2020 Egor Pugin <egor.pugin@gmail.com>
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

CREATE TABLE file (
    file_id INTEGER PRIMARY KEY,
    path TEXT(4096) NOT NULL COLLATE NOCASE,
    hash INTEGER NOT NULL,
    -- different size on systems (macos has 128 bits, others 64 bits)
    last_write_time BLOB NOT NULL
);
CREATE UNIQUE INDEX ux_file ON file (path ASC);

--------------------------------------------------------------------------------
--
--------------------------------------------------------------------------------

--------------------------------------------------------------------------------
-- % split - merge '%' and 'split' together when patches are available
--------------------------------------------------------------------------------
