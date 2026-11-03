#pragma once

#include "../Formats/ModelLoader.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace ALTEngine::Game::TextureUse
{
    // WHICH OF A LEVEL'S TEXTURES ARE USED - the first step of the texture
    // blanking (data/TextureBlankList.json, ALTTextureBlankList). A level's
    // sheet (its NNNGFX.B16) is drawn from by three things only:
    //
    //   - its faces: every record of the level file, record 0 included (the
    //     first cell's first face, which the Level Cleanup gives back). A face
    //     whose routine is animated (8, 10) does not name a texture: its
    //     texture index is an animator ordinal (TextureAnimation.h);
    //   - its animators' frame programs (listB): every entry with bit 15 clear
    //     is a texture the program shows. All of them count, whether or not a
    //     face uses that animator - a script or a broken crate can start one;
    //   - its door and lift models (the D and L0 sections of the level file),
    //     on the same sheet.
    //
    // Nothing else reads a level sheet's texture rectangles: the crates and
    // pickups have their own sheets, and the cryopod cover's flat colour is a
    // palette entry. The scan is generous on purpose - it only keeps more.
    enum Use : uint8_t
    {
        FACE = 1,        // a face of the level names it
        FRAME = 2,       // an animator's program shows it
        MODEL = 4,       // a door or lift model's face names it
    };

    struct Result
    {
        std::vector<uint8_t> use;   // one per texture descriptor: Use bits, 0 = unused
        int outOfRange = 0;         // references past the sheet's descriptors (counted, ignored)
    };

    // `levelBytes` is the level file as the fixes leave it (before the Level
    // Cleanup, which is optional: what it takes out is still drawn without
    // it). `models` are its door and lift meshes. `descriptors` is the number
    // of texture rectangles its sheet has.
    bool Scan(const std::vector<uint8_t>& levelBytes, const std::vector<Formats::ModelMesh>& models, size_t descriptors,
              Result& result, std::string& error);
}
