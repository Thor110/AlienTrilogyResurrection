#include "MissionText.h"

#include <fstream>
#include <regex>
#include <sstream>
#include <stdexcept>

namespace ALTEngine::Formats
{
    namespace
    {
        std::vector<std::string> SplitLines(const std::string& text)
        {
            std::vector<std::string> lines;
            std::string current;
            for (char c : text)
            {
                if (c == '\n')
                {
                    if (!current.empty() && current.back() == '\r') { current.pop_back(); }
                    lines.push_back(current);
                    current.clear();
                }
                else
                {
                    current += c;
                }
            }
            if (!current.empty())
            {
                if (current.back() == '\r') { current.pop_back(); }
                lines.push_back(current);
            }
            return lines;
        }

        bool IsBlank(const std::string& line)
        {
            return line.find_first_not_of(" \t") == std::string::npos;
        }
    }

    BriefingLine MissionTextLoader::ParseLine(const std::string& line)
    {
        BriefingLine result;
        bool bright = false;
        std::string current;

        size_t i = 0;
        while (i < line.size())
        {
            if (line[i] == '&' && i + 1 < line.size() && (line[i + 1] == '0' || line[i + 1] == '1'))
            {
                if (!current.empty())
                {
                    result.segments.push_back({ bright, current });
                    current.clear();
                }
                bright = (line[i + 1] == '1');
                i += 2;
            }
            else
            {
                current += line[i];
                i += 1;
            }
        }
        if (!current.empty())
        {
            result.segments.push_back({ bright, current });
        }
        return result;
    }

    std::vector<MissionBriefing> MissionTextLoader::Load(const std::filesystem::path& path)
    {
        std::ifstream in(path, std::ios::binary);
        if (!in.is_open())
        {
            throw std::runtime_error("MissionTextLoader: could not open " + path.string());
        }
        std::stringstream buffer;
        buffer << in.rdbuf();
        std::vector<std::string> lines = SplitLines(buffer.str());

        static const std::regex headerPattern(R"(^(\d+\.\d+\.\d+)\s+(.+)$)");

        std::vector<MissionBriefing> result;
        size_t i = 0;

        while (i < lines.size())
        {
            // Look for the next entry header, skipping blank lines
            // (and anything else) in between.
            std::smatch match;
            if (!std::regex_match(lines[i], match, headerPattern))
            {
                ++i;
                continue;
            }

            MissionBriefing briefing;
            briefing.levelCode = match[1].str();
            briefing.title = match[2].str();
            ++i;

            if (i < lines.size()) { ++i; } // underline - not validated, just skipped
            if (i < lines.size() && lines[i] == "*") { ++i; } // opening '*'

            BriefingParagraph currentParagraph;
            while (i < lines.size() && lines[i] != "*")
            {
                if (lines[i] == "#")
                {
                    briefing.paragraphs.push_back(std::move(currentParagraph));
                    currentParagraph = BriefingParagraph{};
                }
                else
                {
                    currentParagraph.lines.push_back(ParseLine(lines[i]));
                }
                ++i;
            }
            briefing.paragraphs.push_back(std::move(currentParagraph));

            if (i < lines.size() && lines[i] == "*") { ++i; } // closing '*'

            // Optional "Icon: ..." line - skip blank lines looking for it,
            // but stop looking the moment we hit the next real content
            // (another header, or non-blank/non-Icon line).
            while (i < lines.size() && IsBlank(lines[i])) { ++i; }
            if (i < lines.size() && lines[i].rfind("Icon:", 0) == 0)
            {
                briefing.icon = lines[i].substr(5);
                while (!briefing.icon.empty() && briefing.icon.front() == ' ') { briefing.icon.erase(0, 1); }
                ++i;
            }

            result.push_back(std::move(briefing));
        }

        return result;
    }
}
