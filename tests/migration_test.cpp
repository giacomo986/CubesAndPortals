// A world file older than the build that opens it.
//
// M9 asks for "migrazione dei salvataggi" (DESIGN.md 8). This is the gate,
// written before the code the way M8's were, and what it gates is a defect that
// has been in the file format since M2: `schema_version` was written once and
// never moved, while the schema gained `containers` in M7 and `node_meta` in
// M8. The check compares this build's number with this build's number and
// passes for any file any build ever wrote -- a check that cannot fail, which
// is the shape this project keeps finding at the bottom of its green sabotages.
//
// What that costs is not theoretical. A world made by today's build, opened by
// a build from before M8, is accepted: that build does not know the table, so
// it never clears a cell's memory when the block there is broken, and the next
// block put in that cell inherits what the last one remembered.
//
// The property, in one sentence: a file from an OLDER schema opens and keeps
// what was in it, a file from a NEWER one is refused by name, and the version
// in the file after an open is this build's.

#include "core/item/item_registry.h"
#include "core/registry/core_nodes.h"
#include "core/storage/world_db.h"
#include "core/voxel/chunk.h"
#include "core/world/node_meta.h"
#include "core/worldgen/worldgen.h"

#include <sqlite3.h>

#include <cstdio>
#include <cstdlib>
#include <string>
#include <unistd.h>

using namespace cnp;

namespace {

int g_failures = 0;

void check(bool cond, const char *what)
{
    std::printf(cond ? "  ok    %s\n" : "  FAIL  %s\n", what);
    if (!cond) ++g_failures;
}

std::string temp_path(const char *name)
{
    const char *dir = std::getenv("TMPDIR");
    return std::string(dir ? dir : "/tmp") + "/cnp_" + name + "_" +
           std::to_string((long)::getpid()) + ".db";
}

void remove_db(const std::string &path)
{
    std::remove(path.c_str());
    std::remove((path + "-wal").c_str());
    std::remove((path + "-shm").c_str());
}

constexpr uint32_t kSeed = 7;

NodeRegistry &blocks()
{
    static NodeRegistry reg = [] {
        NodeRegistry r;
        register_core_nodes(r);
        return r;
    }();
    return reg;
}

// A world with something in every table this build knows about, so that an
// upgrade that dropped one would be caught by what came back rather than by
// reading the migration.
bool fill(const std::string &path, std::string &err)
{
    World  world;
    Mapgen mapgen;
    mapgen.generate(world, blocks(), kSeed, 32, kWorldHeight);

    WorldDb db;
    if (!db.open(path, kSeed, blocks(), err)) return false;
    if (!db.save_all_chunks(world, err)) return false;

    NodeMetaSet meta;
    std::string why;
    meta.set(Cell{4, 5, 6}, "holds", "core:stone", why);
    if (!db.save_node_meta(meta, err)) return false;

    db.close();
    return true;
}

// Rewrites the version the file says it is. This is the only way to write a
// file from another build without having another build.
//
// Through sqlite and not through WorldDb, which is the point: WorldDb refuses
// to open a file whose version is not this build's, so it cannot be used to put
// one there -- the first version of this test tried, and the second case failed
// because the file it had just marked "newer" could not be opened to mark it
// "older" again.
bool say_version(const std::string &path, int version, std::string &err)
{
    sqlite3 *db = nullptr;
    if (sqlite3_open(path.c_str(), &db) != SQLITE_OK) {
        err = sqlite3_errmsg(db);
        sqlite3_close(db);
        return false;
    }
    const std::string sql = "INSERT OR REPLACE INTO meta(key, value) VALUES('schema_version', '" +
                            std::to_string(version) + "');";
    char *msg = nullptr;
    const bool ok = sqlite3_exec(db, sql.c_str(), nullptr, nullptr, &msg) == SQLITE_OK;
    if (!ok && msg) err = msg;
    sqlite3_free(msg);
    sqlite3_close(db);
    return ok;
}

} // namespace

