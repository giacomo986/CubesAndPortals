#include "core/storage/world_db.h"

#include "core/item/item_codec.h"
#include "core/net/bytes.h"
#include "core/net/chunk_codec.h"
#include "core/registry/core_nodes.h"

#include <sqlite3.h>

#include <cstdio>
#include <cstring>
#include <set>

namespace cnp {
namespace {

// Names the server's owner let go of (cnp_server --free-name), with the
// identity that had them. Whoever claims one next takes over that identity's
// bag and place: that is how a player who forgot their password gets back what
// they had, and how a name is handed to somebody else.
#define FREED_NAMES_TABLE                                                                      \
    "CREATE TABLE IF NOT EXISTS freed_names ("                                                 \
    "  name TEXT PRIMARY KEY,"                                                                 \
    "  identity BLOB NOT NULL"                                                                 \
    ");"

// Bumped when the schema changes in a way an older build reads wrongly. An
// older build opening a newer file is refused by this rather than by whatever
// it happens to trip over first.

const char *kSchema =
    "PRAGMA journal_mode = WAL;"
    "PRAGMA synchronous = NORMAL;"
    "CREATE TABLE IF NOT EXISTS meta ("
    "  key TEXT PRIMARY KEY,"
    "  value TEXT NOT NULL"
    ");"
    "CREATE TABLE IF NOT EXISTS chunks ("
    "  x INTEGER NOT NULL,"
    "  y INTEGER NOT NULL,"
    "  z INTEGER NOT NULL,"
    "  data BLOB NOT NULL,"
    "  PRIMARY KEY (x, y, z)"
    ") WITHOUT ROWID;"
    // What is in the chests. Keyed by the cell, because that is what a
    // container is keyed by everywhere else (core/item/container.h): a chest
    // belongs to a place and not to a session.
    //
    // A table and not a column on `chunks`, even though a container always
    // sits inside one. Chunks are a blob written by the chunk codec, byte for
    // byte the same thing the network sends, and that is a property worth more
    // than the join it costs -- the moment a stored chunk carries something a
    // streamed chunk does not, there are two formats and one of them is only
    // ever parsed on this side.
    // What a player is carrying, keyed by their IDENTITY and not by their name.
    //
    // DESIGN.md 4.4 is explicit that the key is the identity and the name is a
    // label -- two players may pick the same name and neither can pick the
    // other's key -- and this is the first place that costs something. A bag
    // keyed by name is a bag anybody takes by typing yours. A session with no
    // identity therefore has no row here at all and starts empty every time,
    // which is a real limit and better than the alternative.
    //
    // The name is stored beside it anyway, for a human reading the file: it is
    // the last label that key played under and it is not what anything looks
    // up by.
    "CREATE TABLE IF NOT EXISTS player_items ("
    "  identity BLOB PRIMARY KEY,"
    "  name TEXT NOT NULL,"
    "  data BLOB NOT NULL,"
    "  last_seen INTEGER NOT NULL"
    ") WITHOUT ROWID;"
    "CREATE TABLE IF NOT EXISTS containers ("
    "  x INTEGER NOT NULL,"
    "  y INTEGER NOT NULL,"
    "  z INTEGER NOT NULL,"
    "  data BLOB NOT NULL,"
    "  PRIMARY KEY (x, y, z)"
    ") WITHOUT ROWID;"
    // What the cells remember (core/world/node_meta.h, M8). A row per FIELD
    // rather than a blob per cell, because these are short strings a person
    // reading the file has a reason to look at -- a clock's count, whether a
    // vent is enabled -- and a blob would put them behind a decoder.
    //
    // CREATE IF NOT EXISTS runs on every open, so a world file written before
    // this table existed grows one the next time it is opened, holding nothing.
    // No migration: an empty table and a world whose cells remember nothing are
    // the same world.
    // What mods keep across a restart (core.storage_set), each mod's own. The
    // same argument as node_meta: an empty table is a world whose mods kept
    // nothing, so a file from before it needs no migration.
    "CREATE TABLE IF NOT EXISTS mod_store ("
    "  mod TEXT NOT NULL,"
    "  key TEXT NOT NULL,"
    "  value TEXT NOT NULL,"
    "  PRIMARY KEY (mod, key)"
    ") WITHOUT ROWID;"
    "CREATE TABLE IF NOT EXISTS node_meta ("
    "  x INTEGER NOT NULL,"
    "  y INTEGER NOT NULL,"
    "  z INTEGER NOT NULL,"
    "  key TEXT NOT NULL,"
    "  value TEXT NOT NULL,"
    "  PRIMARY KEY (x, y, z, key)"
    ") WITHOUT ROWID;"
    // The block table the chunks were written with: the name of every id, so
    // that a build whose table moved can read them by name (remap_blocks). A
    // file from before this table grows it empty, and has only its
    // content_hash to go by until a build with the same table writes it.
    "CREATE TABLE IF NOT EXISTS blocks ("
    "  id INTEGER PRIMARY KEY,"
    "  name TEXT NOT NULL"
    ");"
    // Who each name on this world belongs to. Separate from `players`, which
    // holds where somebody was and only once they have been somewhere: a name
    // is owned from the first join, by whoever joined under it first.
    "CREATE TABLE IF NOT EXISTS names ("
    "  name TEXT PRIMARY KEY,"
    "  identity BLOB NOT NULL,"
    "  first_seen INTEGER NOT NULL"
    ");"
    FREED_NAMES_TABLE;

// Where each player was, keyed by identity (schema 3). Apart from kSchema
// because the migration from 2 has to create it again: before 3 a table of
// the same name was keyed by the player's NAME, and CREATE IF NOT EXISTS
// leaves a table with the old columns exactly where it is.
const char *kPlayersTable =
    "CREATE TABLE IF NOT EXISTS players ("
    "  identity BLOB PRIMARY KEY,"
    "  name TEXT NOT NULL,"
    "  x REAL NOT NULL, y REAL NOT NULL, z REAL NOT NULL,"
    "  last_seen INTEGER NOT NULL,"
    "  look_x REAL NOT NULL DEFAULT 0, look_y REAL NOT NULL DEFAULT 0,"
    "  look_z REAL NOT NULL DEFAULT 0"
    ") WITHOUT ROWID;";

std::string sqlite_error(::sqlite3 *db, const std::string &what)
{
    return what + ": " + (db ? sqlite3_errmsg(db) : "no database");
}

// The names the file's chunks were written with, in id order. Empty for a file
// from before the table was kept -- and for one whose table has a hole in it,
// which is no better than none.
std::vector<std::string> read_block_table(::sqlite3 *db)
{
    std::vector<std::string> names;
    sqlite3_stmt            *stmt = nullptr;
    if (sqlite3_prepare_v2(db, "SELECT id, name FROM blocks ORDER BY id", -1, &stmt, nullptr) !=
        SQLITE_OK)
        return names;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const unsigned char *text = sqlite3_column_text(stmt, 1);
        if (sqlite3_column_int64(stmt, 0) != (int64_t)names.size() || !text) {
            names.clear();
            break;
        }
        names.emplace_back((const char *)text);
    }
    sqlite3_finalize(stmt);
    return names;
}

} // namespace

