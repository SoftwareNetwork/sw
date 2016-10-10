#
# cppan
#

add_library(sqlite3
    sqlite3.c
    sqlite3.h
    sqlite3ext.h
)

set_property(TARGET
    sqlite3
PROPERTY FOLDER dep)