int main()
{
    std::printf("a world file older than the build that opens it\n");

    const std::string path = temp_path("migration");
    remove_db(path);

    std::string err;
    if (!fill(path, err)) {
        std::printf("  FAIL  could not write a world to start from: %s\n", err.c_str());
        return 1;
    }

    // The control. A file this build wrote opens again, and everything in it is
    // still there. Without this the two checks below could both pass on a build
    // that cannot read its own files.
    {
        WorldDb db;
        check(db.open(path, kSeed, blocks(), err), "a file this build wrote opens again");

        World world;
        check(db.load_all_chunks(world, err) > 0, "and the chunks are in it");

        NodeMetaSet meta;
        check(db.load_node_meta(meta, err) == 1, "and so is what a cell remembered");
        db.close();
    }

    // A file from a build AFTER this one. Refused, and the message has to name
    // both numbers: "this file is newer" sends somebody to look for a corrupt
    // file, and what they need to know is which build to run.
    {
        check(say_version(path, kWorldSchemaVersion + 5, err), "a file can claim a later version");

        WorldDb db;
        err.clear();
        const bool opened = db.open(path, kSeed, blocks(), err);
        check(!opened, "a file from a later build is refused rather than reinterpreted");
        check(err.find(std::to_string(kWorldSchemaVersion + 5)) != std::string::npos &&
                  err.find(std::to_string(kWorldSchemaVersion)) != std::string::npos,
              "and the refusal names the file's version and this build's");
        db.close();
    }

    // And a refused open must not CHANGE the file it refused. A bare file with
    // nothing but a version row in it: if this build applies its schema before
    // deciding, the refusal leaves a file full of tables the build that made it
    // has never heard of.
    {
        const std::string bare = temp_path("migration_bare");
        remove_db(bare);

        sqlite3 *raw = nullptr;
        check(sqlite3_open(bare.c_str(), &raw) == SQLITE_OK, "a bare file can be made");
        sqlite3_exec(raw,
                     "CREATE TABLE meta(key TEXT PRIMARY KEY, value TEXT);"
                     "INSERT INTO meta VALUES('schema_version', '99');",
                     nullptr, nullptr, nullptr);
        sqlite3_close(raw);

        {
            WorldDb db;
            err.clear();
            check(!db.open(bare, kSeed, blocks(), err), "and a file from version 99 is refused");
            db.close();
        }

        int tables = 0;
        if (sqlite3_open(bare.c_str(), &raw) == SQLITE_OK) {
            sqlite3_stmt *stmt = nullptr;
            if (sqlite3_prepare_v2(raw, "SELECT COUNT(*) FROM sqlite_master WHERE type='table';",
                                   -1, &stmt, nullptr) == SQLITE_OK &&
                sqlite3_step(stmt) == SQLITE_ROW)
                tables = sqlite3_column_int(stmt, 0);
            sqlite3_finalize(stmt);
        }
        sqlite3_close(raw);

        check(tables == 1, "and the refusal left the file exactly as it found it");
        remove_db(bare);
    }

    // And the half M9 is for: a file from a build BEFORE this one. It opens, it
    // keeps what was in it, and afterwards the file says this build's version.
    //
    // Written against kWorldSchemaVersion - 1 rather than against the number 1,
    // so it goes on meaning the same thing the next time the schema moves.
    {
        check(say_version(path, kWorldSchemaVersion - 1, err),
              "a file can claim the version before this one");

        WorldDb db;
        err.clear();
        const bool opened = db.open(path, kSeed, blocks(), err);
        check(opened, opened ? "a file from an earlier build opens"
                             : ("a file from an earlier build was refused: " + err).c_str());
        if (opened) {
            World world;
            check(db.load_all_chunks(world, err) > 0, "and its chunks survived the upgrade");

            NodeMetaSet meta;
            check(db.load_node_meta(meta, err) == 1,
                  "and so did what a cell remembered, in a table that build may not have had");

            std::string said;
            check(db.get_meta("schema_version", said) &&
                      said == std::to_string(kWorldSchemaVersion),
                  "and the file now says the version of the build that upgraded it");
        }
        db.close();
    }

    // The version has to move when the schema does. Nothing here can check that
    // a human bumped it, and this is the closest a test gets: the schema has
    // changed twice since the number was first written, so a build that still
    // says 1 is a build whose version says nothing.
    check(kWorldSchemaVersion > 1,
          "the version has moved since the schema did (containers in M7, node_meta in M8)");

    remove_db(path);

    std::printf(g_failures == 0 ? "\nMIGRATION OK\n" : "\nMIGRATION FAILED (%d)\n", g_failures);
    return g_failures == 0 ? 0 : 1;
}
