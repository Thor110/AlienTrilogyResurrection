#include "TestFramework.h"

#include "Screens/PickupSystem.h"
#include "Screens/PlayerInventoryState.h"
#include "Screens/WeaponSystem.h"

using namespace ALTEngine::Screens;
namespace WS = ALTEngine::Screens::WeaponSystem;

namespace
{
    // Fires one round of `weapon` the way GameplayScreen does: fire, then let
    // the animation finish so the weapon is idle again.
    bool FireOnce(PlayerInventoryState& inv, int weapon)
    {
        WS::Runtime rt;
        rt.weapon = weapon;
        const bool fired = WS::TryPrimaryFire(rt, inv);
        WS::OnAnimationEnded(rt);
        WS::UpdateIdle(rt, inv, true);
        return fired;
    }
}

TEST_CASE("Inventory: a new loadout is the pistol with 45 rounds")
{
    PlayerInventoryState inv;
    CHECK(inv.Available(0));
    CHECK(!inv.Available(1));
    CHECK_EQ(inv.AmmoTotal(0), 45);
    CHECK_EQ(inv.pistol.rounds, 15);
    CHECK_EQ(inv.pistol.units, 2);
}

TEST_CASE("Inventory: weapon indices are the canonical order")
{
    PlayerInventoryState inv;
    CHECK(inv.ByIndex(0) == &inv.pistol);
    CHECK(inv.ByIndex(1) == &inv.shotgun);
    CHECK(inv.ByIndex(2) == &inv.pulseRifle);
    CHECK(inv.ByIndex(3) == &inv.flamethrower);
    CHECK(inv.ByIndex(4) == &inv.smartGun);
}

TEST_CASE("Inventory: firing spends the inventory's own rounds and reloads from spares")
{
    PlayerInventoryState inv;
    for (int i = 0; i < 15; ++i) { CHECK(FireOnce(inv, 0)); }
    CHECK_EQ(inv.AmmoTotal(0), 30);
    CHECK_EQ(inv.pistol.rounds, 15);    // reloaded
    CHECK_EQ(inv.pistol.units, 1);
}

TEST_CASE("Inventory: pickups still add ammo after the cap was reached once")
{
    // The regression: firing used to drain a separate HUD copy while pickups
    // clamped this one, so once enough had been picked up to reach the cap,
    // every later pickup added nothing at all.
    PlayerInventoryState inv;
    PlayerHudState hud;
    for (int i = 0; i < 20; ++i) { Pickups::Apply(Pickups::PISTOL_AMMO, 15, 1, inv, hud); }
    const int cap = Pickups::PISTOL_MAG_CAP * (Pickups::PISTOL_UNIT_CAP + 1);
    CHECK_EQ(inv.AmmoTotal(0), cap);

    for (int i = 0; i < 40; ++i) { FireOnce(inv, 0); }
    CHECK_EQ(inv.AmmoTotal(0), cap - 40);

    Pickups::Apply(Pickups::PISTOL_AMMO, 15, 1, inv, hud);
    CHECK_EQ(inv.AmmoTotal(0), cap - 10);
}

TEST_CASE("Inventory: pulse rifle and flamethrower ammo land on their own weapons")
{
    PlayerInventoryState inv;
    PlayerHudState hud;
    Pickups::Apply(Pickups::PULSE_AND_AMMO, 50, 0, inv, hud);
    CHECK(inv.Available(2));
    CHECK_EQ(inv.AmmoTotal(2), 50);
    CHECK_EQ(inv.AmmoTotal(3), 0);

    Pickups::Apply(Pickups::FLAMETHROWER_AND_AMMO, 30, 0, inv, hud);
    CHECK_EQ(inv.AmmoTotal(3), 30);
    CHECK_EQ(inv.AmmoTotal(2), 50);

    // Pickup type 3 is the smartgun, weapon 4.
    Pickups::Apply(Pickups::SMARTGUN_AND_AMMO, 30, 0, inv, hud);
    CHECK_EQ(inv.AmmoTotal(4), 30);
}

TEST_CASE("Inventory: Fully Loaded really refills what has been spent")
{
    PlayerInventoryState inv;
    inv.GiveEverything();
    CHECK_EQ(inv.AmmoTotal(2), 999);
    for (int i = 0; i < 10; ++i) { FireOnce(inv, 2); }
    CHECK_EQ(inv.AmmoTotal(2), 989);
    inv.GiveEverything();
    CHECK_EQ(inv.AmmoTotal(2), 999);
}

TEST_CASE("Inventory: a dry pistol is topped up with one round when nothing else can be held")
{
    PlayerInventoryState inv;
    inv.SetAmmoTotal(0, 0);
    WS::Runtime rt;
    rt.weapon = 0;
    WS::UpdateIdle(rt, inv, /*canSwitchAway*/ false);
    CHECK_EQ(inv.pistol.rounds, 1);
    CHECK_EQ(rt.state, static_cast<int>(WS::STATE_RELOAD));
}

TEST_CASE("Inventory: a total that divides exactly leaves the last magazine loaded")
{
    PlayerInventoryState inv;
    inv.SetAmmoTotal(2, 300);
    CHECK_EQ(inv.pulseRifle.rounds, 100);
    CHECK_EQ(inv.pulseRifle.units, 2);
    inv.SetAmmoTotal(1, 37);           // shotgun: no magazine
    CHECK_EQ(inv.shotgun.rounds, 37);
    CHECK_EQ(inv.shotgun.units, 0);
}
