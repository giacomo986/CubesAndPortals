# SQLite, vendored

`sqlite3.c`, `sqlite3.h` and `sqlite3ext.h`, exactly as published, from the
official amalgamation:

- version **3.50.4**
- <https://www.sqlite.org/2025/sqlite-amalgamation-3500400.zip>
- sha256 of that zip: `1d3049dd0f830a025a53105fc79fd2ab9431aea99e137809d064d8ee8356b032`

## Why a copy and not a submodule

The other three dependencies here are submodules, and this one is not, which is
worth explaining rather than leaving as an inconsistency.

SQLite's canonical repository is Fossil, and the GitHub mirror ships the source
in ~100 separate files that have to be run through a Tcl script to produce the
amalgamation. That would put **tclsh** in the build requirements of this
project, and DESIGN.md constraint 4 is that no new toolchain is added for
anything that can be had without one.

The amalgamation is the form SQLite itself recommends for embedding, it is a
single translation unit, and it is public domain. Copying it in costs one
directory and no build step.

## Updating it

Download the new amalgamation from sqlite.org, replace the three files, put the
new version and hash above, and say in the commit which version and why. There
is nothing else to do: no patches are applied and no files are edited.