WorldDb::~WorldDb()
{
    close();
}

bool WorldDb::exec(const char *sql, std::string &err)
{
    char *message = nullptr;
    if (sqlite3_exec(m_db, sql, nullptr, nullptr, &message) != SQLITE_OK) {
        err = std::string("sql failed: ") + (message ? message : "unknown");
        sqlite3_free(message);
        return false;
    }
    return true;
}

bool WorldDb::has_column(const char *table, const char *column) const
{
    if (!m_db) return false;

    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, "SELECT 1 FROM pragma_table_info(?) WHERE name = ?;", -1,
                           &stmt, nullptr) != SQLITE_OK)
        return false;
    sqlite3_bind_text(stmt, 1, table, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, column, -1, SQLITE_STATIC);
    const bool there = sqlite3_step(stmt) == SQLITE_ROW;
    sqlite3_finalize(stmt);
    return there;
}

// Does this file have that table? Asked of sqlite_master, because "the read
// failed" and "there is no such table" are different things and only one of
// them means the file is new.
bool WorldDb::has_table(const char *name) const
{
    if (!m_db) return false;

    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(m_db,
                           "SELECT 1 FROM sqlite_master WHERE type = 'table' AND name = ?;", -1,
                           &stmt, nullptr) != SQLITE_OK)
        return false;
    sqlite3_bind_text(stmt, 1, name, -1, SQLITE_STATIC);
    const bool there = sqlite3_step(stmt) == SQLITE_ROW;
    sqlite3_finalize(stmt);
    return there;
}

// Brings a file forward from the version it says it is to this build's.
//
// One step per version, in order, inside ONE transaction: a migration that
// stopped half way would leave a file that is neither version, and the next
// open would read it as whatever the version row happened to say.
//
// 1 -> 2 has no step of its own and that is not an oversight. Everything the
// schema gained between them -- `containers` in M7, `node_meta` in M8 -- is a
// CREATE TABLE IF NOT EXISTS that runs on every open, so the tables are already
// there by the time this is called. What the bump actually buys is the OTHER
// direction: a build from before M8 now refuses a file this one has touched,
// instead of opening it, not knowing about node_meta, and leaving a cell's
// memory behind when the block there is broken -- which the next block put in
// that cell would inherit.
bool WorldDb::migrate(int from, std::string &err)
{
    if (!exec("BEGIN IMMEDIATE;", err)) return false;

    for (int step = from; step < kWorldSchemaVersion; ++step) {
        switch (step) {
        case 1:
            break;   // additive; the schema above has already done it
        case 2:
            // `players` keyed by name becomes `players` keyed by identity. The
            // old rows are dropped and not carried: a name cannot be turned
            // into a key, and no server before 3 ever wrote one -- the table
            // existed and save_player had no caller.
            if (!exec("DROP TABLE IF EXISTS players;", err) || !exec(kPlayersTable, err)) {
                std::string ignored;
                exec("ROLLBACK;", ignored);
                return false;
            }
            break;
        default:
            err = "this build has no way to bring a world file from schema version " +
                  std::to_string(step) + " to " + std::to_string(step + 1);
            exec("ROLLBACK;", err);
            return false;
        }
    }

    if (!set_meta("schema_version", std::to_string(kWorldSchemaVersion), err)) {
        std::string ignored;
        exec("ROLLBACK;", ignored);
        return false;
    }

    return exec("COMMIT;", err);
}

bool WorldDb::open(const std::string &path, uint32_t seed, NodeRegistry &blocks,
                   std::string &err)
{
    close();

    // No SQLITE_OPEN_URI: a path is a path here, and URI filenames would let a
    // world name carry query parameters that change how the file is opened.
    const int flags = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE;
    if (sqlite3_open_v2(path.c_str(), &m_db, flags, nullptr) != SQLITE_OK) {
        err = sqlite_error(m_db, "cannot open " + path);
        close();
        return false;
    }

    m_path = path;

    // What the file says it is, READ BEFORE this build's schema is applied to
    // it. The order used to be the other way round, and it meant a file from a
    // later build had this build's tables added to it on the way to being
    // refused: an open that fails must not change the file it refused.
    //
    // A file with no meta table at all is a new file. Asked of sqlite_master
    // rather than by trying a read and treating the failure as "new", because
    // those are different things and one of them is a file that has rotted.
    int found = -1;
    if (has_table("meta")) {
        std::string said;
        if (!get_meta("schema_version", said)) {
            // A meta table with no version in it. Every build since M2 writes
            // one, so this is not an older world -- it is a file that is not
            // one of ours, or one that has lost the row. Named, because
            // "cannot bring version 0 forward" sends somebody looking for a
            // migration that was never the problem.
            err = "world file has a meta table but no schema version: it was not made by this "
                  "engine, or the row is gone";
            close();
            return false;
        }
        found = std::atoi(said.c_str());

        if (found > kWorldSchemaVersion) {
            err = "world file has schema version " + std::to_string(found) +
                  ", this build writes " + std::to_string(kWorldSchemaVersion) +
                  " -- it was made by a later build, and this one would read it wrong";
            close();
            return false;
        }
    }

    if (!exec(kSchema, err) || !exec(kPlayersTable, err)) {
        close();
        return false;
    }

    std::string stored;

    if (found < 0) {
        // A new file.
        if (!set_meta("schema_version", std::to_string(kWorldSchemaVersion), err)) {
            close();
            return false;
        }
    } else if (found < kWorldSchemaVersion) {
        if (!migrate(found, err)) {
            close();
            return false;
        }
    }

    // Where a player looked, since 2026-09-19: three columns a file from before
    // grows at 0, which reads as "no look kept". No version bump, because a
    // build from before reads the file right -- it names the columns it wants.
    for (const char *col : {"look_x", "look_y", "look_z"}) {
        if (has_column("players", col)) continue;
        const std::string sql =
            std::string("ALTER TABLE players ADD COLUMN ") + col + " REAL NOT NULL DEFAULT 0;";
        if (!exec(sql.c_str(), err)) {
            close();
            return false;
        }
    }

    // The seed. A world file is the output of one generator run; opening it
    // under a different seed would mean the stored chunks and the ones
    // generated on demand come from two different worlds, which is a seam a
    // player walks into.
    if (get_meta("seed", stored)) {
        const uint32_t file_seed = (uint32_t)std::strtoul(stored.c_str(), nullptr, 10);
        if (file_seed != seed) {
            err = "world file was made with seed " + stored + ", this server has seed " +
                  std::to_string(seed);
            close();
            return false;
        }
    } else if (!set_meta("seed", std::to_string(seed), err)) {
        close();
        return false;
    }

    // The block table. Ids in a stored chunk mean whatever the table that wrote
    // them said they meant, and loading them under another one produces a
    // world made of the wrong blocks. The file keeps its table by name, so a
    // table that moved -- a mod added a block, and every id after it shifted --
    // is carried over into this server's instead of refused.
    const std::vector<std::string> names        = node_table(blocks);
    const uint64_t                 content_hash = registry_content_hash(blocks);
    const std::vector<std::string> file_names   = read_block_table(m_db);
    m_remapped                                  = 0;
    m_placeholders.clear();

    const bool has_hash = get_meta("content_hash", stored);
    if (has_hash && std::strtoull(stored.c_str(), nullptr, 10) != content_hash) {
        if (file_names.empty()) {
            err = "world file was made by a build with a different block table (file " + stored +
                  ", this server " + std::to_string(content_hash) +
                  ") and saved before block names were kept -- the stored chunks would be read "
                  "as the wrong blocks";
            close();
            return false;
        }
        if (!remap_blocks(file_names, blocks, err)) {
            close();
            return false;
        }
    } else if (!has_hash || file_names.empty()) {
        // A new file, or one from before the table was kept whose ids are this
        // server's: the table is written now, while it is known to be right.
        if (!exec("BEGIN IMMEDIATE;", err)) {
            close();
            return false;
        }
        if (!set_meta("content_hash", std::to_string(content_hash), err) ||
            !write_block_table(names, err) || !exec("COMMIT;", err)) {
            std::string ignored;
            exec("ROLLBACK;", ignored);
            close();
            return false;
        }
    }

    m_seed         = seed;
    m_content_hash = registry_content_hash(blocks);   // placeholders and all
    return true;
}

