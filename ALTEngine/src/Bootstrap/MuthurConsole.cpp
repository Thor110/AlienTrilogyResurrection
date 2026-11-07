#include "MuthurConsole.h"
#include "AppWindow.h"
#include "Font8x8.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cctype>
#include <deque>
#include <iostream>
#include <streambuf>
#include <thread>
#include <vector>

namespace ALTEngine::Bootstrap
{
    namespace
    {
        constexpr Color COLOR_GREEN{ 51, 255, 102, 255 };
        constexpr Color COLOR_GREEN_DIM{ 24, 130, 52, 255 };
        constexpr size_t KEEP_LINES = 400;
        constexpr Uint64 CHAR_MS = 3;        // a headline line types out at this pace
        constexpr Uint64 FRAME_MS = 16;      // the per-file lines draw at most this often
        constexpr Uint64 END_HOLD_MS = 900;  // an ordinary start's closing pause
        bool quitRequested = false;
        bool waitForKey = false;
        MuthurConsole::State* current = nullptr;
    }

    struct MuthurConsole::State
    {
        std::string title;
        std::deque<std::string> lines;
        std::string pending;          // the line being written, not yet ended
        std::thread::id owner = std::this_thread::get_id();
        bool skipTyping = false;
        Uint64 lastPresent = 0;
        bool hasWindow = false;

        // Events: a key or click skips the typing; closing asks to stop.
        void Pump()
        {
            SDL_Event e;
            while (SDL_PollEvent(&e))
            {
                if (e.type == SDL_EVENT_QUIT) { quitRequested = true; skipTyping = true; }
                else if (e.type == SDL_EVENT_KEY_DOWN || e.type == SDL_EVENT_MOUSE_BUTTON_DOWN) { skipTyping = true; }
            }
        }

        // The whole readout, `typed` characters of the last line shown.
        void Draw(size_t typed, bool cursor)
        {
            if (!hasWindow) { return; }
            SDL_Renderer* renderer = AppWindow::Instance().Renderer();
            int w = 0, h = 0;
            SDL_GetRenderOutputSize(renderer, &w, &h);
            const int scale = std::max(1, (h + 240) / 480);   // 2 from 720 lines up
            const int lineH = TextHeight(scale) + scale * 3;
            const int margin = scale * 8;

            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
            SDL_RenderClear(renderer);
            DrawBitmapText(renderer, title, margin, margin, scale * 2, COLOR_GREEN);

            const SDL_FRect panel{ float(margin), float(margin + TextHeight(scale * 2) + scale * 6),
                                   float(w - margin * 2), float(h - margin * 3 - TextHeight(scale * 2) - scale * 6) };
            SDL_SetRenderDrawColor(renderer, COLOR_GREEN.r, COLOR_GREEN.g, COLOR_GREEN.b, 255);
            for (int i = 0; i < 2; ++i)
            {
                SDL_FRect r{ panel.x - i, panel.y - i, panel.w + i * 2.0f, panel.h + i * 2.0f };
                SDL_RenderRect(renderer, &r);
            }

            // Wrapped to the panel; the newest at the bottom.
            const int room = std::max(1, (int(panel.w) - scale * 12 + scale) / (9 * scale));   // DrawBitmapText's advance: 8 + 1 per character
            // A row: its text, and whether it is bright - a headline line, or
            // the one being typed; a detail line (indented, per file) is dim,
            // all its wrapped rows alike.
            // Only as many as the panel shows: wrapped from the newest line
            // back until it is full (the readout keeps 400 lines, and a
            // headline redraws once a character while it types).
            const int fit = std::max(1, (int(panel.h) - scale * 8) / lineH);
            std::deque<std::pair<std::string, bool>> rows;
            std::vector<std::pair<std::string, bool>> wrapped;
            for (size_t i = lines.size(); i-- > 0 && rows.size() < size_t(fit);)
            {
                std::string text = lines[i];
                const bool last = i + 1 == lines.size();
                const bool bright = last || text.rfind("  ", 0) != 0;
                if (last) { text = text.substr(0, std::min(typed, text.size())) + (cursor ? "_" : ""); }
                wrapped.clear();
                if (text.empty()) { wrapped.push_back({ "", bright }); }
                // After the last comma that fits, so a count stays with its
                // word ("0 FAILED"); else at a space; else mid-word. A
                // continuation row does not start with the space after it.
                for (size_t p = 0; p < text.size();)
                {
                    size_t n = std::min(size_t(room), text.size() - p);
                    if (p + n < text.size())
                    {
                        const size_t limit = p + n - 1;
                        const size_t comma = text.rfind(',', limit);
                        const size_t space = text.rfind(' ', p + n);
                        if (comma != std::string::npos && comma > p + size_t(room) / 4) { n = comma - p + 1; }
                        else if (space != std::string::npos && space > p + size_t(room) / 2) { n = space - p + 1; }
                    }
                    wrapped.push_back({ text.substr(p, n), bright });
                    p += n;
                    while (p < text.size() && text[p] == ' ' && text[p - 1] == ',') { ++p; }
                }
                rows.insert(rows.begin(), wrapped.begin(), wrapped.end());
            }
            const size_t first = rows.size() > size_t(fit) ? rows.size() - size_t(fit) : 0;
            int y = int(panel.y) + scale * 4;
            for (size_t r = first; r < rows.size(); ++r)
            {
                DrawBitmapText(renderer, rows[r].first, int(panel.x) + scale * 6, y, scale,
                               rows[r].second ? COLOR_GREEN : COLOR_GREEN_DIM);
                y += lineH;
            }
            SDL_RenderPresent(renderer);
            lastPresent = SDL_GetTicks();
        }

