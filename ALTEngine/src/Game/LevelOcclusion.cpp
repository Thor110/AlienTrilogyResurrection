#include "LevelOcclusion.h"

#include "CreatureSight.h"
#include "PlayerMover.h"
#include "../Formats/RenderMesh.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <map>

namespace ALTEngine::Game::LevelOcclusion
{
    namespace
    {
        struct V3 { double x, y, z; };
        V3 operator-(V3 a, V3 b) { return { a.x - b.x, a.y - b.y, a.z - b.z }; }
        double Dot(V3 a, V3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
        V3 Cross(V3 a, V3 b) { return { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x }; }
        V3 Lerp(V3 a, V3 b, double t) { return { a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t }; }

        struct V2 { double x, y; };
        using Poly = std::vector<V2>;
        double Turn(V2 o, V2 a, V2 b) { return (a.x - o.x) * (b.y - o.y) - (a.y - o.y) * (b.x - o.x); }
        Poly Hull(std::vector<V2> p)
        {
            std::sort(p.begin(), p.end(), [](V2 a, V2 b) { return a.x < b.x || (a.x == b.x && a.y < b.y); });
            if (p.size() < 3) { return p; }
            Poly h(2 * p.size());
            size_t k = 0;
            for (size_t i = 0; i < p.size(); ++i)
            {
                while (k >= 2 && Turn(h[k - 2], h[k - 1], p[i]) <= 0) { --k; }
                h[k++] = p[i];
            }
            for (size_t i = p.size() - 1, t = k + 1; i > 0; --i)
            {
                while (k >= t && Turn(h[k - 2], h[k - 1], p[i - 1]) <= 0) { --k; }
                h[k++] = p[i - 1];
            }
            h.resize(k - 1);
            return h;   // counter-clockwise
        }
        // The part of P left of a->b.
        Poly ClipLeft(const Poly& P, V2 a, V2 b)
        {
            Poly out;
            for (size_t i = 0; i < P.size(); ++i)
            {
                const V2 p = P[i], q = P[(i + 1) % P.size()];
                const double sp = Turn(a, b, p), sq = Turn(a, b, q);
                if (sp >= 0) { out.push_back(p); }
                if ((sp >= 0) != (sq >= 0)) { const double t = sp / (sp - sq); out.push_back({ p.x + (q.x - p.x) * t, p.y + (q.y - p.y) * t }); }
            }
            return out;
        }

        constexpr uint32_t NO_RUN = 0xffffffffu;
        constexpr size_t RECORD_SIZE = 20;
        constexpr uint8_t RUN_END = 0x80;
        constexpr double CELL = 512.0;
        constexpr double EDGE = 8.0;              // how near a cell's edge the eye is taken
        constexpr double WALL_CLEARANCE = 190.0;  // how near a wall cell's edge (PlayerMover::PROBE_LEAD, 200, less some)
        constexpr double IN_FRONT = 1.0;          // a face is seen from a point this far in front of it
        // The strict pass counts a sight line as blocked only this far inside
        // a blocker's open edges: lines between the points tried could pass
        // just over a wall's top or round a door's side.
        constexpr double OPEN_EDGE_MARGIN = 8.0;

        // The eye's height over the floor: 0x300 standing (PlayerCamera::
        // EYE_HEIGHT_ORIGINAL), down to 0x180 dying (FUN_0003ee4c), the bob
        // dipping it up to 64 more; 128 either way for a slope; 256 for the
        // Modern jump.
        constexpr int EYE_LOW = 0x180 - 64 - 128;
        constexpr int EYE_HIGH = 0x300 + 128 + 256;

        // Cones the strict pass checks: the port's, which always widens the
        // walk to its screen's half-width + 0x20 (GameplayScreen's
        // minHalfWidth): a 4:3 window at the original's 50.2 degrees (atan(160
        // / 256), 0x16c), wider ones, and all round (a steep look up or down).
        // The original's own narrower cones never draw these files.
        constexpr int YAWS = 64;
        constexpr std::array<int, 5> WIDTHS = { 0x16c + 0x20, 0x2a0, 0x340, 0x400, 0x800 };
        constexpr int CONFIG_WORDS = YAWS * int(WIDTHS.size()) / 64;
        // How far either side of the view a face can be on screen at each of
        // those: the cone less the 0x20. Half a yaw step more covers the
        // facings between.
        constexpr std::array<int, 5> SCREEN_HALF = { 0x16c, 0x2a0 - 0x20, 0x340 - 0x20, 0x400 - 0x20, 0x800 };
        // The wide cones come only from looking up or down (Modern Free Look,
        // up to 1.4 radians): the screen is that wide from a 32:9 window at
        // 0, 0, 32, 60 and 62 degrees of pitch (halfFov = atan(tanV * aspect
        // / (cos p - tanV sin p)), as GameplayScreen has it). The screen
        // reaches 25.1 degrees either side of the pitch, so a sight line
        // nearer level than the pitch less that (and 3 degrees more) is off
        // it there.
        constexpr std::array<double, 5> MIN_ELEVATION_DEGREES = { 0.0, 0.0, 32.0 - 28.1, 60.0 - 28.1, 62.0 - 28.1 };
        constexpr int YAW_SLACK = 4096 / YAWS / 2;

        struct Face
        {
            std::array<V3, 4> v{};
            int n = 0;
            int cell = -1;
            bool twoSided = false;
        };
        struct Tri
        {
            V3 a, b, c, n;
            double d = 0;
            int record = 0, cell = 0;
            bool back = false;        // a two-sided face's reversed copy
            // Each edge (a-b, b-c, c-a): 1 / length, and whether no other
            // blocker shares it (a wall's top, a door's side).
            std::array<double, 3> inverse{};
            std::array<bool, 3> open{};
        };
        struct Eye
        {
            double x0 = 0, x1 = 0, z0 = 0, z1 = 0, y0 = 0, y1 = 0;
            int diagonal = 0;         // 0x19-0x1c: only that half of the cell
            std::vector<V3> corners;  // the region's corners
        };

        bool InHalf(int attribute, double fx, double fz)
        {
            switch (attribute)
            {
            case 0x19: return fx + fz >= 1.0;
            case 0x1a: return fz >= fx;
            case 0x1b: return fx >= fz;
            case 0x1c: return fx + fz <= 1.0;
            default: return true;
            }
        }
    }

