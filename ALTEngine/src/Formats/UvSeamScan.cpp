#include "UvSeamScan.h"

#include "RenderMesh.h"
#include "TextureAnimation.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <utility>

namespace ALTEngine::Formats
{
    namespace
    {
        using Uv = std::pair<float, float>;
        using Uvs = std::array<Uv, 4>;


        struct Vec { float x, y, z; };

        Vec Sub(const ModelVertex& a, const ModelVertex& b)
        {
            return { static_cast<float>(a.x - b.x), static_cast<float>(a.y - b.y), static_cast<float>(a.z - b.z) };
        }

        bool Normal(const LevelGeometry& level, const ModelQuad& q, Vec& out)
        {
            auto in = [&](int32_t i) { return i >= 0 && static_cast<size_t>(i) < level.vertices.size(); };
            if (!in(q.a) || !in(q.b) || !in(q.c)) { return false; }
            const auto& A = level.vertices[static_cast<size_t>(q.a)];
            const auto& B = level.vertices[static_cast<size_t>(q.b)];
            const auto& C = level.vertices[static_cast<size_t>(q.c)];
            Vec u, v;
            if (q.d >= 0 && in(q.d))
            {
                u = Sub(C, A);
                v = Sub(level.vertices[static_cast<size_t>(q.d)], B);
            }
            else
            {
                u = Sub(B, A);
                v = Sub(C, A);
            }
            out = { u.y * v.z - u.z * v.y, u.z * v.x - u.x * v.z, u.x * v.y - u.y * v.x };
            const float len = std::sqrt(out.x * out.x + out.y * out.y + out.z * out.z);
            if (len < 1e-3f) { return false; }
            out = { out.x / len, out.y / len, out.z / len };
            return true;
        }

        bool SameVertexSet(std::array<int32_t, 4> a, const ModelQuad& q)
        {
            std::array<int32_t, 4> b = { q.a, q.b, q.c, q.d };
            std::sort(a.begin(), a.end());
            std::sort(b.begin(), b.end());
            return a == b;
        }

        struct FaceInfo
        {
            bool usable = false;
            bool triangle = false;
            int corners = 4;
            int page = 0;
            Uvs raw{};
            bool flip = false;     // its current orientation, from a patch
            int steps = 0;
            bool patched = false;
            Vec normal{};
            uint16_t texIndex = 0;
        };

        struct Page
        {
            const std::vector<uint8_t>* rgba = nullptr;
            int width = 256, height = 256;
        };

        // Two descriptors that overlap or border each other on the page.
        bool RectsTouch(const BxRectangle& a, const BxRectangle& b)
        {
            constexpr int SLACK = 2;
            const bool xs = a.x <= b.x + b.width + SLACK && b.x <= a.x + a.width + SLACK;
            const bool ys = a.y <= b.y + b.height + SLACK && b.y <= a.y + a.height + SLACK;
            return xs && ys;
        }
    }

    std::array<std::pair<float, float>, 4> OrientUvs(std::array<std::pair<float, float>, 4> uvs,
                                                    bool triangle, bool flip, int steps)
    {
        const int corners = triangle ? 3 : 4;
        if (flip)
        {
            std::swap(uvs[0], uvs[1]);
            if (!triangle) { std::swap(uvs[2], uvs[3]); }
        }
        steps = ((steps % corners) + corners) % corners;
        if (steps != 0)
        {
            auto rotated = uvs;
            for (int i = 0; i < corners; ++i)
            {
                rotated[static_cast<size_t>(i)] = uvs[static_cast<size_t>((i - steps + corners) % corners)];
            }
            uvs = rotated;
        }
        return uvs;
    }