bool WorldDb::write_block_table(const std::vector<std::string> &names, std::string &err)
{
    if (!exec("DELETE FROM blocks;", err)) return false;

    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, "INSERT INTO blocks (id, name) VALUES (?, ?)", -1, &stmt,
                           nullptr) != SQLITE_OK) {
        err = sqlite_error(m_db, "prepare block table insert");
        return false;
    }
    bool ok = true;
    for (size_t id = 0; ok && id < names.size(); ++id) {
        sqlite3_reset(stmt);
        sqlite3_bind_int64(stmt, 1, (int64_t)id);
        sqlite3_bind_text(stmt, 2, names[id].c_str(), -1, SQLITE_TRANSIENT);
        ok = sqlite3_step(stmt) == SQLITE_DONE;
    }
    if (!ok) err = sqlite_error(m_db, "write block table");
    sqlite3_finalize(stmt);
    return ok;
}

bool WorldDb::remap_blocks(const std::vector<std::string> &from, NodeRegistry &to,
                           std::string &err)
{
    // Old id to new, looked up by name the first time a stored cell holds it.
    // A name `to` has no block for is registered as a placeholder then, and
    // only then: a mod taken out whose blocks were never placed leaves nothing
    // behind.
    std::vector<BlockId> map(from.size(), kAir);
    std::vector<uint8_t> known(from.size(), 0);
    std::set<std::string> rotten;
    auto carry = [&](BlockId id) -> BlockId {
        if (id >= from.size()) {
            rotten.insert("#" + std::to_string(id));
            return id;
        }
        if (!known[id]) {
            known[id] = 1;
            map[id]   = id == kAir ? kAir : to.find_or_alias(from[id]);
            if (map[id] == kAir && id != kAir) {
                map[id] = to.register_unknown(from[id]);
                m_placeholders.push_back(from[id]);
            }
        }
        return map[id];
    };

    if (!exec("BEGIN IMMEDIATE;", err)) return false;
    auto fail = [&]() {
        std::string ignored;
        exec("ROLLBACK;", ignored);
        return false;
    };

    // Every row read before any is written: an UPDATE under a SELECT still
    // stepping over the same table is a row that SELECT may meet again.
    struct Row {
        ChunkPos             p;
        std::vector<uint8_t> data;
    };
    std::vector<Row> rows;
    sqlite3_stmt    *stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, "SELECT x, y, z, data FROM chunks", -1, &stmt, nullptr) !=
        SQLITE_OK) {
        err = sqlite_error(m_db, "prepare chunk scan");
        return fail();
    }
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const auto *data = (const uint8_t *)sqlite3_column_blob(stmt, 3);
        const int   size = sqlite3_column_bytes(stmt, 3);
        rows.push_back({ChunkPos{sqlite3_column_int(stmt, 0), sqlite3_column_int(stmt, 1),
                                 sqlite3_column_int(stmt, 2)},
                        std::vector<uint8_t>(data, data + (size < 0 ? 0 : size))});
    }
    sqlite3_finalize(stmt);

    if (sqlite3_prepare_v2(m_db, "UPDATE chunks SET data = ? WHERE x = ? AND y = ? AND z = ?", -1,
                           &stmt, nullptr) != SQLITE_OK) {
        err = sqlite_error(m_db, "prepare chunk update");
        return fail();
    }
    for (const Row &row : rows) {
        // A row that will not read is left as it is: loading it reports it and
        // skips it, table or no table.
        ByteReader  r(row.data.data(), row.data.size());
        std::string chunk_err;
        auto        chunk = net::decode_chunk(r, chunk_err);
        if (!chunk) continue;

        bool changed = false;
        if (chunk->is_uniform()) {
            const BlockId was = chunk->uniform_block(), now = carry(was);
            if (now != was) {
                chunk->set_uniform_block(now);
                changed = true;
            }
        } else {
            for (int z = 0; z < kChunkSize; ++z)
                for (int y = 0; y < kChunkSize; ++y)
                    for (int x = 0; x < kChunkSize; ++x) {
                        const BlockId was = chunk->get(x, y, z), now = carry(was);
                        if (now != was) {
                            chunk->set(x, y, z, now);
                            changed = true;
                        }
                    }
        }
        if (!changed || !rotten.empty()) continue;   // nothing to write, or it all rolls back

        std::vector<uint8_t> blob;
        if (!net::encode_chunk(*chunk, blob, 3)) {
            err = "could not encode a chunk";
            sqlite3_finalize(stmt);
            return fail();
        }
        sqlite3_reset(stmt);
        sqlite3_bind_blob(stmt, 1, blob.data(), (int)blob.size(), SQLITE_TRANSIENT);
        sqlite3_bind_int(stmt, 2, row.p.x);
        sqlite3_bind_int(stmt, 3, row.p.y);
        sqlite3_bind_int(stmt, 4, row.p.z);
        if (sqlite3_step(stmt) != SQLITE_DONE) {
            err = sqlite_error(m_db, "rewrite chunk");
            sqlite3_finalize(stmt);
            return fail();
        }
        ++m_remapped;
    }
    sqlite3_finalize(stmt);

    if (!rotten.empty()) {
        err = "world file holds block ids past the end of its own table (" + *rotten.begin() +
              " of " + std::to_string(from.size()) + ") -- it has rotted";
        m_remapped = 0;
        return fail();
    }

    if (!set_meta("content_hash", std::to_string(registry_content_hash(to)), err) ||
        !write_block_table(node_table(to), err))
        return fail();
    return exec("COMMIT;", err);
}

