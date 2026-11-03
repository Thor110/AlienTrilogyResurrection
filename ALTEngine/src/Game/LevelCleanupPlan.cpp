#include "LevelCleanupPlan.h"

#include "CreatureSight.h"
#include "LevelFileLayout.h"
#include "LevelOcclusion.h"
#include "PlayerMover.h"
#include "../Formats/LevelLoader.h"
#include "../Formats/RenderMesh.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <exception>
#include <map>
#include <set>

namespace ALTEngine::Game::LevelCleanupPlan
{
    using namespace LevelFileLayout;

    namespace
    {
        struct V3 { double x, y, z; };

        // Every cell the player can walk to.
        std::vector<uint8_t> Reachable(const Formats::LevelGeometry& level, bool multiplayer, bool skipFirstNode)
        {
            const int length = level.header.mapLength;
            std::vector<uint8_t> reach(level.collisionGrid.size(), 0);
            std::vector<int> queue;
            auto seed = [&](int x, int z) {
                const int i = Formats::CellIndex(level, x, z);
                if (i >= 0 && !reach[size_t(i)]) { reach[size_t(i)] = 1; queue.push_back(i); }
            };
            // A multiplayer map is started at a path node, never at the
            // header's start (MultiplayerSpawn; Level Select does the same):
            // not node 0 on L909, nor a node in a wall cell, which the spawn
            // draws again. L909's header start is a sealed holding room.
            if (!multiplayer) { seed(level.header.playerStartX, level.header.playerStartY); }
            for (size_t k = 0; multiplayer && k < level.pathNodes.size(); ++k)
            {
                if (k == 0 && skipFirstNode && level.pathNodes.size() > 1) { continue; }
                const auto& n = level.pathNodes[k];
                const int i = Formats::CellIndex(level, n.x, n.y);
                if (i >= 0 && PlayerMover::WallAttribute(level.collisionGrid[size_t(i)].attribute)) { continue; }
                seed(n.x, n.y);
            }
            for (size_t head = 0; head < queue.size(); ++head)
            {
                const int i = queue[head], x = i % length, z = i / length;
                static constexpr int STEP[4][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };
                for (const auto& d : STEP)
                {
                    const int j = Formats::CellIndex(level, x + d[0], z + d[1]);
                    if (j < 0 || reach[size_t(j)]) { continue; }
                    if (PlayerMover::WallAttribute(level.collisionGrid[size_t(j)].attribute)) { continue; }
                    reach[size_t(j)] = 1;
                    queue.push_back(j);
                }
            }
            return reach;
        }