    std::vector<uint8_t> HiddenRecords(const Formats::LevelGeometry& level, const std::vector<uint8_t>& records,
                                       size_t recordCount, const std::vector<int16_t>& vertexXyz,
                                       const std::vector<uint32_t>& cellStarts, const std::vector<uint8_t>& reachable,
                                       const std::vector<uint8_t>& solid, const std::vector<uint8_t>* only, bool strictWalk)
    {
        std::vector<uint8_t> hidden(recordCount, 0);
        const int L = level.header.mapLength, W = level.header.mapWidth;
        const size_t cells = size_t(L) * size_t(W);
        if (cells == 0 || cellStarts.size() != cells || reachable.size() != cells || records.size() < recordCount * RECORD_SIZE)
        {
            return hidden;
        }
        const size_t vertexCount = vertexXyz.size() / 3;
        auto i32 = [&](size_t r, int k) {
            const size_t p = r * RECORD_SIZE + size_t(k) * 4;
            return static_cast<int32_t>(uint32_t(records[p]) | (uint32_t(records[p + 1]) << 8)
                                        | (uint32_t(records[p + 2]) << 16) | (uint32_t(records[p + 3]) << 24));
        };
        auto routine = [&](size_t r) { return records[r * RECORD_SIZE + 18]; };
        auto light = [&](size_t r) { return records[r * RECORD_SIZE + 19]; };

        // Who lists each record.
        std::vector<int> owners(recordCount, 0), ownerCell(recordCount, -1);
        for (size_t c = 0; c < cells; ++c)
        {
            const uint32_t start = cellStarts[c];
            if (start == NO_RUN || start == 0 || start >= recordCount) { continue; }
            for (size_t r = start; r < recordCount; ++r)
            {
                if (i32(r, 0) == -1) { break; }
                if (owners[r]++ == 0) { ownerCell[r] = int(c); }
                if (light(r) & RUN_END) { break; }
            }
        }

        std::vector<Face> faces(recordCount);
        for (size_t r = 1; r < recordCount; ++r)
        {
            if (owners[r] == 0) { continue; }
            Face& f = faces[r];
            f.cell = ownerCell[r];
            f.n = i32(r, 3) < 0 ? 3 : 4;
            for (int k = 0; k < f.n; ++k)
            {
                const int32_t v = i32(r, k);
                if (v < 0 || size_t(v) >= vertexCount) { f.n = 0; break; }
                f.v[size_t(k)] = { double(vertexXyz[size_t(v) * 3]), double(vertexXyz[size_t(v) * 3 + 1]), double(vertexXyz[size_t(v) * 3 + 2]) };
            }
        }
        // A cut-out face whose texture has no clear texel blocks as an opaque
        // one does, from both sides (cut-outs are not culled).
        auto solidCutout = [&](size_t r) { return r < solid.size() && solid[r] != 0; };
        auto opaque = [&](size_t r) {
            const auto dr = Formats::ClassifyDrawRoutine(routine(r));
            return (dr.fill == Formats::FaceFill::Opaque && !dr.flat && dr.culled) || solidCutout(r);
        };
        auto seeThrough = [&](size_t r) {
            const auto dr = Formats::ClassifyDrawRoutine(routine(r));
            return dr.fill != Formats::FaceFill::Hidden && !(dr.fill == Formats::FaceFill::Opaque && !dr.flat) && !solidCutout(r);
        };
        for (size_t r = 1; r < recordCount; ++r) { faces[r].twoSided = solidCutout(r); }

        auto cellOf = [&](double wx, double wz, int& cx, int& cz) {
            cx = int(std::floor(wx / CELL + L / 2.0));
            cz = int(std::floor(wz / CELL + W / 2.0));
        };

        // The blockers: opaque faces' triangles as the renderer draws them
        // (a, b, c and a, c, d), binned by the cells they reach over.
        std::vector<Tri> tris;
        std::vector<std::vector<int>> bins(cells);
        std::vector<std::vector<int>> seeThroughIn(cells);
        std::vector<std::pair<double, double>> faceY(recordCount, { 0.0, 0.0 });
        for (size_t r = 1; r < recordCount; ++r)
        {
            const Face& f = faces[r];
            if (f.n == 0 || f.cell < 0) { continue; }
            double y0 = 1e18, y1 = -1e18;
            for (int k = 0; k < f.n; ++k) { y0 = std::min(y0, f.v[size_t(k)].y); y1 = std::max(y1, f.v[size_t(k)].y); }
            faceY[r] = { y0, y1 };
            if (seeThrough(r)) { seeThroughIn[size_t(f.cell)].push_back(int(r)); }
            if (!opaque(r)) { continue; }
            for (int t = 0; t < (f.n - 2) * (f.twoSided ? 2 : 1); ++t)
            {
                // Two-sided: the same triangles again, wound the other way.
                const bool back = t >= f.n - 2;
                const int u = back ? t - (f.n - 2) : t;
                Tri tri{ f.v[0], f.v[size_t(u + 1)], f.v[size_t(u + 2)], {}, 0.0, int(r), f.cell };
                if (back) { std::swap(tri.b, tri.c); tri.back = true; }
                V3 n = Cross(tri.b - tri.a, tri.c - tri.a);
                const double len = std::sqrt(Dot(n, n));
                if (len < 1e-6) { continue; }
                tri.n = { n.x / len, n.y / len, n.z / len };
                tri.d = Dot(tri.n, tri.a);
                const int index = int(tris.size());
                const V3 edges[3] = { tri.b - tri.a, tri.c - tri.b, tri.a - tri.c };
                for (int k = 0; k < 3; ++k) { tri.inverse[size_t(k)] = 1.0 / std::max(1e-9, std::sqrt(Dot(edges[k], edges[k]))); }
                tris.push_back(tri);
                const double x0 = std::min({ tri.a.x, tri.b.x, tri.c.x }), x1 = std::max({ tri.a.x, tri.b.x, tri.c.x });
                const double z0 = std::min({ tri.a.z, tri.b.z, tri.c.z }), z1 = std::max({ tri.a.z, tri.b.z, tri.c.z });
                int a0, b0, a1, b1;
                cellOf(x0 - 1, z0 - 1, a0, b0);
                cellOf(x1 + 1, z1 + 1, a1, b1);
                for (int zz = std::max(0, b0); zz <= std::min(W - 1, b1); ++zz)
                {
                    for (int xx = std::max(0, a0); xx <= std::min(L - 1, a1); ++xx) { bins[size_t(zz) * size_t(L) + size_t(xx)].push_back(index); }
                }
            }
        }

        {
            // An edge is closed when another blocker's triangle has an edge it
            // lies along (both its ends within EDGE_SLACK of that edge): the
            // levels' corners are often a unit or two apart (-1280 against
            // -1282), and a short edge can run along part of a longer one.
            constexpr double EDGE_SLACK = 3.0, BUCKET = 64.0;
            struct Edge { V3 p, q; int tri; };
            std::vector<Edge> edges;
            edges.reserve(tris.size() * 3);
            for (size_t i = 0; i < tris.size(); ++i)
            {
                const Tri& t = tris[i];
                edges.push_back({ t.a, t.b, int(i) });
                edges.push_back({ t.b, t.c, int(i) });
                edges.push_back({ t.c, t.a, int(i) });
            }
            using Key = std::array<int, 3>;
            auto bucket = [&](V3 p) { return Key{ int(std::floor(p.x / BUCKET)), int(std::floor(p.y / BUCKET)), int(std::floor(p.z / BUCKET)) }; };
            // Each edge under every bucket its length passes through.
            std::map<Key, std::vector<int>> byBucket;
            for (size_t i = 0; i < edges.size(); ++i)
            {
                const V3 d = edges[i].q - edges[i].p;
                const int steps = 1 + int(std::sqrt(Dot(d, d)) / (BUCKET / 2));
                std::vector<Key> seen;
                for (int k = 0; k <= steps; ++k)
                {
                    const Key key = bucket(Lerp(edges[i].p, edges[i].q, double(k) / steps));
                    if (std::find(seen.begin(), seen.end(), key) == seen.end()) { seen.push_back(key); byBucket[key].push_back(int(i)); }
                }
            }
            auto near = [&](V3 x, const Edge& e) {
                const V3 d = e.q - e.p;
                const double len2 = Dot(d, d);
                const double u = len2 > 0 ? std::clamp(Dot(x - e.p, d) / len2, 0.0, 1.0) : 0.0;
                const V3 c = Lerp(e.p, e.q, u), off = x - c;
                return Dot(off, off) <= EDGE_SLACK * EDGE_SLACK;
            };
            for (size_t i = 0; i < edges.size(); ++i)
            {
                const Edge& e = edges[i];
                bool closed = false;
                for (int dx = -1; dx <= 1 && !closed; ++dx)
                {
                    for (int dy = -1; dy <= 1 && !closed; ++dy)
                    {
                        for (int dz = -1; dz <= 1 && !closed; ++dz)
                        {
                            Key key = bucket(e.p);
                            key[0] += dx; key[1] += dy; key[2] += dz;
                            const auto it = byBucket.find(key);
                            if (it == byBucket.end()) { continue; }
                            for (int j : it->second)
                            {
                                const Edge& f = edges[size_t(j)];
                                if (f.tri == e.tri) { continue; }
                                // A two-sided face's other side closes nothing.
                                const Tri& mine = tris[size_t(e.tri)];
                                const Tri& theirs = tris[size_t(f.tri)];
                                if (mine.record == theirs.record && mine.back != theirs.back) { continue; }
                                if (near(e.p, f) && near(e.q, f)) { closed = true; break; }
                            }
                        }
                    }
                }
                tris[size_t(e.tri)].open[i % 3] = !closed;
            }
        }

        // Every reachable cell's walk (all round, a superset of every facing
        // and screen) and, for every cell, the reachable cells that draw it.
        const size_t words = (cells + 63) / 64;
        std::vector<int> seerIndex(cells, -1);
        std::vector<std::vector<uint64_t>> walks;
        std::vector<std::vector<int>> seers(cells);
        for (size_t s = 0; s < cells; ++s)
        {
            if (!reachable[s]) { continue; }
            const auto walk = CreatureSight::DrawnCells(level, int(s % size_t(L)), int(s / size_t(L)), 0, 0x800);
            std::vector<uint64_t> bits(words, 0);
            for (size_t k = 0; k < cells && k < walk.size(); ++k)
            {
                if (!walk[k]) { continue; }
                bits[k / 64] |= uint64_t(1) << (k % 64);
                seers[k].push_back(int(s));
            }
            seerIndex[s] = int(walks.size());
            walks.push_back(std::move(bits));
        }
        auto drawnFrom = [&](size_t s, size_t cell) { return (walks[size_t(seerIndex[s])][cell / 64] >> (cell % 64)) & 1u; };

        std::vector<int> liftTravel(cells, 0);
        for (const auto& lift : level.lifts)
        {
            for (int dz = 0; dz < std::max<int>(1, lift.depth); ++dz)
            {
                for (int dx = 0; dx < std::max<int>(1, lift.width); ++dx)
                {
                    const int i = Formats::CellIndex(level, lift.x + dx, lift.y + dz);
                    if (i >= 0) { liftTravel[size_t(i)] = std::max<int>(liftTravel[size_t(i)], lift.travel); }
                }
            }
        }
        auto eyeOf = [&](size_t s) {
            Eye e;
            const int sx = int(s % size_t(L)), sz = int(s / size_t(L));
            const double cx0 = (sx - L / 2.0) * CELL, cz0 = (sz - W / 2.0) * CELL;
            e.x0 = cx0 + EDGE; e.x1 = cx0 + CELL - EDGE; e.z0 = cz0 + EDGE; e.z1 = cz0 + CELL - EDGE;
            // The mover keeps the player's centre PROBE_LEAD (200) clear of a
            // wall cell (it probes that far ahead and to each side), so the eye
            // stays that far from a side of the cell that borders one.
            auto wall = [&](int dx, int dz) {
                const int j = Formats::CellIndex(level, sx + dx, sz + dz);
                return j >= 0 && PlayerMover::WallAttribute(level.collisionGrid[size_t(j)].attribute);
            };
            if (wall(-1, 0)) { e.x0 = std::max(e.x0, cx0 + WALL_CLEARANCE); }
            if (wall(1, 0)) { e.x1 = std::min(e.x1, cx0 + CELL - WALL_CLEARANCE); }
            if (wall(0, -1)) { e.z0 = std::max(e.z0, cz0 + WALL_CLEARANCE); }
            if (wall(0, 1)) { e.z1 = std::min(e.z1, cz0 + CELL - WALL_CLEARANCE); }
            const auto& g = level.collisionGrid[s];
            int lo = g.floorHeight, hi = g.floorHeight;
            static constexpr int STEP[4][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };
            for (const auto& d : STEP)
            {
                const int j = Formats::CellIndex(level, sx + d[0], sz + d[1]);
                if (j < 0 || !reachable[size_t(j)]) { continue; }
                lo = std::min<int>(lo, level.collisionGrid[size_t(j)].floorHeight);
                hi = std::max<int>(hi, level.collisionGrid[size_t(j)].floorHeight);
            }
            e.y0 = lo * 32.0 + EYE_LOW - liftTravel[s] * 32.0;
            e.y1 = hi * 32.0 + EYE_HIGH + liftTravel[s] * 32.0;
            if (g.attribute >= 0x19 && g.attribute <= 0x1c)
            {
                e.diagonal = g.attribute;
                const double m = EDGE, M = CELL - EDGE;
                std::array<std::pair<double, double>, 3> half;
                switch (g.attribute)
                {
                case 0x19: half = { { { M, M }, { 2 * m, M }, { M, 2 * m } } }; break;
                case 0x1a: half = { { { m, M }, { m, 2 * m }, { M - m, M } } }; break;
                case 0x1b: half = { { { M, m }, { 2 * m, m }, { M, M - m } } }; break;
                default: half = { { { m, m }, { M - m, m }, { m, M - m } } }; break;
                }
                for (const auto& p : half)
                {
                    e.corners.push_back({ cx0 + p.first, e.y0, cz0 + p.second });
                    e.corners.push_back({ cx0 + p.first, e.y1, cz0 + p.second });
                }
            }
            else
            {
                for (int i = 0; i < 8; ++i) { e.corners.push_back({ (i & 1) ? e.x1 : e.x0, (i & 2) ? e.y1 : e.y0, (i & 4) ? e.z1 : e.z0 }); }
            }
            return e;
        };

        // Does any part of the face show its front to the eye's region? A
        // quad is tried as all four of its triangles (the renderer's two, the
        // other split's two).
        auto facesEye = [&](const Face& f, const Eye& e) {
            if (f.twoSided) { return true; }
            std::array<std::array<V3, 3>, 4> subs{};
            int count = 0;
            if (f.n == 3) { subs[0] = { f.v[0], f.v[1], f.v[2] }; count = 1; }
            else
            {
                subs = { { { f.v[0], f.v[1], f.v[2] }, { f.v[0], f.v[2], f.v[3] }, { f.v[0], f.v[1], f.v[3] }, { f.v[1], f.v[2], f.v[3] } } };
                count = 4;
            }
            for (int i = 0; i < count; ++i)
            {
                const auto& t = subs[size_t(i)];
                V3 n = Cross(t[1] - t[0], t[2] - t[0]);
                const double len = std::sqrt(Dot(n, n));
                if (len < 1e-6) { continue; }
                n = { n.x / len, n.y / len, n.z / len };
                const double d = Dot(n, t[0]);
                for (const V3& p : e.corners) { if (Dot(n, p) > d + IN_FRONT) { return true; } }
            }
            return false;
        };

        // A see-through face the walk from s draws anywhere over the shaft
        // between the eye's region and face r.
        auto seeThroughBetween = [&](const Eye& e, size_t r, size_t s) {
            const Face& f = faces[r];
            std::vector<V2> pts;
            for (const V3& p : e.corners) { pts.push_back({ p.x, p.z }); }
            for (int k = 0; k < f.n; ++k) { pts.push_back({ f.v[size_t(k)].x, f.v[size_t(k)].z }); }
            const Poly H = Hull(pts);
            if (H.size() < 3) { return true; }
            const double y0 = std::min(e.y0, faceY[r].first), y1 = std::max(e.y1, faceY[r].second);
            double x0 = 1e18, x1 = -1e18, z0 = 1e18, z1 = -1e18;
            for (const V2& p : H) { x0 = std::min(x0, p.x); x1 = std::max(x1, p.x); z0 = std::min(z0, p.y); z1 = std::max(z1, p.y); }
            int a0, b0, a1, b1;
            cellOf(x0, z0, a0, b0);
            cellOf(x1, z1, a1, b1);
            for (int zz = std::max(0, b0); zz <= std::min(W - 1, b1); ++zz)
            {
                for (int xx = std::max(0, a0); xx <= std::min(L - 1, a1); ++xx)
                {
                    const size_t ci = size_t(zz) * size_t(L) + size_t(xx);
                    if (seeThroughIn[ci].empty() || !drawnFrom(s, ci)) { continue; }
                    const double cx0 = (xx - L / 2.0) * CELL, cz0 = (zz - W / 2.0) * CELL;
                    Poly square{ { cx0, cz0 }, { cx0 + CELL, cz0 }, { cx0 + CELL, cz0 + CELL }, { cx0, cz0 + CELL } };
                    for (size_t i = 0; i < H.size() && square.size() >= 3; ++i) { square = ClipLeft(square, H[i], H[(i + 1) % H.size()]); }
                    if (square.size() < 3) { continue; }
                    for (int q : seeThroughIn[ci])
                    {
                        if (size_t(q) != r && faceY[size_t(q)].second >= y0 && faceY[size_t(q)].first <= y1) { return true; }
                    }
                }
            }
            return false;
        };

        // Is the line o -> t blocked by an opaque face facing o that
        // `counts(tri)` allows? Walks the cells o -> t crosses.
        std::vector<int> mark(tris.size(), 0);
        int stamp = 0;
        auto blocked = [&](V3 o, V3 t, size_t self, double margin, auto&& counts) {
            const double gx = o.x / CELL + L / 2.0, gz = o.z / CELL + W / 2.0, ex = t.x / CELL + L / 2.0, ez = t.z / CELL + W / 2.0;
            int cx = int(std::floor(gx)), cz = int(std::floor(gz));
            const int tx = int(std::floor(ex)), tz = int(std::floor(ez));
            const int sx = ex > gx ? 1 : -1, sz = ez > gz ? 1 : -1;
            const double dx = ex - gx, dz = ez - gz;
            double tmx = dx != 0 ? ((sx > 0 ? (cx + 1 - gx) : (gx - cx)) / std::fabs(dx)) : 1e30;
            double tmz = dz != 0 ? ((sz > 0 ? (cz + 1 - gz) : (gz - cz)) / std::fabs(dz)) : 1e30;
            const double tdx = dx != 0 ? 1 / std::fabs(dx) : 1e30, tdz = dz != 0 ? 1 / std::fabs(dz) : 1e30;
            ++stamp;
            for (int guard = 0; guard < 4 * (L + W) + 8; ++guard)
            {
                if (cx >= 0 && cz >= 0 && cx < L && cz < W)
                {
                    for (int k : bins[size_t(cz) * size_t(L) + size_t(cx)])
                    {
                        if (mark[size_t(k)] == stamp) { continue; }
                        mark[size_t(k)] = stamp;
                        const Tri& q = tris[size_t(k)];
                        const double de = Dot(q.n, o) - q.d;
                        if (de <= 0) { continue; }                  // seen from behind: not drawn
                        const double dt = Dot(q.n, t) - q.d;
                        if (dt >= 0) { continue; }
                        const double s = de / (de - dt);
                        if (s >= 1 - 1e-6 || s <= 1e-6) { continue; }
                        const V3 p = Lerp(o, t, s);
                        const auto inside = [&](V3 from, V3 to, int edge) {
                            const double need = q.open[size_t(edge)] ? margin : -1e-6;
                            return Dot(Cross(to - from, p - from), q.n) * q.inverse[size_t(edge)] >= need;
                        };
                        if (inside(q.a, q.b, 0) && inside(q.b, q.c, 1) && inside(q.c, q.a, 2) && size_t(q.record) != self && counts(q))
                        {
                            return true;
                        }
                    }
                }
                if (cx == tx && cz == tz) { break; }
                if (tmx < tmz) { tmx += tdx; cx += sx; } else { tmz += tdz; cz += sz; }
                if (std::min(tmx - tdx, tmz - tdz) > 1.0 + 1e-9) { break; }
            }
            return false;
        };

        // Points over a face, and over an eye's region.
        auto facePoints = [&](const Face& f, int grid) {
            std::vector<V3> pts;
            if (f.n == 4)
            {
                for (int i = 0; i < grid; ++i)
                {
                    const double a = 0.04 + 0.92 * i / (grid - 1);
                    const V3 p0 = Lerp(f.v[0], f.v[1], a), p1 = Lerp(f.v[3], f.v[2], a);
                    for (int j = 0; j < grid; ++j) { pts.push_back(Lerp(p0, p1, 0.04 + 0.92 * j / (grid - 1))); }
                }
            }
            else
            {
                for (int i = 0; i < grid; ++i)
                {
                    for (int j = 0; i + j < grid; ++j)
                    {
                        const double a = (i + 0.3) / (grid + 0.6), b = (j + 0.3) / (grid + 0.6), c = 1 - a - b;
                        pts.push_back({ f.v[0].x * c + f.v[1].x * a + f.v[2].x * b, f.v[0].y * c + f.v[1].y * a + f.v[2].y * b,
                                        f.v[0].z * c + f.v[1].z * a + f.v[2].z * b });
                    }
                }
            }
            return pts;
        };
        auto eyePoints = [&](const Eye& e, int across, int heights) {
            std::vector<V3> pts;
            for (int ix = 0; ix < across; ++ix)
            {
                for (int iz = 0; iz < across; ++iz)
                {
                    const double fx = (ix + 0.5) / across, fz = (iz + 0.5) / across;
                    const double x = e.x0 + (e.x1 - e.x0) * fx, z = e.z0 + (e.z1 - e.z0) * fz;
                    if (e.diagonal && !InHalf(e.diagonal, (x - e.x0 + EDGE) / CELL, (z - e.z0 + EDGE) / CELL)) { continue; }
                    for (int iy = 0; iy < heights; ++iy) { pts.push_back({ x, e.y0 + (e.y1 - e.y0) * iy / (heights - 1), z }); }
                }
            }
            for (const V3& c : e.corners) { pts.push_back(c); }
            return pts;
        };
        auto anyClear = [&](const std::vector<V3>& eyes, const std::vector<V3>& targets, size_t self, auto&& counts) {
            for (const V3& o : eyes)
            {
                for (const V3& t : targets) { if (!blocked(o, t, self, 0.0, counts)) { return true; } }
            }
            return false;
        };

        // Pass 1, coarse: from the nearest reachable cells out, any clear line
        // keeps the face. What is left is checked again in pass 2.
        std::vector<size_t> candidates;
        for (size_t r = 1; r < recordCount; ++r)
        {
            const Face& f = faces[r];
            if (f.n == 0 || owners[r] != 1 || f.cell < 0 || reachable[size_t(f.cell)]) { continue; }
            if (only ? !(r < only->size() && (*only)[r]) : !opaque(r)) { continue; }
            auto list = seers[size_t(f.cell)];
            if (list.empty()) { continue; }
            double cx = 0, cz = 0;
            for (int k = 0; k < f.n; ++k) { cx += f.v[size_t(k)].x / f.n; cz += f.v[size_t(k)].z / f.n; }
            auto distance = [&](int s) {
                const double sx = (s % L - L / 2.0 + 0.5) * CELL - cx, sz = (s / L - W / 2.0 + 0.5) * CELL - cz;
                return sx * sx + sz * sz;
            };
            std::sort(list.begin(), list.end(), [&](int a, int b) { return distance(a) < distance(b); });
            const auto targets = facePoints(f, 3);
            bool seen = false;
            for (int s : list)
            {
                const Eye e = eyeOf(size_t(s));
                if (!facesEye(f, e)) { continue; }
                if (seeThroughBetween(e, r, size_t(s))) { seen = true; break; }
                const auto counts = [&](const Tri& q) { return drawnFrom(size_t(s), size_t(q.cell)) != 0; };
                if (anyClear(eyePoints(e, 3, 2), targets, r, counts)) { seen = true; break; }
            }
            if (!seen) { candidates.push_back(r); }
        }
        if (candidates.empty()) { return hidden; }

        // Pass 2, strict and denser, from every reachable cell that draws a
        // candidate's cell. The walk is rerun at every facing and cone, and a
        // blocker counts for a sight line only if its cell is drawn at every
        // one of those that draws the candidate's cell with the line on
        // screen.
        std::vector<uint8_t> stillHidden(recordCount, 0);
        std::vector<std::vector<size_t>> bySeer(cells);
        for (size_t r : candidates)
        {
            stillHidden[r] = 1;
            for (int s : seers[size_t(faces[r].cell)]) { bySeer[size_t(s)].push_back(r); }
        }
        using Configs = std::array<uint64_t, CONFIG_WORDS>;
        constexpr int BEARINGS = 256;   // 16 angle units each
        std::vector<Configs> onScreen(BEARINGS);
        for (int b = 0; b < BEARINGS; ++b)
        {
            Configs m{};
            int bit = 0;
            for (size_t w = 0; w < WIDTHS.size(); ++w)
            {
                for (int y = 0; y < YAWS; ++y, ++bit)
                {
                    const int half = SCREEN_HALF[w] + YAW_SLACK;
                    // How far the bucket's nearer edge is from the yaw.
                    const int yaw = y * (4096 / YAWS), lo = b * (4096 / BEARINGS), hi = lo + 4096 / BEARINGS;
                    const int off = (yaw >= lo && yaw <= hi) ? 0
                        : std::min({ (lo - yaw + 8192) % 4096, (yaw - hi + 8192) % 4096 });
                    if (half >= 2048 || off <= half) { m[size_t(bit / 64)] |= uint64_t(1) << (bit % 64); }
                }
            }
            onScreen[size_t(b)] = m;
        }
        std::vector<Configs> configs(cells);
        for (size_t s = 0; s < cells; ++s)
        {
            bool any = false;
            for (size_t r : bySeer[s]) { any = any || stillHidden[r]; }
            if (!any) { continue; }
            for (auto& c : configs) { c.fill(0); }
            if (strictWalk)   // every facing and cone's walk
            {
                int bit = 0;
                for (int width : WIDTHS)
                {
                    for (int y = 0; y < YAWS; ++y, ++bit)
                    {
                        for (int k : CreatureSight::DrawnCellList(level, int(s % size_t(L)), int(s / size_t(L)), y * (4096 / YAWS), width))
                        {
                            if (k >= 0 && size_t(k) < cells) { configs[size_t(k)][size_t(bit / 64)] |= uint64_t(1) << (bit % 64); }
                        }
                    }
                }
            }
            const Eye e = eyeOf(s);
            // The lifts (sight lines alone) are few: tried as densely as the check does.
            const auto eyes = strictWalk ? eyePoints(e, 4, 3) : eyePoints(e, 6, 4);
            for (size_t r : bySeer[s])
            {
                if (!stillHidden[r]) { continue; }
                const Face& f = faces[r];
                if (!facesEye(f, e)) { continue; }
                const Configs& drawn = configs[size_t(f.cell)];
                bool seen = false;
                for (const V3& o : eyes)
                {
                    for (const V3& t : facePoints(f, strictWalk ? 4 : 6))
                    {
                        double a = std::atan2(-(t.x - o.x), t.z - o.z) * 4096.0 / 6.283185307179586;
                        if (a < 0) { a += 4096.0; }
                        const Configs& screen = onScreen[size_t(std::min(BEARINGS - 1, int(a) / (4096 / BEARINGS)))];
                        const double run = std::sqrt((t.x - o.x) * (t.x - o.x) + (t.z - o.z) * (t.z - o.z));
                        const double elevation = std::atan2(std::fabs(t.y - o.y), run) * 57.29577951308232;
                        Configs steep{};
                        for (size_t wi = 0; wi < WIDTHS.size(); ++wi)
                        {
                            if (elevation < MIN_ELEVATION_DEGREES[wi]) { continue; }
                            for (int y = 0; y < YAWS; ++y) { const int at = int(wi) * YAWS + y; steep[size_t(at / 64)] |= uint64_t(1) << (at % 64); }
                        }
                        Configs need{};
                        bool any2 = false;
                        for (int w = 0; w < CONFIG_WORDS; ++w) { need[size_t(w)] = drawn[size_t(w)] & screen[size_t(w)] & steep[size_t(w)]; }
                        for (int w = 0; w < CONFIG_WORDS; ++w) { any2 = any2 || need[size_t(w)]; }
                        if (strictWalk && !any2) { continue; }   // never on screen while the face's cell is drawn
                        const auto counts = [&](const Tri& q) {
                            if (!strictWalk) { return drawnFrom(s, size_t(q.cell)) != 0; }
                            const Configs& have = configs[size_t(q.cell)];
                            for (int w = 0; w < CONFIG_WORDS; ++w) { if ((have[size_t(w)] & need[size_t(w)]) != need[size_t(w)]) { return false; } }
                            return true;
                        };
                        if (!blocked(o, t, r, OPEN_EDGE_MARGIN, counts)) { seen = true; break; }
                    }
                    if (seen) { break; }
                }
                if (seen) { stillHidden[r] = 0; }
            }
        }
        for (size_t r : candidates) { hidden[r] = stillHidden[r]; }
        return hidden;
    }
}