    std::vector<UvSeamSuspect> ScanLevelUvSeams(const LevelGeometry& level, const BndTextureSet& textures,
                                                const UvSeamOptions& options)
    {
        std::vector<UvSeamSuspect> suspects;

        // Pages by their TP number, which is what a descriptor's page names.
        std::map<int, Page> pages;
        for (const auto& t : textures.textures)
        {
            int index = 0;
            try { index = std::stoi(t.index); } catch (...) { continue; }
            pages[index] = { &t.rgba, t.width, t.height };
        }

        TextureAnimator animator;
        animator.Reset(level);

        const size_t count = level.quads.size();
        std::vector<FaceInfo> info(count);
        for (size_t f = 0; f < count; ++f)
        {
            const ModelQuad& q = level.quads[f];
            const DrawRoutine routine = ClassifyDrawRoutine(q.flags);
            if (routine.fill == FaceFill::Hidden || routine.flat) { continue; }
            uint16_t tex = q.texIndex;
            if (routine.animated && !animator.CurrentTexture(static_cast<int>(q.texIndex), tex)) { continue; }
            if (tex >= textures.uvRects.size()) { continue; }
            FaceInfo& fi = info[f];
            fi.page = textures.uvRects[tex].page;
            if (!pages.count(fi.page)) { continue; }
            if (!Normal(level, q, fi.normal)) { continue; }
            fi.triangle = q.d < 0;
            fi.corners = fi.triangle ? 3 : 4;
            fi.texIndex = tex;
            fi.raw = LevelFaceUvs(q, tex, textures.uvRects);
            for (const auto& rot : options.applied)
            {
                if (SameVertexSet(rot.vertices, q))
                {
                    fi.flip = rot.flip;
                    fi.steps = rot.steps;
                    fi.patched = true;
                    break;
                }
            }
            fi.usable = true;
        }

        // Shared edges, by their two vertices.
        struct Side { int face; int i; int j; };
        std::map<std::pair<int32_t, int32_t>, std::vector<Side>> edges;
        for (size_t f = 0; f < count; ++f)
        {
            if (!info[f].usable) { continue; }
            const ModelQuad& q = level.quads[f];
            const int32_t v[4] = { q.a, q.b, q.c, q.d };
            const int n = info[f].corners;
            for (int i = 0; i < n; ++i)
            {
                const int j = (i + 1) % n;
                const int32_t a = v[i], b = v[j];
                const auto key = a < b ? std::make_pair(a, b) : std::make_pair(b, a);
                edges[key].push_back({ static_cast<int>(f), a < b ? i : j, a < b ? j : i });
            }
        }

        // Each face's neighbours across an edge, with its own corner pair
        // and theirs, both running from the lower vertex to the higher.
        struct Link { int i, j, other, oi, oj; };
        std::vector<std::vector<Link>> links(count);
        for (const auto& [key, sides] : edges)
        {
            if (sides.size() != 2) { continue; }
            const Side& s = sides[0];
            const Side& t = sides[1];
            const Vec& n1 = info[static_cast<size_t>(s.face)].normal;
            const Vec& n2 = info[static_cast<size_t>(t.face)].normal;
            if (n1.x * n2.x + n1.y * n2.y + n1.z * n2.z < options.minCosine) { continue; }
            links[static_cast<size_t>(s.face)].push_back({ s.i, s.j, t.face, t.i, t.j });
            links[static_cast<size_t>(t.face)].push_back({ t.i, t.j, s.face, s.i, s.j });
        }

        // Each face's orientation as the search has it, starting from the
        // level's own (or its patch's).
        struct Orientation { bool flip; int steps; };
        std::vector<Orientation> state(count);
        for (size_t f = 0; f < count; ++f)
        {
            state[f] = { info[f].flip, info[f].corners > 0 ? ((info[f].steps % info[f].corners) + info[f].corners) % info[f].corners : 0 };
        }
        const std::vector<Orientation> initial = state;
        auto oriented = [&](int f, Orientation o) {
            const FaceInfo& fi = info[static_cast<size_t>(f)];
            return OrientUvs(fi.raw, fi.triangle, o.flip, o.steps);
        };
        auto current = [&](int f) { return oriented(f, state[static_cast<size_t>(f)]); };

        // How far apart, in texels on the page, the two faces put the edge's
        // two vertices - or -1 when the faces are not two pieces of one
        // picture (other pages, the same tile, or rectangles that don't
        // touch), where nothing says the texture runs on.
        auto comparable = [&](int f, const Link& l) {
            const FaceInfo& fi = info[static_cast<size_t>(f)];
            const FaceInfo& gi = info[static_cast<size_t>(l.other)];
            return fi.page == gi.page && fi.texIndex != gi.texIndex
                && RectsTouch(textures.uvRects[fi.texIndex], textures.uvRects[gi.texIndex]);
        };
        auto edgeCost = [&](int f, const Uvs& mine, const Link& l, const Uvs& theirs) {
            const Page& page = pages[info[static_cast<size_t>(f)].page];
            auto gap = [&](const Uv& a, const Uv& b) {
                const float du = (a.first - b.first) * static_cast<float>(page.width);
                const float dv = (a.second - b.second) * static_cast<float>(page.height);
                return std::sqrt(du * du + dv * dv);
            };
            return 0.5f * (gap(mine[static_cast<size_t>(l.i)], theirs[static_cast<size_t>(l.oi)])
                         + gap(mine[static_cast<size_t>(l.j)], theirs[static_cast<size_t>(l.oj)]));
        };
        // Keep only the edges worth comparing.
        for (size_t f = 0; f < count; ++f)
        {
            auto& ls = links[f];
            ls.erase(std::remove_if(ls.begin(), ls.end(), [&](const Link& l) { return !comparable(static_cast<int>(f), l); }), ls.end());
        }
        auto faceCost = [&](int f, const Uvs& mine) {
            float total = 0;
            for (const Link& l : links[static_cast<size_t>(f)]) { total += edgeCost(f, mine, l, current(l.other)); }
            return links[static_cast<size_t>(f)].empty() ? 0.0f : total / static_cast<float>(links[static_cast<size_t>(f)].size());
        };
        // The absolute orientation that turns a face the way `delta` turns
        // what it shows now.
        auto compose = [&](int f, Orientation delta) {
            const FaceInfo& fi = info[static_cast<size_t>(f)];
            const Uvs target = OrientUvs(current(f), fi.triangle, delta.flip, delta.steps);
            for (int flip = 0; flip < 2; ++flip)
            {
                for (int steps = 0; steps < fi.corners; ++steps)
                {
                    const Uvs u = oriented(f, { flip != 0, steps });
                    bool same = true;
                    for (int k = 0; k < fi.corners; ++k) { same = same && u[static_cast<size_t>(k)] == target[static_cast<size_t>(k)]; }
                    if (same) { return Orientation{ flip != 0, steps }; }
                }
            }
            return state[static_cast<size_t>(f)];
        };

        // 1. Whole pictures. A picture's pieces are often all wrong the same
        //    way, and then no one piece can be put right against the others;
        //    so each connected group is also tried turned as one.
        std::vector<int> group(count, -1);
        int groups = 0;
        for (size_t f = 0; f < count; ++f)
        {
            if (links[f].empty() || group[f] >= 0) { continue; }
            std::vector<int> members, stack{ static_cast<int>(f) };
            group[f] = groups;
            while (!stack.empty())
            {
                const int g = stack.back();
                stack.pop_back();
                members.push_back(g);
                for (const Link& l : links[static_cast<size_t>(g)])
                {
                    if (group[static_cast<size_t>(l.other)] < 0) { group[static_cast<size_t>(l.other)] = groups; stack.push_back(l.other); }
                }
            }
            ++groups;
            if (members.size() < 2) { continue; }
            auto groupCost = [&]() {
                float total = 0;
                for (int m : members) { total += faceCost(m, current(m)); }
                return total;
            };
            const std::vector<Orientation> before = state;
            const float now = groupCost();
            float best = now;
            std::vector<Orientation> bestState = state;
            for (int flip = 0; flip < 2; ++flip)
            {
                for (int steps = 0; steps < 4; ++steps)
                {
                    if (flip == 0 && steps == 0) { continue; }
                    std::vector<Orientation> trial = before;
                    for (int m : members)
                    {
                        state = before;
                        trial[static_cast<size_t>(m)] = compose(m, { flip != 0, steps % info[static_cast<size_t>(m)].corners });
                    }
                    state = trial;
                    const float c = groupCost();
                    if (c < best) { best = c; bestState = trial; }
                }
            }
            state = (best < now * options.maxRatio) ? bestState : before;
        }

        // 2. Face by face, each to whatever suits its neighbours best, until
        //    nothing moves.
        for (int pass = 0; pass < 16; ++pass)
        {
            bool moved = false;
            for (size_t f = 0; f < count; ++f)
            {
                if (links[f].empty()) { continue; }
                const FaceInfo& fi = info[f];
                const float now = faceCost(static_cast<int>(f), current(static_cast<int>(f)));
                Orientation bestO = state[f];
                float best = now;
                for (int flip = 0; flip < 2; ++flip)
                {
                    for (int steps = 0; steps < fi.corners; ++steps)
                    {
                        const float c = faceCost(static_cast<int>(f), oriented(static_cast<int>(f), { flip != 0, steps }));
                        if (c < best - 0.5f) { best = c; bestO = { flip != 0, steps }; }
                    }
                }
                if (best < now * options.maxRatio && (bestO.flip != state[f].flip || bestO.steps != state[f].steps))
                {
                    state[f] = bestO;
                    moved = true;
                }
            }
            if (!moved) { break; }
        }

        // What changed, and by how much its own edges improved.
        for (size_t f = 0; f < count; ++f)
        {
            if (state[f].flip == initial[f].flip && state[f].steps == initial[f].steps) { continue; }
            const auto after = state;
            state = initial;
            const float was = faceCost(static_cast<int>(f), current(static_cast<int>(f)));
            state = after;
            const float now = faceCost(static_cast<int>(f), current(static_cast<int>(f)));
            if (was < options.minMismatch || now > was * options.maxRatio) { continue; }
            if (static_cast<int>(links[f].size()) < options.minEdges) { continue; }
            const ModelQuad& q = level.quads[f];
            UvSeamSuspect s;
            s.face = static_cast<int>(f);
            s.vertices = { q.a, q.b, q.c, q.d };
            s.texIndex = info[f].texIndex;
            s.drawRoutine = q.flags;
            s.edges = static_cast<int>(links[f].size());
            s.currentCost = was;
            s.bestCost = now;
            s.flip = state[f].flip;
            s.steps = state[f].steps;
            s.alreadyPatched = info[f].patched;
            suspects.push_back(s);
        }

        std::sort(suspects.begin(), suspects.end(), [](const UvSeamSuspect& a, const UvSeamSuspect& b) {
            return (a.currentCost - a.bestCost) > (b.currentCost - b.bestCost);
        });
        return suspects;
    }
}