        // Steps 1, 2 and 5.
        bool OutOfViewAndFacingAway(const std::vector<uint8_t>& bytes, const Layout& lay, const Formats::LevelGeometry& level,
                                    const std::vector<uint8_t>& reach, LevelCleanup::Plan& plan)
        {
            const size_t recordCount = lay.recordCount, cells = lay.cells;
            const int mapLength = lay.mapLength, mapWidth = lay.mapWidth;
            auto recordA = [&](size_t r) { return lay.RecordA(bytes, r); };
            auto cellStart = [&](size_t c) { return lay.CellStart(bytes, c); };
            auto forRun = [&](size_t start, auto&& visit) {
                for (size_t r = start; r < recordCount; ++r)
                {
                    if (recordA(r) == -1) { break; }
                    visit(r);
                    if (lay.Light(bytes, r) & RUN_END) { break; }
                }
            };

            // Each record's planes, front side out (the side its winding
            // shows; floors come out facing up). A quad that is not flat shows
            // a different side on each of its triangles - the renderer's a-b-c
            // / a-c-d, or the other split's a-b-d / b-c-d - so it is seen when
            // any of them is. Faces drawn from both sides, or with no area, get
            // no plane and are judged by their cells alone.
            struct Plane { double nx = 0, ny = 0, nz = 0, d = 0; };
            struct Planes { std::array<Plane, 4> p{}; int count = 0; bool culled = false; };
            std::vector<Planes> planes(recordCount);
            for (size_t r = 1; r < recordCount; ++r)
            {
                if (recordA(r) == -1 || !Formats::ClassifyDrawRoutine(lay.Routine(bytes, r)).culled) { continue; }
                const int n = lay.Corner(bytes, r, 3) < 0 ? 3 : 4;
                std::array<V3, 4> v{};
                bool bad = false;
                for (int k = 0; k < n; ++k)
                {
                    const int32_t i = lay.Corner(bytes, r, k);
                    if (i < 0 || i >= lay.vertexCount) { bad = true; break; }
                    v[size_t(k)] = { double(lay.Coordinate(bytes, size_t(i), 0)), double(lay.Coordinate(bytes, size_t(i), 1)),
                                     double(lay.Coordinate(bytes, size_t(i), 2)) };
                }
                if (bad) { continue; }
                static constexpr int SPLITS[4][3] = { { 0, 1, 2 }, { 0, 2, 3 }, { 0, 1, 3 }, { 1, 2, 3 } };
                Planes& pl = planes[r];
                for (int t = 0; t < (n == 4 ? 4 : 1); ++t)
                {
                    const V3& A = v[size_t(SPLITS[t][0])];
                    const V3& B = v[size_t(SPLITS[t][1])];
                    const V3& C = v[size_t(SPLITS[t][2])];
                    const double ux = B.x - A.x, uy = B.y - A.y, uz = B.z - A.z;
                    const double wx = C.x - A.x, wy = C.y - A.y, wz = C.z - A.z;
                    Plane q{ uy * wz - uz * wy, uz * wx - ux * wz, ux * wy - uy * wx, 0.0 };
                    const double len = std::sqrt(q.nx * q.nx + q.ny * q.ny + q.nz * q.nz);
                    if (len < 1e-6) { continue; }
                    q.nx /= len; q.ny /= len; q.nz /= len;
                    q.d = q.nx * A.x + q.ny * A.y + q.nz * A.z;
                    pl.p[size_t(pl.count++)] = q;
                }
                pl.culled = pl.count > 0;
            }

            // Where the eye can be in a cell the player reaches: the cell's
            // square, from under its floor to over its ceiling (a lift's
            // cells to over its top). Generous on purpose - it only keeps more.
            std::vector<int> topExtra(cells, 0), bottomExtra(cells, 0);
            for (const auto& lift : level.lifts)
            {
                for (int dz = 0; dz < std::max<int>(1, lift.depth); ++dz)
                {
                    for (int dx = 0; dx < std::max<int>(1, lift.width); ++dx)
                    {
                        const int i = Formats::CellIndex(level, lift.x + dx, lift.y + dz);
                        if (i < 0) { continue; }
                        topExtra[size_t(i)] = std::max(topExtra[size_t(i)], int(lift.travel) * 32 + 0x30 * 32);
                        bottomExtra[size_t(i)] = std::max(bottomExtra[size_t(i)], int(lift.travel) * 32);
                    }
                }
            }
            constexpr double CELL = 512.0, HEIGHT_MARGIN = 256.0, IN_FRONT = 1.0;
            auto seesFront = [&](const Planes& pl, size_t eyeCell) {
                const auto& c = level.collisionGrid[eyeCell];
                const double x0 = (double(eyeCell % size_t(mapLength)) - mapLength / 2.0) * CELL;
                const double z0 = (double(eyeCell / size_t(mapLength)) - mapWidth / 2.0) * CELL;
                const double y0 = double(c.floorHeight) * 32.0 - HEIGHT_MARGIN - bottomExtra[eyeCell];
                // Over the ceiling, or a standing eye with a Modern jump, if higher.
                const double y1 = std::max(double(std::max(c.ceilingHeight, c.floorHeight)) * 32.0 + HEIGHT_MARGIN,
                                           double(c.floorHeight) * 32.0 + 0x300 + 128 + 256) + topExtra[eyeCell];
                for (int i = 0; i < pl.count; ++i)
                {
                    const Plane& q = pl.p[size_t(i)];
                    const double best = q.nx * (q.nx > 0 ? x0 + CELL : x0) + q.ny * (q.ny > 0 ? y1 : y0)
                                      + q.nz * (q.nz > 0 ? z0 + CELL : z0);
                    if (best > q.d + IN_FRONT) { return true; }
                }
                return false;
            };

            // Each reachable cell's view walk: the cells it draws are
            // drawable, and each face of theirs that faces the eye's cell is seen.
            std::vector<uint8_t> drawable(cells, 0), seen(recordCount, 0);
            for (size_t i = 0; i < cells; ++i)
            {
                if (!reach[i]) { continue; }
                const auto walk = CreatureSight::DrawnCells(level, int(i % size_t(mapLength)), int(i / size_t(mapLength)), 0, 0x800);
                for (size_t k = 0; k < cells && k < walk.size(); ++k)
                {
                    if (!walk[k]) { continue; }
                    drawable[k] = 1;
                    const uint32_t start = cellStart(k);
                    if (start == NO_RUN || start >= recordCount) { continue; }
                    forRun(start == 0 ? 1 : start, [&](size_t r) {
                        if (!seen[r] && (!planes[r].culled || seesFront(planes[r], i))) { seen[r] = 1; }
                    });
                }
            }

            std::vector<int> owners(recordCount, 0);
            std::vector<uint8_t> inView(recordCount, 0);
            for (size_t c = 0; c < cells; ++c)
            {
                const uint32_t start = cellStart(c);
                if (start == NO_RUN || start == 0 || start >= recordCount) { continue; }
                forRun(start, [&](size_t r) { ++owners[r]; if (drawable[c]) { inView[r] = 1; } });
            }

            // The first cell: the first cell with start 0, when records 1..
            // are unlisted and end in the run bit.
            size_t firstCell = cells;
            for (size_t c = 0; c < cells; ++c) { if (cellStart(c) == 0) { firstCell = c; break; } }
            int firstRun = 0;
            if (firstCell < cells && recordCount > 1 && owners[1] == 0 && recordA(1) != -1)
            {
                bool ended = false;
                for (size_t r = 1; r < recordCount && !ended; ++r)
                {
                    if (owners[r] != 0 || recordA(r) == -1) { break; }
                    ++firstRun;
                    ended = (lay.Light(bytes, r) & RUN_END) != 0;
                }
                if (!ended) { firstRun = 0; }
            }

            std::set<uint32_t> remove;
            for (size_t r = 1; r < recordCount; ++r)
            {
                if (owners[r] == 0) { continue; }
                if (!inView[r]) { remove.insert(uint32_t(r)); ++plan.outOfView; }
                else if (!seen[r]) { remove.insert(uint32_t(r)); ++plan.facingAway; }
            }
            if (firstRun > 0)
            {
                plan.firstCell = int(firstCell);
                for (int k = 1; k <= firstRun; ++k)
                {
                    if (!drawable[firstCell]) { remove.insert(uint32_t(k)); ++plan.outOfView; }
                    else if (!seen[size_t(k)]) { remove.insert(uint32_t(k)); ++plan.facingAway; }
                }
            }
            plan.remove.assign(remove.begin(), remove.end());
            for (size_t c = 0; c < cells; ++c)
            {
                const uint32_t start = cellStart(c);
                if (!drawable[c] && start != NO_RUN) { plan.clear.push_back(uint32_t(c)); }
            }
            return true;
        }

