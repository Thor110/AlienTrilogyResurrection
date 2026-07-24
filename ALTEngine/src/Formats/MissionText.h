#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace ALTEngine::Formats
{
    // A single &0/&1-tagged colored run within a line.
    struct BriefingSegment
    {
        bool bright = false; // true = &1 (light green), false = &0 (dark green)
        std::string text;
    };

    // One line of a paragraph - a sequence of colored runs. An empty
    // segment list means a blank line (used for vertical spacing within
    // a paragraph - the source represents these as a line containing
    // just "&0" with no following text).
    struct BriefingLine
    {
        std::vector<BriefingSegment> segments;
    };

    // A '#'-delimited block of lines within an entry - typically rendered
    // as a paragraph with a blank line before the next one.
    struct BriefingParagraph
    {
        std::vector<BriefingLine> lines;
    };

    struct MissionBriefing
    {
        std::string levelCode; // "1.1.1"
        std::string title;     // "Entrance"
        std::vector<BriefingParagraph> paragraphs;
        std::string icon;      // "Crate & barrel" - empty if this entry has no Icon: line
    };

    // Parses a MISSION#.TXT file (CD/LANGUAGE/MISSIONE.TXT etc - one per
    // language) into a list of mission briefings, in file order.
    //
    // Line breaks within a paragraph are preserved exactly as they
    // appear in the source, not re-wrapped - the original text was
    // already hand-wrapped by the developers to fit the briefing box at
    // the game's fixed bitmap-font width, so reflowing it would be wrong
    // even if it happened to still fit.
    class MissionTextLoader
    {
    public:
        static std::vector<MissionBriefing> Load(const std::filesystem::path& path);

        // Exposed for testing/reuse - parses one line's &0/&1 markup into
        // colored segments. Color persists across tag boundaries within
        // the line (each tag stays active until the next tag or end of
        // line); starts as dark (&0) by default, since every real line
        // in the source begins with an explicit tag anyway.
        static BriefingLine ParseLine(const std::string& line);
    };
}