void WorldDb::close()
{
    if (!m_db) return;
    sqlite3_close(m_db);
    m_db = nullptr;
}

bool WorldDb::set_meta(const std::string &key, const std::string &value, std::string &err)
{
    sqlite3_stmt *stmt = nullptr;
    const char   *sql  = "INSERT INTO meta (key, value) VALUES (?, ?) "
                         "ON CONFLICT(key) DO UPDATE SET value = excluded.value";
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        err = sqlite_error(m_db, "prepare meta insert");
        return false;
    }

    sqlite3_bind_text(stmt, 1, key.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, value.c_str(), -1, SQLITE_TRANSIENT);

    const bool ok = sqlite3_step(stmt) == SQLITE_DONE;
    if (!ok) err = sqlite_error(m_db, "write meta");
    sqlite3_finalize(stmt);
    return ok;
}

bool WorldDb::get_meta(const std::string &key, std::string &out) const
{
    out.clear();
    if (!m_db) return false;

    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, "SELECT value FROM meta WHERE key = ?", -1, &stmt, nullptr) !=
        SQLITE_OK)
        return false;

    sqlite3_bind_text(stmt, 1, key.c_str(), -1, SQLITE_TRANSIENT);

    bool found = false;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const unsigned char *text = sqlite3_column_text(stmt, 0);
        if (text) out = (const char *)text;
        found = true;
    }
    sqlite3_finalize(stmt);
    return found;
}

bool WorldDb::save_chunks(const World &world, const std::vector<ChunkPos> &positions,
                          std::string &err)
{
    if (!m_db) {
        err = "no world file open";
        return false;
    }
    if (positions.empty()) return true;

    // One transaction for the batch. Per-chunk transactions are roughly fifty
    // times slower, because each one is a disk sync.
    if (!exec("SAVEPOINT save", err)) return false;

    sqlite3_stmt *stmt = nullptr;
    const char   *sql  = "INSERT INTO chunks (x, y, z, data) VALUES (?, ?, ?, ?) "
                         "ON CONFLICT(x, y, z) DO UPDATE SET data = excluded.data";
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        err = sqlite_error(m_db, "prepare chunk insert");
        std::string ignored;
        exec("ROLLBACK TO save; RELEASE save", ignored);
        return false;
    }

    bool ok = true;
    for (const ChunkPos p : positions) {
        const Chunk *chunk = world.find_chunk(p);
        if (!chunk) continue;

        std::vector<uint8_t> blob;
        if (!net::encode_chunk(*chunk, blob, 3)) {
            err = "could not encode a chunk";
            ok  = false;
            break;
        }

        sqlite3_reset(stmt);
        sqlite3_bind_int(stmt, 1, p.x);
        sqlite3_bind_int(stmt, 2, p.y);
        sqlite3_bind_int(stmt, 3, p.z);
        sqlite3_bind_blob(stmt, 4, blob.data(), (int)blob.size(), SQLITE_TRANSIENT);

        if (sqlite3_step(stmt) != SQLITE_DONE) {
            err = sqlite_error(m_db, "write chunk");
            ok  = false;
            break;
        }
    }

    sqlite3_finalize(stmt);

    std::string commit_err;
    if (!ok) {
        exec("ROLLBACK TO save; RELEASE save", commit_err);
        return false;
    }
    if (!exec("RELEASE save", err)) return false;
    return true;
}

bool WorldDb::save_all_chunks(const World &world, std::string &err)
{
    std::vector<ChunkPos> positions;
    positions.reserve(world.chunk_count());
    for (const auto &kv : world.chunks()) positions.push_back(kv.first);
    return save_chunks(world, positions, err);
}

std::unique_ptr<Chunk> WorldDb::load_chunk(ChunkPos p, std::string &err) const
{
    if (!m_db) {
        err = "no world file open";
        return nullptr;
    }

    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, "SELECT data FROM chunks WHERE x = ? AND y = ? AND z = ?", -1,
                           &stmt, nullptr) != SQLITE_OK) {
        err = sqlite_error(m_db, "prepare chunk select");
        return nullptr;
    }

    sqlite3_bind_int(stmt, 1, p.x);
    sqlite3_bind_int(stmt, 2, p.y);
    sqlite3_bind_int(stmt, 3, p.z);

    std::unique_ptr<Chunk> chunk;

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const void *data = sqlite3_column_blob(stmt, 0);
        const int   size = sqlite3_column_bytes(stmt, 0);

        // Through the same decoder a chunk off the network goes through, with
        // the same refusals. A file is not more trustworthy than a socket: it
        // may have been written by a different version, corrupted by the disk,
        // or handed over by somebody else entirely.
        ByteReader r(data, (size_t)(size < 0 ? 0 : size));
        chunk = net::decode_chunk(r, err);
        if (!chunk) {
            err = "stored chunk is unreadable: " + err;
        } else if (!(chunk->pos() == p)) {
            // The row key and the blob disagree. See load_all_chunks: rubbish
            // decodes as a valid chunk at whatever position its first bytes
            // happen to spell.
            err = "the blob stored for this position says it belongs elsewhere";
            chunk.reset();
        }
    }

    sqlite3_finalize(stmt);
    return chunk;
}

std::vector<std::unique_ptr<Chunk>> WorldDb::load_column(int cx, int cz, std::string &err) const
{
    std::vector<std::unique_ptr<Chunk>> out;
    if (!m_db) {
        err = "no world file open";
        return out;
    }

    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, "SELECT y, data FROM chunks WHERE x = ? AND z = ?", -1, &stmt,
                           nullptr) != SQLITE_OK) {
        err = sqlite_error(m_db, "prepare column select");
        return out;
    }
    sqlite3_bind_int(stmt, 1, cx);
    sqlite3_bind_int(stmt, 2, cz);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const ChunkPos p{cx, sqlite3_column_int(stmt, 0), cz};
        const void    *data = sqlite3_column_blob(stmt, 1);
        const int      size = sqlite3_column_bytes(stmt, 1);

        ByteReader  r(data, (size_t)(size < 0 ? 0 : size));
        std::string chunk_err;
        auto        chunk = net::decode_chunk(r, chunk_err);
        if (!chunk || !(chunk->pos() == p)) {
            err = "chunk " + std::to_string(p.x) + "," + std::to_string(p.y) + "," +
                  std::to_string(p.z) + " is unreadable" +
                  (chunk ? ": it says it belongs elsewhere" : ": " + chunk_err);
            continue;
        }
        out.push_back(std::move(chunk));
    }

    sqlite3_finalize(stmt);
    return out;
}

