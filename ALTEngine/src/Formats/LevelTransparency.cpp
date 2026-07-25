#include "LevelTransparency.h"

namespace ALTEngine::Formats
{
    std::vector<int> GetLevelTransparencyIndices(int levelId, int groupIndex)
    {
        switch (levelId)
        {
        case 111: case 113: case 114: case 115: case 121:
            switch (groupIndex)
            {
            case 0: case 2: case 4: return { 255 };
            case 1: case 3: return { 0 };
            }
            break;

        case 112:
            switch (groupIndex)
            {
            case 0: return {};
            case 1: case 3: return { 0 };
            case 2: case 4: return { 255 };
            }
            break;

        case 122: case 213:
            switch (groupIndex)
            {
            case 0: return { 255 };
            case 1: case 2: case 3: case 4: return {};
            }
            break;

        case 131: case 211: case 212: case 231: case 232: case 242: case 243:
        case 262: case 331: case 361: case 391: case 901: case 906: case 907:
            return {};

        case 141: case 155: case 161: case 162: case 263: case 311: case 352:
        case 353: case 381: case 902: case 903: case 908: case 909:
            switch (groupIndex)
            {
            case 0: case 1: case 2: case 3: return {};
            case 4: return { 255 };
            }
            break;

        case 154: case 321: case 322: case 323: case 324: case 325:
            switch (groupIndex)
            {
            case 0: case 4: return { 255 };
            case 1: case 2: case 3: return {};
            }
            break;

        case 222:
            switch (groupIndex)
            {
            case 0: case 1: case 3: case 4: return {};
            case 2: return { 255 };
            }
            break;

        case 351: case 371:
            switch (groupIndex)
            {
            case 0: case 1: case 3: return {};
            case 2: case 4: return { 255 };
            }
            break;

        case 900:
            switch (groupIndex)
            {
            case 0: return { 255 };
            case 1: case 3: return { 0 };
            case 2: case 4: return {};
            }
            break;

        case 904: case 905:
            switch (groupIndex)
            {
            case 0: return { 255 };
            case 1: case 2: case 3: return {};
            case 4: return { 255 };
            }
            break;

        default:
            break;
        }

        return {};
    }
}