        // Step 3. Returns the records of every lift that can go.
        std::vector<uint32_t> Lifts(const std::vector<uint8_t>& bytes, const Layout& lay, const std::vector<uint8_t>& reach,
                                    const std::vector<uint8_t>& solid, const std::vector<uint16_t>& doorTextures,
                                    const std::vector<uint32_t>& roomDoors, std::vector<uint32_t>& roomsOut)
        {
            std::vector<uint32_t> out;
            if (doorTextures.empty()) { return out; }
            const size_t recordCount = lay.recordCount, cells = lay.cells;

            // Each record's cell (the run that lists it) and corners.
            std::vector<int> owner(recordCount, -1);
            for (size_t c = 0; c < cells; ++c)
            {
                const uint32_t start = lay.CellStart(bytes, c);
                if (start == NO_RUN || start == 0 || start >= recordCount) { continue; }
                for (size_t r = start; r < recordCount; ++r)
                {
                    if (lay.RecordA(bytes, r) == -1) { break; }
                    owner[r] = int(c);
                    if (lay.Light(bytes, r) & RUN_END) { break; }
                }
            }
            std::vector<std::vector<V3>> corners(recordCount);
            for (size_t r = 1; r < recordCount; ++r)
            {
                if (lay.RecordA(bytes, r) == -1) { continue; }
                for (int k = 0; k < 4; ++k)
                {
                    const int32_t i = lay.Corner(bytes, r, k);
                    if (i < 0 || i >= lay.vertexCount) { continue; }
                    corners[r].push_back({ double(lay.Coordinate(bytes, size_t(i), 0)), double(lay.Coordinate(bytes, size_t(i), 1)),
                                           double(lay.Coordinate(bytes, size_t(i), 2)) });
                }
            }
            auto isDoor = [&](size_t r) {
                return !corners[r].empty() && std::find(doorTextures.begin(), doorTextures.end(), lay.Texture(bytes, r)) != doorTextures.end();
            };
            // A door tile hides what is behind it: opaque, or a cut-out with
            // no clear texel.
            auto doorSolid = [&](size_t r) {
                const auto dr = Formats::ClassifyDrawRoutine(lay.Routine(bytes, r));
                return (dr.fill == Formats::FaceFill::Opaque && !dr.flat) || (r < solid.size() && solid[r]);
            };

            // Faces sharing a corner (within SLACK), by 8-unit buckets.
            constexpr double SLACK = 3.0, BUCKET = 8.0;
            using Key = std::array<int, 3>;
            auto key = [&](const V3& p) { return Key{ int(std::floor(p.x / BUCKET)), int(std::floor(p.y / BUCKET)), int(std::floor(p.z / BUCKET)) }; };
            std::map<Key, std::vector<uint32_t>> buckets;
            for (size_t r = 1; r < recordCount; ++r) { for (const V3& p : corners[r]) { buckets[key(p)].push_back(uint32_t(r)); } }
            auto touching = [&](size_t r, auto&& visit) {
                std::set<uint32_t> done;
                for (const V3& p : corners[r])
                {
                    const Key k = key(p);
                    for (int dx = -1; dx <= 1; ++dx)
                    {
                        for (int dy = -1; dy <= 1; ++dy)
                        {
                            for (int dz = -1; dz <= 1; ++dz)
                            {
                                const auto it = buckets.find(Key{ k[0] + dx, k[1] + dy, k[2] + dz });
                                if (it == buckets.end()) { continue; }
                                for (uint32_t j : it->second)
                                {
                                    if (j == r || done.count(j)) { continue; }
                                    for (const V3& q : corners[j])
                                    {
                                        const double d2 = (q.x - p.x) * (q.x - p.x) + (q.y - p.y) * (q.y - p.y) + (q.z - p.z) * (q.z - p.z);
                                        if (d2 <= SLACK * SLACK) { done.insert(j); visit(size_t(j)); break; }
                                    }
                                }
                            }
                        }
                    }
                }
            };

            // The doors: door tiles joined corner to corner.
            std::vector<int> door(recordCount, -1);
            int doors = 0;
            for (size_t r = 1; r < recordCount; ++r)
            {
                if (!isDoor(r) || door[r] >= 0) { continue; }
                std::vector<size_t> stack{ r };
                door[r] = doors;
                while (!stack.empty())
                {
                    const size_t a = stack.back();
                    stack.pop_back();
                    touching(a, [&](size_t j) { if (isDoor(j) && door[j] < 0) { door[j] = doors; stack.push_back(j); } });
                }
                ++doors;
            }
            constexpr size_t MOST_FACES = 120;      // a lift is 74 (L321-L325); anything bigger is not one
            constexpr double DEEPEST = 2600.0;      // and runs 2048 behind its door
            for (int g = 0; g < doors; ++g)
            {
                std::vector<size_t> tiles;
                for (size_t r = 1; r < recordCount; ++r) { if (door[r] == g) { tiles.push_back(r); } }
                // A door named by hand (a sealed room's): no size limit.
                bool room = false;
                for (size_t r : tiles) { room = room || std::find(roomDoors.begin(), roomDoors.end(), uint32_t(r)) != roomDoors.end(); }
                bool allSolid = true;
                double nx = 0, ny = 0, nz = 0, cx = 0, cy = 0, cz = 0;
                int m = 0;
                for (size_t r : tiles)
                {
                    allSolid = allSolid && doorSolid(r);
                    const auto& p = corners[r];
                    for (size_t i = 0; i < p.size(); ++i)   // Newell's normal: the side the tiles show
                    {
                        const V3& A = p[i];
                        const V3& B = p[(i + 1) % p.size()];
                        nx += (A.y - B.y) * (A.z + B.z); ny += (A.z - B.z) * (A.x + B.x); nz += (A.x - B.x) * (A.y + B.y);
                        cx += A.x; cy += A.y; cz += A.z; ++m;
                    }
                }
                const double len = std::sqrt(nx * nx + ny * ny + nz * nz);
                if (!allSolid || len < 1e-6 || m == 0) { continue; }
                nx /= len; ny /= len; nz /= len;
                const double d = (nx * cx + ny * cy + nz * cz) / m;
                auto side = [&](const V3& p) { return nx * p.x + ny * p.y + nz * p.z - d; };
                // Behind the door: no corner in front of its plane, one past it.
                auto behind = [&](size_t r) {
                    bool past = false;
                    for (const V3& p : corners[r])
                    {
                        const double s = side(p);
                        if (s > SLACK) { return false; }
                        if (s < -1.0) { past = true; }
                    }
                    return past;
                };
                std::set<size_t> got;
                std::vector<size_t> stack;
                bool tooBig = false;
                auto take = [&](size_t j) {
                    if (tooBig || isDoor(j) || got.count(j) || !behind(j)) { return; }
                    got.insert(j);
                    stack.push_back(j);
                    if (!room && got.size() > MOST_FACES) { tooBig = true; }
                };
                for (size_t r : tiles) { touching(r, take); }
                while (!stack.empty() && !tooBig)
                {
                    const size_t a = stack.back();
                    stack.pop_back();
                    touching(a, take);
                }
                if (tooBig || got.empty()) { continue; }
                bool ok = true;
                for (size_t r : got)
                {
                    if (owner[r] >= 0 && reach[size_t(owner[r])]) { ok = false; break; }
                    for (const V3& p : corners[r]) { if (!room && -side(p) > DEEPEST) { ok = false; } }
                }
                if (!ok) { continue; }
                for (size_t r : got) { (room ? roomsOut : out).push_back(uint32_t(r)); }
            }
            std::sort(out.begin(), out.end());
            out.erase(std::unique(out.begin(), out.end()), out.end());
            return out;
        }
    }