int64_t WorldDb::load_all_chunks(World &world, std::string &err) const
{
    if (!m_db) {
        err = "no world file open";
        return -1;
    }

    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, "SELECT x, y, z, data FROM chunks", -1, &stmt, nullptr) !=
        SQLITE_OK) {
        err = sqlite_error(m_db, "prepare chunk scan");
        return -1;
    }

    int64_t loaded = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        ChunkPos p;
        p.x = sqlite3_column_int(stmt, 0);
        p.y = sqlite3_column_int(stmt, 1);
        p.z = sqlite3_column_int(stmt, 2);

        const void *data = sqlite3_column_blob(stmt, 3);
        const int   size = sqlite3_column_bytes(stmt, 3);

        ByteReader r(data, (size_t)(size < 0 ? 0 : size));
        std::string     chunk_err;
        auto            chunk = net::decode_chunk(r, chunk_err);
        if (!chunk) {
            // One unreadable chunk does not make the world unopenable. It is
            // reported and skipped, and the generator will fill the hole --
            // which beats refusing to start a server because one row rotted.
            err = "chunk " + std::to_string(p.x) + "," + std::to_string(p.y) + "," +
                  std::to_string(p.z) + " is unreadable: " + chunk_err;
            continue;
        }

        // The row says where this chunk is, and so does the blob inside it. If
        // they disagree, the blob is not the chunk this row is for, and
        // installing it would write over a different part of the world -- a
        // corrupted row taking a healthy chunk with it. The row key wins
        // because it is the thing the query found.
        //
        // Not hypothetical: sixty-four bytes of rubbish decode as a perfectly
        // valid uniform chunk at whatever position the first twelve bytes spell
        // out, which is how this was found.
        if (!(chunk->pos() == p)) {
            err = "chunk " + std::to_string(p.x) + "," + std::to_string(p.y) + "," +
                  std::to_string(p.z) + " holds a chunk that says it belongs at " +
                  std::to_string(chunk->pos().x) + "," + std::to_string(chunk->pos().y) + "," +
                  std::to_string(chunk->pos().z);
            continue;
        }

        net::install_chunk(world, std::move(chunk));
        ++loaded;
    }

    sqlite3_finalize(stmt);
    return loaded;
}

bool WorldDb::name_owner(const std::string &name, uint8_t out[net::kSignPublicBytes]) const
{
    if (!m_db) return false;

    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, "SELECT identity FROM names WHERE name = ?", -1, &stmt,
                           nullptr) != SQLITE_OK)
        return false;
    sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_TRANSIENT);

    bool found = false;
    if (sqlite3_step(stmt) == SQLITE_ROW &&
        sqlite3_column_bytes(stmt, 0) == (int)net::kSignPublicBytes) {
        std::memcpy(out, sqlite3_column_blob(stmt, 0), net::kSignPublicBytes);
        found = true;
    }
    sqlite3_finalize(stmt);
    return found;
}

bool WorldDb::claim_name(const std::string &name, const uint8_t identity[net::kSignPublicBytes],
                         int64_t when, std::string &err)
{
    if (!m_db) {
        err = "no world file open";
        return false;
    }

    sqlite3_stmt *stmt = nullptr;
    // OR IGNORE: whoever got there first keeps it, and a race between two
    // joins is decided by the file rather than by which check ran first.
    if (sqlite3_prepare_v2(m_db,
                           "INSERT OR IGNORE INTO names (name, identity, first_seen) "
                           "VALUES (?, ?, ?)",
                           -1, &stmt, nullptr) != SQLITE_OK) {
        err = sqlite_error(m_db, "prepare name insert");
        return false;
    }
    sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_blob(stmt, 2, identity, (int)net::kSignPublicBytes, SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 3, when);

    const bool ok = sqlite3_step(stmt) == SQLITE_DONE;
    if (!ok) err = sqlite_error(m_db, "write name");
    sqlite3_finalize(stmt);
    if (!ok || sqlite3_changes(m_db) != 1) return ok;

    // A name its owner freed: the new holder inherits what the old one had,
    // unless they already have a bag or a place of their own here.
    stmt = nullptr;
    std::vector<uint8_t> old;
    if (sqlite3_prepare_v2(m_db, "SELECT identity FROM freed_names WHERE name = ?", -1, &stmt,
                           nullptr) == SQLITE_OK) {
        sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_TRANSIENT);
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            const uint8_t *b = (const uint8_t *)sqlite3_column_blob(stmt, 0);
            old.assign(b, b + sqlite3_column_bytes(stmt, 0));
        }
    }
    sqlite3_finalize(stmt);
    if (old.empty()) return true;

    for (const char *sql : {"UPDATE OR IGNORE players SET identity = ?1 WHERE identity = ?2",
                            "UPDATE OR IGNORE player_items SET identity = ?1 WHERE identity = ?2",
                            "DELETE FROM freed_names WHERE name = ?3"}) {
        stmt = nullptr;
        if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
            err = sqlite_error(m_db, "hand a freed name over");
            return false;
        }
        // The same three for each; one a statement does not use is SQLITE_RANGE.
        sqlite3_bind_blob(stmt, 1, identity, (int)net::kSignPublicBytes, SQLITE_TRANSIENT);
        sqlite3_bind_blob(stmt, 2, old.data(), (int)old.size(), SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 3, name.c_str(), -1, SQLITE_TRANSIENT);
        const bool done = sqlite3_step(stmt) == SQLITE_DONE;
        sqlite3_finalize(stmt);
        if (!done) {
            err = sqlite_error(m_db, "hand a freed name over");
            return false;
        }
    }
    return true;
}

// The two a server's owner runs by hand, on a world no server holds.
namespace {

::sqlite3 *open_for_names(const std::string &path, std::string &err)
{
    ::sqlite3 *db = nullptr;
    if (sqlite3_open_v2(path.c_str(), &db, SQLITE_OPEN_READWRITE, nullptr) != SQLITE_OK ||
        sqlite3_exec(db, FREED_NAMES_TABLE, nullptr, nullptr, nullptr) != SQLITE_OK) {
        err = sqlite_error(db, "cannot open " + path);
        sqlite3_close(db);
        return nullptr;
    }
    return db;
}

} // namespace

bool WorldDb::list_names(const std::string &path, std::vector<NameRow> &out, std::string &err)
{
    ::sqlite3 *db = open_for_names(path, err);
    if (!db) return false;

    sqlite3_stmt *stmt = nullptr;
    const bool    ok   = sqlite3_prepare_v2(db,
                                            "SELECT name, identity, first_seen FROM names "
                                            "ORDER BY first_seen",
                                            -1, &stmt, nullptr) == SQLITE_OK;
    if (!ok) err = sqlite_error(db, "no names table");
    while (ok && sqlite3_step(stmt) == SQLITE_ROW) {
        NameRow r;
        r.name = (const char *)sqlite3_column_text(stmt, 0);
        if (sqlite3_column_bytes(stmt, 1) == (int)net::kSignPublicBytes)
            std::memcpy(r.identity, sqlite3_column_blob(stmt, 1), net::kSignPublicBytes);
        r.first_seen = sqlite3_column_int64(stmt, 2);
        out.push_back(std::move(r));
    }
    sqlite3_finalize(stmt);
    sqlite3_close(db);
    return ok;
}

