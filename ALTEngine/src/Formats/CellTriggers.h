#pragma once

#include <cstdint>

namespace ALTEngine::Formats
{
    // CELL-ENTRY TRIGGERS - why enemies appear as you walk into a room.
    //
    // This is the mechanism the port was missing entirely, and it is what makes
    // an alien turn up on the far side of a barrier (Edward, 2026: "there is
    // usually an alien on the other side of the first barrier ... I am not seeing
    // it on ours").
    //
    // THE CHAIN:
    //   FUN_00031f3c   the entity mover. When an entity's cell changes, it calls
    //   FUN_0004129c   which reads the NEW cell's trigger id from cell byte +0xf.
    //                  Zero means no trigger. Otherwise it walks a linked list of
    //                  actions and runs each one.
    //   FUN_0002f224   the action that spawns a monster - case 3 below.
    //
    // THE GUARD THAT MATTERS: `bVar7 = entity[+0x71] != cell[+0xf]`. Every action
    // is conditional on that, and +0x71 is the trigger id the entity fired LAST.
    // So an action runs on ENTERING a trigger's area and not again while the
    // entity stays inside it - re-entering after leaving fires it once more.
    // Without that guard a spawn trigger would fire every tick you stood on it.
    //
    // THE ACTION LIST is at DAT_0024834c, four bytes per entry:
    //     +0  action id
    //     +1  index of the next entry, 0xff ends the list
    //     +2  first argument
    //     +3  second argument
    // and the trigger's own record at DAT_00248248 + id*4 holds flags at +0
    // (bit 4 enables it), a head index at +1, and a remaining-uses count at +2
    // which is decremented after a successful run unless it is 0xff (unlimited).
    //
    // THE ACTIONS:
    //     0  FUN_00029d44   object state change
    //     1  FUN_00029594   door, and it also drives a HUD notice via FUN_00053930
    //     2  FUN_0003bb14   lift
    //     3  FUN_0002f224   SPAWN A MONSTER - arg2 is the monster index, arg1 the
    //                       amount added to its trigger counter
    //     5  FUN_00041e20   with FUN_000537dc alongside for the notice
    //     7  FUN_00041dd8   with FUN_000537bc alongside
    //     8  raises bit 4 of DAT_000b0cc0 and sets DAT_000ace21 - the level exit
    //     9  FUN_00042fcc
    //
    // Actions 1, 5 and 7 each have a second call guarded by DAT_000ae0a4, which
    // is the same flag that moves the HUD elements - so those are the ones that
    // print a notice. That places the door and lift messages precisely.
    //
    // NOTE THE TRIGGER FIRES FOR ANY ENTITY, not only the player: the mover calls
    // it, and the mover runs for monsters too. So a creature can open a door or
    // set off a spawn by walking over the trigger.
    namespace CellTriggers
    {
        inline constexpr int CELL_TRIGGER_ID = 0x0f;      // byte in the collision cell
        inline constexpr int ENTITY_LAST_TRIGGER = 0x71;  // byte in the entity

        inline constexpr int TRIGGER_ENABLED_FLAG = 4;    // bit 4 of the trigger's flags
        inline constexpr int TRIGGER_UNLIMITED = 0xff;    // uses byte, never decremented
        inline constexpr int ACTION_LIST_END = 0xff;

        enum Action
        {
            ACTION_OBJECT = 0,
            ACTION_DOOR = 1,
            ACTION_LIFT = 2,
            ACTION_SPAWN_MONSTER = 3,
            ACTION_FIVE = 5,
            ACTION_SEVEN = 7,
            ACTION_LEVEL_EXIT = 8,
            ACTION_NINE = 9,
        };

        // An entity fires a trigger only when the cell's id differs from the one
        // it last fired - see the note above.
        inline bool ShouldFire(int lastTriggerId, int cellTriggerId)
        {
            return cellTriggerId != 0 && lastTriggerId != cellTriggerId;
        }
    }
}