    bool Make(const std::vector<uint8_t>& bytes, const Options& options, LevelCleanup::Plan& plan, std::string& error,
              Details* details)
    {
        if (details) { *details = {}; }
        plan = {};
        Layout lay;
        if (!Read(bytes, lay, error)) { return false; }
        Formats::LevelGeometry level;
        try { level = Formats::LevelLoader::Parse(bytes, "level"); }
        catch (const std::exception& e) { error = e.what(); return false; }
        if (level.collisionGrid.size() != lay.cells) { error = "the cell grid is not where the header puts it"; return false; }

        const auto reach = Reachable(level, options.multiplayer, options.skipFirstNode);
        if (!OutOfViewAndFacingAway(bytes, lay, level, reach, plan)) { error = "could not judge the faces"; return false; }

        std::set<uint32_t> remove(plan.remove.begin(), plan.remove.end());

        // The faces LevelOcclusion finds hidden once `plan` is applied, as the
        // input numbers them: of `only` (input records) if given, else of all.
        auto hiddenAfter = [&](const std::set<uint32_t>* only, bool strictWalk, std::vector<uint32_t>& found) {
            std::vector<uint8_t> work = bytes;
            LevelCleanup::Report report;
            std::vector<uint32_t> from;
            if (!LevelCleanup::Apply(work, plan, report, error, &from)) { return false; }
            Layout now;
            if (!Read(work, now, error)) { return false; }
            Formats::LevelGeometry left;
            try { left = Formats::LevelLoader::Parse(work, "level"); }
            catch (const std::exception& e) { error = e.what(); return false; }
            std::vector<uint8_t> records(work.begin() + std::ptrdiff_t(now.records), work.begin() + std::ptrdiff_t(now.grid));
            std::vector<int16_t> xyz(size_t(now.vertexCount) * 3);
            for (size_t v = 0; v < now.vertexCount; ++v)
            {
                for (int k = 0; k < 3; ++k) { xyz[v * 3 + size_t(k)] = now.Coordinate(work, v, k); }
            }
            std::vector<uint32_t> starts(now.cells);
            for (size_t c = 0; c < now.cells; ++c) { starts[c] = now.CellStart(work, c); }
            std::vector<uint8_t> solidNow(now.recordCount, 0), onlyNow;
            if (only) { onlyNow.assign(now.recordCount, 0); }
            for (size_t r = 0; r < from.size() && r < solidNow.size(); ++r)
            {
                solidNow[r] = from[r] < options.solid.size() ? options.solid[from[r]] : 0;
                if (only && only->count(from[r])) { onlyNow[r] = 1; }
            }
            const auto hidden = LevelOcclusion::HiddenRecords(left, records, now.recordCount, xyz, starts, reach, solidNow,
                                                              only ? &onlyNow : nullptr, strictWalk);
            found.clear();
            for (size_t r = 1; r < hidden.size() && r < from.size(); ++r) { if (hidden[r]) { found.push_back(from[r]); } }
            return true;
        };

        // Lifts: what the crawl finds goes only where no sight line reaches it
        // (a low eye can see in under a door's bottom edge in places). The
        // sight lines alone decide here, not the walk's cones.
        std::set<uint32_t> lift;
        std::vector<uint32_t> rooms;
        for (uint32_t r : Lifts(bytes, lay, reach, options.solid, options.liftDoorTextures, options.roomDoors, rooms))
        {
            if (!remove.count(r)) { lift.insert(r); }
        }
        // Sealed rooms named by hand go whole: their doors were checked in play.
        for (uint32_t r : rooms) { if (remove.insert(r).second) { ++plan.byHand; } }
        plan.remove.assign(remove.begin(), remove.end());
        if (!lift.empty())
        {
            std::vector<uint32_t> found;
            if (!hiddenAfter(&lift, false, found)) { return false; }
            for (uint32_t r : found) { if (remove.insert(r).second) { ++plan.inLifts; } }
            plan.remove.assign(remove.begin(), remove.end());
            if (details) { for (uint32_t r : lift) { if (!remove.count(r)) { details->liftKept.push_back(r); } } }
        }

        // Hidden faces, judged among what is left: a face already taken out
        // hides nothing.
        std::vector<uint32_t> found;
        if (!hiddenAfter(nullptr, true, found)) { return false; }
        for (uint32_t r : found) { if (remove.insert(r).second) { ++plan.hidden; } }
        for (uint32_t r : options.byHand)
        {
            if (r == 0 || r >= lay.recordCount) { error = "data/LevelCleanupExtra.json names face record " + std::to_string(r) + ", which the file does not have"; return false; }
            if (remove.insert(r).second) { ++plan.byHand; }
        }
        if (details)
        {
            auto& kept = details->liftKept;
            kept.erase(std::remove_if(kept.begin(), kept.end(), [&](uint32_t r) { return remove.count(r) != 0; }), kept.end());
        }
        plan.remove.assign(remove.begin(), remove.end());
        return true;
    }
}
