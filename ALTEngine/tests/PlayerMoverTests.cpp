#include "TestFramework.h"

#include "Game/PlayerMover.h"

#include <vector>

namespace PM = ALTEngine::Game::PlayerMover;
using ALTEngine::Formats::CollisionNode;

namespace
{
    // An 8 x 8 room of open floor at height 0 with a 0x20-unit ceiling.
    struct Room
    {
        std::vector<CollisionNode> cells = std::vector<CollisionNode>(64);
        PM::World w;
        Room()
        {
            for (auto& c : cells) { c.ceilingHeight = 0x20; }
            w.cell = [this](int x, int z) -> const CollisionNode* {
                if (x < 0 || z < 0 || (x >> 9) >= 8 || (z >> 9) >= 8) { return nullptr; }
                return &cells[static_cast<size_t>((z >> 9) * 8 + (x >> 9))];
            };
            w.floor = [this](int x, int z) { const auto* c = w.cell(x, z); return c ? c->floorHeight * 32 : 0; };
            w.ceiling = [this](int x, int z) { const auto* c = w.cell(x, z); return c ? c->ceilingHeight * 32 : 0; };
            w.occupied = [](int, int) { return false; };
        }
        CollisionNode& At(int cx, int cz) { return cells[static_cast<size_t>(cz * 8 + cx)]; }
    };
}

TEST_CASE("PlayerMover: open floor moves the whole step")
{
    Room r;
    CHECK_EQ(PM::MoveX(r.w, 0x700, 0x700, 0, 0x40), 0x740);
    CHECK_EQ(PM::MoveZ(r.w, 0x700, 0x700, 0, -0x40), 0x6c0);
}

TEST_CASE("PlayerMover: a wall stops the player 200 short of it")
{
    Room r;
    r.At(4, 3).attribute = 1;                     // wall cell starts at x = 0x800
    // Probe = 0x780 + 0x40 + 200 lands in the wall: put 201 short of its edge.
    CHECK_EQ(PM::MoveX(r.w, 0x780, 0x700, 0, 0x40), 0x800 - 201);
    // Walls only from the attribute: the old +2/+3 bytes don't stop anything.
    Room open;
    open.At(4, 3).unknown3 = 255;
    CHECK_EQ(PM::MoveX(open.w, 0x780, 0x700, 0, 0x40), 0x7c0);
}

TEST_CASE("PlayerMover: rises over 0x100 and low ceilings stop it, drops step clear of the lip")
{
    Room r;
    r.At(4, 3).floorHeight = 9;                   // 0x120 up
    CHECK_EQ(PM::MoveX(r.w, 0x780, 0x700, 0, 0x40), 0x800 - 201);
    r.At(4, 3).floorHeight = 8;                   // exactly 0x100: allowed
    CHECK_EQ(PM::MoveX(r.w, 0x780, 0x700, 0, 0x40), 0x7c0);
    r.At(4, 3).floorHeight = 0;
    r.At(4, 3).ceilingHeight = 0x17;              // under 0x300 headroom
    CHECK_EQ(PM::MoveX(r.w, 0x780, 0x700, 0, 0x40), 0x800 - 201);

    // Standing at 0x200 high and walking off into a pit at 0: the step lands
    // just inside the low cell and is pushed 0xc9 into it.
    Room drop;
    for (auto& c : drop.cells) { c.ceilingHeight = 0x40; }
    for (int cx = 0; cx < 4; ++cx) { for (int cz = 0; cz < 8; ++cz) { drop.At(cx, cz).floorHeight = 0x10; } }
    CHECK_EQ(PM::MoveX(drop.w, 0x7f0, 0x700, 0x200, 0x20), 0x800 + 0xc9);
}

TEST_CASE("PlayerMover: a diagonal wall keeps the player on its side")
{
    Room r;
    r.At(4, 3).attribute = 0x1a;                  // x within the cell kept above z
    const int x = PM::ClampDiagonalX(r.w, 0x800 + 0x20, 0x600 + 0x80);
    CHECK_EQ(x, 0x800 + 0x81);
}

TEST_CASE("PlayerMover: slippery ramps push downhill")
{
    int x = 0, z = 0;
    CHECK(PM::RampPush(0x35, 0, x, z));
    CHECK_EQ(z, -0x80);
    CHECK(PM::RampPush(0x36, 0x40, x, z));
    CHECK_EQ(x, 0x40);
    CHECK(!PM::RampPush(0x31, 0, x, z));
}

TEST_CASE("PlayerMover: walking into a wall at an angle slides along it at full speed")
{
    Room r;
    for (int cz = 0; cz < 8; ++cz) { r.At(4, cz).attribute = 1; }   // a wall along x = 0x800
    int x = 0x700, z = 0x200;
    for (int t = 0; t < 12; ++t)
    {
        const int nx = PM::MoveX(r.w, x, z, 0, 0x30);
        const int nz = PM::MoveZ(r.w, nx, z, 0, 0x30);
        if (t >= 3) { CHECK_EQ(nz - z, 0x30); }   // never held back along the wall
        x = nx; z = nz;
    }
    CHECK_EQ(x, 0x800 - 201);
}