bool WorldDb::free_name(const std::string &path, const std::string &name, std::string &err)
{
    ::sqlite3 *db = open_for_names(path, err);
    if (!db) return false;

    sqlite3_stmt *stmt = nullptr;
    bool ok = sqlite3_exec(db, "BEGIN", nullptr, nullptr, nullptr) == SQLITE_OK &&
              sqlite3_prepare_v2(db,
                                 "INSERT OR REPLACE INTO freed_names (name, identity) "
                                 "SELECT name, identity FROM names WHERE name = ?1",
                                 -1, &stmt, nullptr) == SQLITE_OK;
    if (ok) {
        sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_TRANSIENT);
        ok = sqlite3_step(stmt) == SQLITE_DONE;
    }
    sqlite3_finalize(stmt);
    if (ok && sqlite3_changes(db) != 1) {
        err = "nobody here has the name '" + name + "'";
        sqlite3_exec(db, "ROLLBACK", nullptr, nullptr, nullptr);
        sqlite3_close(db);
        return false;
    }

    stmt = nullptr;
    ok = ok && sqlite3_prepare_v2(db, "DELETE FROM names WHERE name = ?1", -1, &stmt,
                                  nullptr) == SQLITE_OK;
    if (ok) {
        sqlite3_bind_text(stmt, 1, name.c_str(), -1, SQLITE_TRANSIENT);
        ok = sqlite3_step(stmt) == SQLITE_DONE;
    }
    sqlite3_finalize(stmt);
    ok = ok && sqlite3_exec(db, "COMMIT", nullptr, nullptr, nullptr) == SQLITE_OK;
    if (!ok) {
        err = sqlite_error(db, "free the name");
        sqlite3_exec(db, "ROLLBACK", nullptr, nullptr, nullptr);
    }
    sqlite3_close(db);
    return ok;
}

bool WorldDb::save_player(const StoredPlayer &player, std::string &err)
{
    if (!m_db) {
        err = "no world file open";
        return false;
    }

    sqlite3_stmt *stmt = nullptr;
    const char   *sql =
        "INSERT INTO players (identity, name, x, y, z, last_seen, look_x, look_y, look_z) "
        "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?) "
        "ON CONFLICT(identity) DO UPDATE SET name = excluded.name, x = excluded.x, "
        "y = excluded.y, z = excluded.z, last_seen = excluded.last_seen, "
        "look_x = excluded.look_x, look_y = excluded.look_y, look_z = excluded.look_z";
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        err = sqlite_error(m_db, "prepare player insert");
        return false;
    }

    sqlite3_bind_blob(stmt, 1, player.identity, (int)net::kSignPublicBytes, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, player.name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_double(stmt, 3, player.x);
    sqlite3_bind_double(stmt, 4, player.y);
    sqlite3_bind_double(stmt, 5, player.z);
    sqlite3_bind_int64(stmt, 6, player.last_seen);
    sqlite3_bind_double(stmt, 7, player.look_x);
    sqlite3_bind_double(stmt, 8, player.look_y);
    sqlite3_bind_double(stmt, 9, player.look_z);

    const bool ok = sqlite3_step(stmt) == SQLITE_DONE;
    if (!ok) err = sqlite_error(m_db, "write player");
    sqlite3_finalize(stmt);
    return ok;
}

bool WorldDb::load_player(const uint8_t identity[net::kSignPublicBytes], StoredPlayer &out,
                          std::string &err) const
{
    out = StoredPlayer{};
    if (!m_db) {
        err = "no world file open";
        return false;
    }

    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(m_db,
                           "SELECT name, x, y, z, last_seen, look_x, look_y, look_z FROM players "
                           "WHERE identity = ?", -1,
                           &stmt, nullptr) != SQLITE_OK) {
        err = sqlite_error(m_db, "prepare player select");
        return false;
    }

    sqlite3_bind_blob(stmt, 1, identity, (int)net::kSignPublicBytes, SQLITE_TRANSIENT);

    bool found = false;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        std::memcpy(out.identity, identity, net::kSignPublicBytes);
        const unsigned char *name = sqlite3_column_text(stmt, 0);
        if (name) out.name = (const char *)name;
        out.x         = (float)sqlite3_column_double(stmt, 1);
        out.y         = (float)sqlite3_column_double(stmt, 2);
        out.z         = (float)sqlite3_column_double(stmt, 3);
        out.last_seen = sqlite3_column_int64(stmt, 4);
        out.look_x    = (float)sqlite3_column_double(stmt, 5);
        out.look_y    = (float)sqlite3_column_double(stmt, 6);
        out.look_z    = (float)sqlite3_column_double(stmt, 7);
        found         = true;
    }
    sqlite3_finalize(stmt);
    return found;
}

bool WorldDb::save_player_bag(const uint8_t identity[net::kSignPublicBytes], const std::string &name,
                             const ItemRegistry &items, const Inventory &bag, int64_t when,
                             std::string &err)
{
    if (!m_db) {
        err = "no world file open";
        return false;
    }

    const std::vector<uint8_t> blob = encode_inventory(items, bag);

    sqlite3_stmt *stmt = nullptr;
    const char   *sql =
        "INSERT INTO player_items (identity, name, data, last_seen) VALUES (?, ?, ?, ?) "
        "ON CONFLICT(identity) DO UPDATE SET name = excluded.name, data = excluded.data, "
        "last_seen = excluded.last_seen";
    if (sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        err = sqlite_error(m_db, "prepare bag insert");
        return false;
    }

    sqlite3_bind_blob(stmt, 1, identity, (int)net::kSignPublicBytes, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, name.c_str(), -1, SQLITE_TRANSIENT);
    sqlite3_bind_blob(stmt, 3, blob.data(), (int)blob.size(), SQLITE_TRANSIENT);
    sqlite3_bind_int64(stmt, 4, when);

    const bool ok = sqlite3_step(stmt) == SQLITE_DONE;
    if (!ok) err = sqlite_error(m_db, "write bag");
    sqlite3_finalize(stmt);
    return ok;
}

int64_t WorldDb::bag_seen(const uint8_t identity[net::kSignPublicBytes]) const
{
    if (!m_db) return -1;
    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, "SELECT last_seen FROM player_items WHERE identity = ?", -1,
                           &stmt, nullptr) != SQLITE_OK)
        return -1;
    sqlite3_bind_blob(stmt, 1, identity, (int)net::kSignPublicBytes, SQLITE_TRANSIENT);
    const int64_t seen = sqlite3_step(stmt) == SQLITE_ROW ? sqlite3_column_int64(stmt, 0) : -1;
    sqlite3_finalize(stmt);
    return seen;
}

