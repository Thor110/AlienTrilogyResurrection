#include "DirectoryBrowser.h"
#include "Font8x8.h"
#include "AppWindow.h"
#include "PlatformPaths.h"
#include "FsUtil.h"

#include <SDL3/SDL.h>
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <string>

#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOGDI       // avoid windows.h macro-clobbering DrawText/Rectangle/etc.
#define NOMINMAX    // avoid windows.h macro-clobbering std::min/std::max/std::clamp
#include <windows.h>
#endif

namespace ALTEngine::Bootstrap
{
    namespace
    {
        constexpr Color COLOR_BG{ 0, 0, 0, 255 };
        constexpr Color COLOR_GREEN{ 51, 255, 102, 255 };
        constexpr Color COLOR_GREEN_DIM{ 24, 130, 52, 255 };
        constexpr Color COLOR_RED{ 255, 70, 70, 255 };

        std::string ToLower(std::string s)
        {
            std::transform(s.begin(), s.end(), s.begin(),
                            [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
            return s;
        }

        SDL_FRect ToFRect(int x, int y, int w, int h)
        {
            return SDL_FRect{ static_cast<float>(x), static_cast<float>(y), static_cast<float>(w), static_cast<float>(h) };
        }

        void DrawOutlineRect(SDL_Renderer* renderer, const SDL_FRect& rect, Color color, int thickness = 2)
        {
            SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
            for (int i = 0; i < thickness; ++i)
            {
                SDL_FRect r{ rect.x - i, rect.y - i, rect.w + i * 2.0f, rect.h + i * 2.0f };
                SDL_RenderRect(renderer, &r);
            }
        }

        void DrawFilledRect(SDL_Renderer* renderer, const SDL_FRect& rect, Color color)
        {
            SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
            SDL_RenderFillRect(renderer, &rect);
        }

        // Text cut to `room` pixels: from the left ("...tail") when `keepEnd`,
        // else from the right ("head..."), so nothing draws past its box.
        std::string FitRight(const std::string& text, int room, int scale, bool keepEnd)
        {
            if (room <= 0) { return std::string(); }
            if (TextWidth(text, scale) <= room) { return text; }
            const std::string dots = "...";
            std::string out = text;
            if (keepEnd)
            {
                while (!out.empty() && TextWidth(dots + out, scale) > room) { out.erase(out.begin()); }
                return dots + out;
            }
            while (!out.empty() && TextWidth(out + dots, scale) > room) { out.pop_back(); }
            return out + dots;
        }

        // Small filled folder glyph (body + tab), sized to fit in `box`.
        void DrawFolderIcon(SDL_Renderer* renderer, const SDL_FRect& box, Color color)
        {
            float tabW = box.w / 2.0f;
            float tabH = box.h / 4.0f;
            SDL_FRect tab{ box.x, box.y, tabW, tabH };
            SDL_FRect body{ box.x, box.y + tabH, box.w, box.h - tabH };
            DrawFilledRect(renderer, tab, color);
            DrawFilledRect(renderer, body, color);
        }
    }

    std::vector<DirectoryBrowser::Entry> DirectoryBrowser::BuildEntries(const std::filesystem::path& currentPath) const
    {
        std::vector<Entry> entries;

        if (currentPath.empty())
        {
#if defined(_WIN32)
            DWORD mask = GetLogicalDrives();
            for (int i = 0; i < 26; ++i)
            {
                if (!((mask >> i) & 1)) { continue; }
                std::string letter(1, static_cast<char>('A' + i));
                std::filesystem::path drive(letter + ":\\");

                UINT type = GetDriveTypeA(drive.string().c_str());
                if (type == DRIVE_NO_ROOT_DIR || type == DRIVE_UNKNOWN) { continue; }

                entries.push_back({ letter + ":\\", drive, false });
            }
#else
            entries.push_back({ "/", std::filesystem::path("/"), false });
            if (const char* home = std::getenv("HOME"))
            {
                entries.push_back({ std::string("Home (") + home + ")", std::filesystem::path(home), false });
            }
            // Discs and mounted images.
            for (const auto& root : MountRoots())
            {
                std::error_code rootEc;
                if (std::filesystem::is_directory(root, rootEc))
                {
                    entries.push_back({ root.string(), root, false });
                }
            }
#endif
            return entries;
        }

        entries.push_back({ "..", "", true });

        // Never throws: a folder that cannot be read lists nothing, one that
        // stops being readable part way lists what came before, and a name
        // outside the Windows code page is shown as UTF-8 (PathText).
        std::vector<Entry> subdirs;
        try
        {
            std::error_code ec;
            std::filesystem::directory_iterator it(currentPath, std::filesystem::directory_options::skip_permission_denied, ec);
            for (const std::filesystem::directory_iterator end; !ec && it != end; it.increment(ec))
            {
                std::error_code isDirEc;
                if (it->is_directory(isDirEc) && !isDirEc)
                {
                    subdirs.push_back({ PathText(it->path().filename()), it->path(), false });
                }
            }
        }
        catch (...) {}

        std::sort(subdirs.begin(), subdirs.end(), [](const Entry& a, const Entry& b) {
            return ToLower(a.displayName) < ToLower(b.displayName);
        });

        entries.insert(entries.end(), subdirs.begin(), subdirs.end());
        return entries;
    }

    std::optional<std::filesystem::path> DirectoryBrowser::Run(
        const std::string& headerText,
        const std::string& panelTitle,
        const std::function<bool(const std::filesystem::path&)>& validate)
    {
        AppWindow& app = AppWindow::Instance();
        if (!app.EnsureCreated())
        {
            return std::nullopt;
        }
        SDL_Renderer* renderer = app.Renderer();

        std::filesystem::path currentPath; // empty = drive list
        std::vector<Entry> entries = BuildEntries(currentPath);
        int selectedIndex = -1;
        int hoveredIndex = -1;
        int scrollOffset = 0;

        std::string errorMessage;
        Uint64 errorUntilTicks = 0;

        auto navigateInto = [&](const Entry& entry) {
            if (entry.isParent)
            {
                std::filesystem::path parent = currentPath.parent_path();
                if (parent != currentPath)
                {
                    // Subfolder -> its parent (which may itself be the
                    // drive root, e.g. "F:\AlienTrilogy" -> "F:\").
                    currentPath = parent;
                }
                else
                {
                    // Already at a drive root -> back to the drive list.
                    currentPath = std::filesystem::path();
                }
            }
            else if (!entry.fullPath.empty())
            {
                currentPath = entry.fullPath;
            }
            else
            {
                return; // e.g. an inaccessible entry with no path
            }
            entries = BuildEntries(currentPath);
            selectedIndex = -1;
            hoveredIndex = -1;
            scrollOffset = 0;
        };

        std::optional<std::filesystem::path> result;
        bool running = true;

        // THE PATH BOX: the current folder, editable. Click it (or press Tab)
        // to type or paste a path (Ctrl+V); Enter goes there, Escape gives up
        // the edit. Drawn in its own box left of SELECT, clipped to it.
        bool editing = false;
        std::string typed;
        SDL_Window* window = app.Window();
        auto beginEdit = [&] {
            if (editing) { return; }
            editing = true;
            typed = PathText(currentPath);
            SDL_StartTextInput(window);
        };
        auto endEdit = [&] {
            if (!editing) { return; }
            editing = false;
            SDL_StopTextInput(window);
        };
        auto showError = [&](const std::string& text) {
            errorMessage = text;
            errorUntilTicks = SDL_GetTicks() + 3000;
        };
        auto goToTyped = [&] {
            std::string text = typed;
            while (!text.empty() && (text.back() == ' ' || text.back() == '"')) { text.pop_back(); }
            while (!text.empty() && (text.front() == ' ' || text.front() == '"')) { text.erase(text.begin()); }
            const std::filesystem::path target = PathFromText(text);
            std::error_code ec;
            if (text.empty() || !std::filesystem::is_directory(target, ec))
            {
                showError("NO SUCH FOLDER.");
                return false;
            }
            endEdit();
            currentPath = target;
            entries = BuildEntries(currentPath);
            selectedIndex = -1;
            hoveredIndex = -1;
            scrollOffset = 0;
            return true;
        };
        auto trySelect = [&] {
            if (editing && !goToTyped()) { return; }
            if (!currentPath.empty() && validate(currentPath))
            {
                result = currentPath;
                running = false;
            }
            else
            {
                showError(currentPath.empty()
                    ? "SELECT A FOLDER FIRST."
                    : "DIRECTORY DOES NOT CONTAIN A RECOGNISED ALIEN TRILOGY INSTALLATION.");
            }
        };
        auto inside = [](const SDL_FRect& r, float x, float y) {
            return x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h;
        };

        while (running)
        {
            int windowW = 0, windowH = 0;
            SDL_GetRenderOutputSize(renderer, &windowW, &windowH);

            int scale = std::max(2, windowH / 480);
            int rowHeight = TextHeight(scale) + scale * 6;

            int panelX = windowW / 2 - static_cast<int>(windowW * 0.3);
            int panelY = static_cast<int>(windowH * 0.22);
            int panelW = static_cast<int>(windowW * 0.6);
            int panelH = static_cast<int>(windowH * 0.56);

            SDL_FRect panel = ToFRect(panelX, panelY, panelW, panelH);
            SDL_FRect titleBar = ToFRect(panelX, panelY, panelW, rowHeight + scale * 4);
            SDL_FRect buttonArea = ToFRect(panelX, panelY + panelH - rowHeight - scale * 4, panelW, rowHeight + scale * 4);
            SDL_FRect listBox = ToFRect(
                panelX, static_cast<int>(titleBar.y + titleBar.h),
                panelW, static_cast<int>(buttonArea.y - (titleBar.y + titleBar.h) - scale * 4));

            int selectButtonW = TextWidth("SELECT", scale) + scale * 12;
            SDL_FRect selectButton = ToFRect(
                static_cast<int>(buttonArea.x + buttonArea.w) - selectButtonW - scale * 2,
                static_cast<int>(buttonArea.y) + scale * 2,
                selectButtonW, rowHeight);
            SDL_FRect pathBox = ToFRect(
                static_cast<int>(buttonArea.x) + scale * 2, static_cast<int>(buttonArea.y) + scale * 2,
                std::max(0, static_cast<int>(selectButton.x - buttonArea.x) - scale * 6), rowHeight);

            int visibleRows = std::max(1, static_cast<int>(listBox.h) / rowHeight);
            int maxScroll = std::max(0, static_cast<int>(entries.size()) - visibleRows);
            scrollOffset = std::clamp(scrollOffset, 0, maxScroll);

            SDL_Event event;
            while (SDL_PollEvent(&event))
            {
                // Mouse positions come in window units; the layout is in the
                // renderer's pixels, which differ under display scaling - so
                // without this the button and rows sat elsewhere from where
                // they were drawn.
                SDL_ConvertEventToRenderCoordinates(renderer, &event);

                if (event.type == SDL_EVENT_QUIT)
                {
                    running = false;
                }
                else if (editing && event.type == SDL_EVENT_TEXT_INPUT)
                {
                    typed += event.text.text;
                }
                else if (event.type == SDL_EVENT_KEY_DOWN)
                {
                    const bool ctrl = (event.key.mod & (SDL_KMOD_CTRL | SDL_KMOD_GUI)) != 0;
                    if (editing)
                    {
                        switch (event.key.key)
                        {
                        case SDLK_ESCAPE: endEdit(); break;
                        case SDLK_TAB: endEdit(); break;
                        case SDLK_RETURN:
                        case SDLK_KP_ENTER: goToTyped(); break;
                        case SDLK_BACKSPACE:
                            // Whole UTF-8 characters.
                            while (!typed.empty() && (static_cast<unsigned char>(typed.back()) & 0xC0) == 0x80) { typed.pop_back(); }
                            if (!typed.empty()) { typed.pop_back(); }
                            break;
                        case SDLK_V:
                            if (ctrl)
                            {
                                if (char* clip = SDL_GetClipboardText())
                                {
                                    std::string text(clip);
                                    SDL_free(clip);
                                    text.erase(std::remove_if(text.begin(), text.end(), [](char c) { return c == '\r' || c == '\n'; }), text.end());
                                    typed += text;
                                }
                            }
                            break;
                        default:
                            break;
                        }
                        continue;
                    }
                    switch (event.key.key)
                    {
                    case SDLK_ESCAPE:
                        running = false;
                        break;
                    case SDLK_TAB:
                        beginEdit();
                        break;
                    case SDLK_V:
                        if (ctrl) { beginEdit(); typed.clear(); if (char* clip = SDL_GetClipboardText()) { typed = clip; SDL_free(clip); } }
                        break;
                    case SDLK_UP:
                        if (!entries.empty())
                        {
                            selectedIndex = std::max(0, (selectedIndex < 0 ? 0 : selectedIndex - 1));
                            if (selectedIndex < scrollOffset) { scrollOffset = selectedIndex; }
                        }
                        break;
                    case SDLK_DOWN:
                        if (!entries.empty())
                        {
                            selectedIndex = std::min(static_cast<int>(entries.size()) - 1,
                                                      (selectedIndex < 0 ? 0 : selectedIndex + 1));
                            if (selectedIndex >= scrollOffset + visibleRows) { scrollOffset = selectedIndex - visibleRows + 1; }
                        }
                        break;
                    case SDLK_RETURN:
                    case SDLK_KP_ENTER:
                        if (selectedIndex >= 0 && selectedIndex < static_cast<int>(entries.size()))
                        {
                            navigateInto(entries[selectedIndex]);
                        }
                        else { trySelect(); }
                        break;
                    case SDLK_BACKSPACE:
                        navigateInto(Entry{ "..", "", true });
                        break;
                    default:
                        break;
                    }
                }
                else if (event.type == SDL_EVENT_MOUSE_WHEEL)
                {
                    scrollOffset = std::clamp(scrollOffset - static_cast<int>(event.wheel.y), 0, maxScroll);
                }
                else if (event.type == SDL_EVENT_MOUSE_MOTION)
                {
                    float mx = event.motion.x, my = event.motion.y;
                    hoveredIndex = -1;
                    if (inside(listBox, mx, my))
                    {
                        int rowIndex = scrollOffset + static_cast<int>(my - listBox.y) / rowHeight;
                        if (rowIndex >= 0 && rowIndex < static_cast<int>(entries.size())) { hoveredIndex = rowIndex; }
                    }
                }
                else if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN && event.button.button == SDL_BUTTON_LEFT)
                {
                    float mx = event.button.x, my = event.button.y;

                    if (inside(selectButton, mx, my))
                    {
                        trySelect();
                    }
                    else if (inside(pathBox, mx, my))
                    {
                        beginEdit();
                    }
                    else
                    {
                        endEdit();
                        if (inside(listBox, mx, my))
                        {
                            const int rowIndex = scrollOffset + static_cast<int>(my - listBox.y) / rowHeight;
                            if (rowIndex >= 0 && rowIndex < static_cast<int>(entries.size()))
                            {
                                selectedIndex = rowIndex;
                                // SDL3 reports multi-clicks natively via `clicks`
                                // (OS double-click timing/distance thresholds).
                                if (event.button.clicks >= 2) { navigateInto(entries[static_cast<size_t>(rowIndex)]); }
                            }
                        }
                    }
                }
            }

            // --- draw ---
            SDL_SetRenderDrawColor(renderer, COLOR_BG.r, COLOR_BG.g, COLOR_BG.b, 255);
            SDL_RenderClear(renderer);

            DrawBitmapText(renderer, headerText, scale * 6, scale * 6, scale, COLOR_GREEN);

            DrawOutlineRect(renderer, panel, COLOR_GREEN);
            DrawOutlineRect(renderer, titleBar, COLOR_GREEN);
            DrawBitmapText(renderer, panelTitle, static_cast<int>(titleBar.x) + scale * 4, static_cast<int>(titleBar.y) + scale * 3, scale, COLOR_GREEN);

            DrawOutlineRect(renderer, listBox, COLOR_GREEN);
            const int textRoom = static_cast<int>(listBox.w) - TextHeight(scale) - scale * 12;
            for (int row = 0; row < visibleRows; ++row)
            {
                int index = scrollOffset + row;
                if (index >= static_cast<int>(entries.size())) { break; }

                SDL_FRect rowRect = ToFRect(static_cast<int>(listBox.x), static_cast<int>(listBox.y) + row * rowHeight,
                                             static_cast<int>(listBox.w), rowHeight);
                bool isHovered = (index == hoveredIndex);
                bool isSelected = (index == selectedIndex);
                if (isHovered || isSelected)
                {
                    DrawFilledRect(renderer, rowRect, Color{ 0, 40, 15, 255 });
                }

                SDL_FRect iconBox = ToFRect(static_cast<int>(rowRect.x) + scale * 4, static_cast<int>(rowRect.y) + scale * 3,
                                             TextHeight(scale), TextHeight(scale));
                DrawFolderIcon(renderer, iconBox, isSelected ? COLOR_GREEN : COLOR_GREEN_DIM);

                DrawBitmapText(renderer, FitRight(entries[index].displayName, textRoom, scale, false),
                         static_cast<int>(iconBox.x + iconBox.w) + scale * 4, static_cast<int>(rowRect.y) + scale * 3,
                         scale, isSelected ? COLOR_GREEN : COLOR_GREEN_DIM);
            }

            DrawOutlineRect(renderer, buttonArea, COLOR_GREEN);
            DrawOutlineRect(renderer, selectButton, COLOR_GREEN);
            DrawBitmapText(renderer, "SELECT", static_cast<int>(selectButton.x) + scale * 6, static_cast<int>(selectButton.y) + scale * 3, scale, COLOR_GREEN);

            // The path box: what is typed while editing (with a caret), else
            // the current folder. Its end shows when it is too long - the
            // part that names the folder.
            DrawOutlineRect(renderer, pathBox, editing ? COLOR_GREEN : COLOR_GREEN_DIM, 1);
            {
                const int room = static_cast<int>(pathBox.w) - scale * 6;
                const bool caret = editing && (SDL_GetTicks() / 500) % 2 == 0;
                std::string shown = editing ? typed : (currentPath.empty() ? std::string("CLICK HERE TO TYPE A PATH") : PathText(currentPath));
                shown = FitRight(shown + (editing ? "_" : ""), room, scale, true);
                if (editing && !caret && !shown.empty() && shown.back() == '_') { shown.back() = ' '; }
                DrawBitmapText(renderer, shown, static_cast<int>(pathBox.x) + scale * 3, static_cast<int>(pathBox.y) + scale * 3, scale,
                               editing ? COLOR_GREEN : COLOR_GREEN_DIM);
            }

            if (!errorMessage.empty() && SDL_GetTicks() < errorUntilTicks)
            {
                DrawBitmapText(renderer, errorMessage, panelX, panelY + panelH + scale * 6, scale, COLOR_RED);
            }

            SDL_RenderPresent(renderer);
        }
        endEdit();

        return result;
    }
}