        // A finished line: a headline is typed out, a detail line (indented,
        // as the per-file ones are) just appears.
        void Add(std::string line)
        {
            std::transform(line.begin(), line.end(), line.begin(),
                           [](unsigned char c) { return c < 0x80 ? static_cast<char>(std::toupper(c)) : '?'; });
            lines.push_back(line);
            while (lines.size() > KEEP_LINES) { lines.pop_front(); }
            if (!hasWindow || std::this_thread::get_id() != owner) { return; }

            const bool detail = line.rfind("  ", 0) == 0;
            if (detail || skipTyping)
            {
                Pump();
                if (SDL_GetTicks() - lastPresent >= FRAME_MS) { Draw(line.size(), false); }
                return;
            }
            const Uint64 start = SDL_GetTicks();
            for (;;)
            {
                Pump();
                const size_t typed = skipTyping ? line.size() : size_t((SDL_GetTicks() - start) / CHAR_MS);
                Draw(std::min(typed, line.size()), typed < line.size());
                if (typed >= line.size()) { break; }
                SDL_Delay(1);
            }
        }
    };

    namespace
    {
        // Writes through to the command window and hands each finished line
        // to the readout.
        class TeeBuffer : public std::streambuf
        {
        public:
            TeeBuffer(std::streambuf* out, MuthurConsole::State& readout) : through(out), state(readout) {}

        protected:
            int overflow(int c) override
            {
                if (c == traits_type::eof()) { return sync() == 0 ? 0 : traits_type::eof(); }
                if (through) { through->sputc(static_cast<char>(c)); }
                if (c == '\n') { state.Add(state.pending); state.pending.clear(); }
                else if (c != '\r') { state.pending += static_cast<char>(c); }
                return c;
            }
            std::streamsize xsputn(const char* s, std::streamsize n) override
            {
                for (std::streamsize i = 0; i < n; ++i) { overflow(static_cast<unsigned char>(s[i])); }
                return n;
            }
            int sync() override { return through ? through->pubsync() : 0; }

        private:
            std::streambuf* through;
            MuthurConsole::State& state;
        };
    }

    MuthurConsole::MuthurConsole(const std::string& title) : state(std::make_unique<State>())
    {
        state->title = title;
        state->hasWindow = AppWindow::Instance().EnsureCreated() && AppWindow::Instance().Renderer() != nullptr;
        previous = std::cout.rdbuf();
        tee = std::make_unique<TeeBuffer>(previous, *state);
        std::cout.rdbuf(tee.get());
        current = state.get();
        if (state->hasWindow) { state->Draw(0, true); }
    }

    MuthurConsole::~MuthurConsole()
    {
        std::cout.flush();
        if (!state->pending.empty()) { state->Add(state->pending); state->pending.clear(); }
        if (state->hasWindow && !quitRequested)
        {
            if (waitForKey) { state->skipTyping = false; state->Add("PRESS ANY KEY TO CONTINUE."); }
            const Uint64 start = SDL_GetTicks();
            state->skipTyping = false;
            for (;;)
            {
                state->Pump();
                if (quitRequested || state->skipTyping) { break; }
                if (!waitForKey && SDL_GetTicks() - start >= END_HOLD_MS) { break; }
                state->Draw(state->lines.empty() ? 0 : state->lines.back().size(), (SDL_GetTicks() / 400) % 2 == 0);
                SDL_Delay(16);
            }
        }
        std::cout.rdbuf(previous);
        current = nullptr;
        waitForKey = false;
    }

    void MuthurConsole::WaitForKeyAtEnd() { waitForKey = true; }
    bool MuthurConsole::QuitRequested() { return quitRequested; }
    void MuthurConsole::Redraw()
    {
        if (current && current->hasWindow && std::this_thread::get_id() == current->owner)
        {
            current->Draw(current->lines.empty() ? 0 : current->lines.back().size(), false);
        }
    }
}