bool WorldDb::load_player_bag(const uint8_t identity[net::kSignPublicBytes], ItemRegistry &items,
                              Inventory &bag, std::string &err) const
{
    if (!m_db) {
        err = "no world file open";
        return false;
    }

    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, "SELECT data FROM player_items WHERE identity = ?", -1, &stmt,
                           nullptr) != SQLITE_OK) {
        err = sqlite_error(m_db, "prepare bag select");
        return false;
    }

    sqlite3_bind_blob(stmt, 1, identity, (int)net::kSignPublicBytes, SQLITE_TRANSIENT);

    bool found = false;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        const uint8_t *blob = (const uint8_t *)sqlite3_column_blob(stmt, 0);
        const size_t   size = (size_t)sqlite3_column_bytes(stmt, 0);
        found               = decode_inventory(items, blob, size, bag, err);
    }
    sqlite3_finalize(stmt);
    return found;
}

namespace {

// One entry of a set that is saved whole, for telling whether the set changed
// since it was last written. Summed, so the map's order does not matter.
uint64_t entry_hash(int x, int y, int z, const void *a, size_t an, const void *b = nullptr,
                    size_t bn = 0)
{
    uint64_t h = 1469598103934665603ull;
    const auto mix = [&h](const void *p, size_t n) {
        const uint8_t *c = static_cast<const uint8_t *>(p);
        for (size_t i = 0; i < n; ++i) h = (h ^ c[i]) * 1099511628211ull;
        h = (h ^ 0xFF) * 1099511628211ull;   // a separator, so "ab"+"c" is not "a"+"bc"
    };
    const int xyz[3] = {x, y, z};
    mix(xyz, sizeof(xyz));
    mix(a, an);
    if (b) mix(b, bn);
    return h;
}

} // namespace

bool WorldDb::save_containers(const ItemRegistry &items, const ContainerSet &set,
                             std::string &err)
{
    if (!m_db) {
        err = "no world file open";
        return false;
    }

    // Rewritten whole, so not rewritten at all when nothing in it changed since
    // the last write: most saves of a world nobody is looting.
    std::vector<std::pair<ContainerAt, std::vector<uint8_t>>> rows;
    uint64_t sum = set.size();
    for (const ContainerAt &at : set.cells()) {
        const Inventory *bag = set.find(at);
        if (!bag) continue;
        rows.emplace_back(at, encode_inventory(items, *bag));
        sum += entry_hash(at.x, at.y, at.z, rows.back().second.data(), rows.back().second.size());
    }
    if (m_containers_saved && sum == m_containers_hash) return true;

    // Everything, in one transaction, and the DELETE is part of it. A container
    // that was broken since the last save has to leave the file, and a save
    // that only writes what is there would leave it behind to be loaded back
    // next time -- a chest that comes back from the dead holding what it held.
    if (!exec("SAVEPOINT save", err)) return false;

    bool ok = exec("DELETE FROM containers", err);

    sqlite3_stmt *stmt = nullptr;
    const char   *sql  = "INSERT INTO containers (x, y, z, data) VALUES (?, ?, ?, ?)";
    if (ok && sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        err = sqlite_error(m_db, "prepare container insert");
        ok  = false;
    }

    if (ok) {
        for (const auto &[at, blob] : rows) {

            sqlite3_reset(stmt);
            sqlite3_bind_int(stmt, 1, at.x);
            sqlite3_bind_int(stmt, 2, at.y);
            sqlite3_bind_int(stmt, 3, at.z);
            sqlite3_bind_blob(stmt, 4, blob.data(), (int)blob.size(), SQLITE_TRANSIENT);

            if (sqlite3_step(stmt) != SQLITE_DONE) {
                err = sqlite_error(m_db, "write container");
                ok  = false;
                break;
            }
        }
    }

    if (stmt) sqlite3_finalize(stmt);

    std::string rollback_err;
    if (!ok) {
        exec("ROLLBACK TO save; RELEASE save", rollback_err);
        return false;
    }
    if (!exec("RELEASE save", err)) return false;
    m_containers_saved = true;
    m_containers_hash  = sum;
    return true;
}

bool WorldDb::save_node_meta(const NodeMetaSet &set, std::string &err)
{
    if (!m_db) {
        err = "no world file open";
        return false;
    }

    // The same shape save_containers has, and for the same reason: a field a
    // mod erased has to LEAVE the file. A save that only wrote what is there
    // would load the erased one back next time, and a cell that remembers
    // something nobody wrote is worse than one that remembers nothing.
    //
    // And like it, not written when nothing changed since the last write.
    const std::vector<NodeMetaSet::Field> fields = set.all();
    uint64_t                              sum    = fields.size();
    for (const NodeMetaSet::Field &f : fields)
        sum += entry_hash(f.at.x, f.at.y, f.at.z, f.key.data(), f.key.size(), f.value.data(),
                          f.value.size());
    if (m_meta_saved && sum == m_meta_hash) return true;

    if (!exec("SAVEPOINT save", err)) return false;

    bool ok = exec("DELETE FROM node_meta", err);

    sqlite3_stmt *stmt = nullptr;
    const char   *sql =
        "INSERT INTO node_meta (x, y, z, key, value) VALUES (?, ?, ?, ?, ?)";
    if (ok && sqlite3_prepare_v2(m_db, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        err = sqlite_error(m_db, "prepare node_meta insert");
        ok  = false;
    }

    if (ok) {
        for (const NodeMetaSet::Field &f : fields) {
            sqlite3_reset(stmt);
            sqlite3_bind_int(stmt, 1, f.at.x);
            sqlite3_bind_int(stmt, 2, f.at.y);
            sqlite3_bind_int(stmt, 3, f.at.z);
            sqlite3_bind_text(stmt, 4, f.key.c_str(), (int)f.key.size(), SQLITE_TRANSIENT);
            sqlite3_bind_text(stmt, 5, f.value.c_str(), (int)f.value.size(), SQLITE_TRANSIENT);

            if (sqlite3_step(stmt) != SQLITE_DONE) {
                err = sqlite_error(m_db, "write node_meta");
                ok  = false;
                break;
            }
        }
    }

    if (stmt) sqlite3_finalize(stmt);

    std::string rollback_err;
    if (!ok) {
        exec("ROLLBACK TO save; RELEASE save", rollback_err);
        return false;
    }
    if (!exec("RELEASE save", err)) return false;
    m_meta_saved = true;
    m_meta_hash  = sum;
    return true;
}

int64_t WorldDb::load_node_meta(NodeMetaSet &set, std::string &err) const
{
    if (!m_db) {
        err = "no world file open";
        return 0;
    }

    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, "SELECT x, y, z, key, value FROM node_meta", -1, &stmt,
                           nullptr) != SQLITE_OK) {
        err = sqlite_error(m_db, "prepare node_meta select");
        return 0;
    }

    std::vector<NodeMetaSet::Field> rows;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        NodeMetaSet::Field f;
        f.at.x = sqlite3_column_int(stmt, 0);
        f.at.y = sqlite3_column_int(stmt, 1);
        f.at.z = sqlite3_column_int(stmt, 2);

        const char *k = (const char *)sqlite3_column_text(stmt, 3);
        const char *v = (const char *)sqlite3_column_text(stmt, 4);
        f.key   = k ? k : "";
        f.value = v ? v : "";
        rows.push_back(std::move(f));
    }
    sqlite3_finalize(stmt);

    // Through set(), so the limits apply to a file the same way they apply to a
    // mod. A file written by a build with wider limits loads what fits, and the
    // count coming back is what the caller reports -- silently keeping more
    // than a world may hold is how a limit becomes a comment.
    const size_t took = set.load(rows);
    if (took != rows.size())
        err = "node_meta: " + std::to_string(rows.size() - took) + " field(s) of " +
              std::to_string(rows.size()) + " did not fit this world's limits";
    return (int64_t)took;
}

bool WorldDb::save_mod_store(const std::vector<ModStore::Row> &changed, std::string &err)
{
    if (!m_db) {
        err = "no world file open";
        return false;
    }
    if (changed.empty()) return true;

    // The keys that changed and nothing else, in one transaction: an empty
    // value is a key a mod erased, and it has to leave the file.
    if (!exec("SAVEPOINT save", err)) return false;

    sqlite3_stmt *put  = nullptr;
    sqlite3_stmt *drop = nullptr;
    bool          ok =
        sqlite3_prepare_v2(m_db,
                           "INSERT INTO mod_store (mod, key, value) VALUES (?, ?, ?) "
                           "ON CONFLICT(mod, key) DO UPDATE SET value = excluded.value",
                           -1, &put, nullptr) == SQLITE_OK &&
        sqlite3_prepare_v2(m_db, "DELETE FROM mod_store WHERE mod = ? AND key = ?", -1, &drop,
                           nullptr) == SQLITE_OK;
    if (!ok) err = sqlite_error(m_db, "prepare mod_store write");

    for (size_t i = 0; ok && i < changed.size(); ++i) {
        const ModStore::Row &r    = changed[i];
        sqlite3_stmt        *stmt = r.value.empty() ? drop : put;
        sqlite3_reset(stmt);
        sqlite3_bind_text(stmt, 1, r.mod.c_str(), (int)r.mod.size(), SQLITE_TRANSIENT);
        sqlite3_bind_text(stmt, 2, r.key.c_str(), (int)r.key.size(), SQLITE_TRANSIENT);
        if (!r.value.empty())
            sqlite3_bind_text(stmt, 3, r.value.c_str(), (int)r.value.size(), SQLITE_TRANSIENT);
        if (sqlite3_step(stmt) != SQLITE_DONE) {
            err = sqlite_error(m_db, "write mod_store");
            ok  = false;
        }
    }

    if (put) sqlite3_finalize(put);
    if (drop) sqlite3_finalize(drop);

    std::string rollback_err;
    if (!ok) {
        exec("ROLLBACK TO save; RELEASE save", rollback_err);
        return false;
    }
    return exec("RELEASE save", err);
}

int64_t WorldDb::load_mod_store(ModStore &store, std::string &err) const
{
    if (!m_db) {
        err = "no world file open";
        return 0;
    }

    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, "SELECT mod, key, value FROM mod_store", -1, &stmt, nullptr) !=
        SQLITE_OK) {
        err = sqlite_error(m_db, "prepare mod_store select");
        return 0;
    }

    // Through set(), so a file obeys the limits a mod does.
    int64_t     took = 0, rows = 0;
    std::string why;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        const char *m = (const char *)sqlite3_column_text(stmt, 0);
        const char *k = (const char *)sqlite3_column_text(stmt, 1);
        const char *v = (const char *)sqlite3_column_text(stmt, 2);
        ++rows;
        if (store.set(m ? m : "", k ? k : "", v ? v : "", why)) ++took;
    }
    sqlite3_finalize(stmt);
    store.take_touched();   // what the file holds is not a change to write back

    if (took != rows)
        err = "mod_store: " + std::to_string(rows - took) + " key(s) of " +
              std::to_string(rows) + " refused: " + why;
    return took;
}

int64_t WorldDb::load_containers(ItemRegistry &items, ContainerSet &set, std::string &err) const
{
    if (!m_db) {
        err = "no world file open";
        return 0;
    }

    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(m_db, "SELECT x, y, z, data FROM containers", -1, &stmt, nullptr) !=
        SQLITE_OK) {
        err = sqlite_error(m_db, "prepare container select");
        return 0;
    }

    int64_t loaded = 0;
    while (sqlite3_step(stmt) == SQLITE_ROW) {
        ContainerAt at;
        at.x = sqlite3_column_int(stmt, 0);
        at.y = sqlite3_column_int(stmt, 1);
        at.z = sqlite3_column_int(stmt, 2);

        const uint8_t *blob = (const uint8_t *)sqlite3_column_blob(stmt, 3);
        const size_t   size = (size_t)sqlite3_column_bytes(stmt, 3);

        // The size first, because a container's slot count is decided when it
        // is made and never changed afterwards -- so it has to be known one
        // step before the contents that go in it.
        const int slots = stored_slots(blob, size);
        if (slots <= 0) {
            err = "a container row that is not an inventory, at " + std::to_string(at.x) + " " +
                  std::to_string(at.y) + " " + std::to_string(at.z);
            continue;
        }

        std::string open_err;
        Inventory  *bag = set.open(at, slots, open_err);
        if (!bag) {
            err = "could not make the container at " + std::to_string(at.x) + " " +
                  std::to_string(at.y) + " " + std::to_string(at.z) + ": " + open_err;
            continue;
        }

        std::string decode_err;
        if (!decode_inventory(items, blob, size, *bag, decode_err)) {
            // The container stays, empty, rather than being erased. A row that
            // will not parse says the contents are unreadable; it does not say
            // the chest was never there, and a world that answers a rotted row
            // by deleting the block would be losing more than the row did.
            err = "the container at " + std::to_string(at.x) + " " + std::to_string(at.y) + " " +
                  std::to_string(at.z) + " would not read: " + decode_err;
            continue;
        }

        ++loaded;
    }

    sqlite3_finalize(stmt);
    return loaded;
}

WorldDbStats WorldDb::stats() const
{
    WorldDbStats s;
    if (!m_db) return s;

    sqlite3_stmt *stmt = nullptr;
    if (sqlite3_prepare_v2(m_db,
                           "SELECT (SELECT COUNT(*) FROM chunks), (SELECT COUNT(*) FROM players), "
                           "(SELECT COUNT(*) FROM containers), "
                           "(SELECT page_count * page_size FROM pragma_page_count(), "
                           "pragma_page_size())",
                           -1, &stmt, nullptr) == SQLITE_OK) {
        if (sqlite3_step(stmt) == SQLITE_ROW) {
            s.chunks        = sqlite3_column_int64(stmt, 0);
            s.players       = sqlite3_column_int64(stmt, 1);
            s.containers    = sqlite3_column_int64(stmt, 2);
            s.bytes_on_disk = sqlite3_column_int64(stmt, 3);
        }
        sqlite3_finalize(stmt);
    }
    return s;
}

} // namespace cnp
