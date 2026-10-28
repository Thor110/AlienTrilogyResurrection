#pragma once

// A deliberately tiny test framework: self-registering TEST_CASEs, CHECK and
// REQUIRE, and a main() that runs everything (or the cases whose names contain
// argv[1]). No dependency to install through vcpkg, and nothing here needs SDL,
// so the tests run on any machine that can build ALTFormats.

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace AltTest
{
    struct SkipTest
    {
        std::string reason;
    };

    struct RequireFailed
    {
    };

    struct Case
    {
        const char* name;
        std::function<void()> body;
    };

    inline std::vector<Case>& Registry()
    {
        static std::vector<Case> cases;
        return cases;
    }

    inline int& FailureCount()
    {
        static int failures = 0;
        return failures;
    }

    struct Registrar
    {
        Registrar(const char* name, std::function<void()> body) { Registry().push_back({ name, std::move(body) }); }
    };

    inline void ReportFailure(const char* file, int line, const std::string& what)
    {
        std::printf("    %s:%d: FAILED: %s\n", file, line, what.c_str());
        FailureCount()++;
    }

    // Root of the demo disc copy committed under Notes/. Level fixtures come
    // from here; a test that needs one calls RequireDemoFile, which skips the
    // test rather than failing it if the file is absent.
    inline std::filesystem::path DemoRoot()
    {
#ifdef ALT_TEST_DEMO_ROOT
        return std::filesystem::path(ALT_TEST_DEMO_ROOT);
#else
        return {};
#endif
    }

    inline std::filesystem::path RequireDemoFile(const std::string& relative)
    {
        std::filesystem::path path = DemoRoot() / relative;
        std::error_code ec;
        if (DemoRoot().empty() || !std::filesystem::exists(path, ec))
        {
            throw SkipTest{ "fixture not found: " + path.string() };
        }
        return path;
    }

    inline int RunAll(int argc, char** argv)
    {
        const std::string filter = argc > 1 ? argv[1] : "";
        int run = 0, skipped = 0, failedCases = 0;
        for (const Case& c : Registry())
        {
            if (!filter.empty() && std::string(c.name).find(filter) == std::string::npos) { continue; }
            const int before = FailureCount();
            try
            {
                c.body();
            }
            catch (const SkipTest& s)
            {
                std::printf("[ SKIP ] %s (%s)\n", c.name, s.reason.c_str());
                skipped++;
                continue;
            }
            catch (const RequireFailed&)
            {
            }
            catch (const std::exception& e)
            {
                ReportFailure(__FILE__, __LINE__, std::string("unexpected exception: ") + e.what());
            }
            run++;
            const bool ok = FailureCount() == before;
            if (!ok) { failedCases++; }
            std::printf("[ %s ] %s\n", ok ? " OK " : "FAIL", c.name);
        }
        std::printf("\n%d run, %d failed, %d skipped\n", run, failedCases, skipped);
        return failedCases == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
    }
}

#define ALT_TEST_CONCAT_INNER(a, b) a##b
#define ALT_TEST_CONCAT(a, b) ALT_TEST_CONCAT_INNER(a, b)

#define TEST_CASE(name)                                                                                    \
    static void ALT_TEST_CONCAT(AltTestBody_, __LINE__)();                                                 \
    static ::AltTest::Registrar ALT_TEST_CONCAT(AltTestRegistrar_, __LINE__)(name, &ALT_TEST_CONCAT(AltTestBody_, __LINE__)); \
    static void ALT_TEST_CONCAT(AltTestBody_, __LINE__)()

#define CHECK(expr)                                                                         \
    do { if (!(expr)) { ::AltTest::ReportFailure(__FILE__, __LINE__, #expr); } } while (0)

#define REQUIRE(expr)                                                                       \
    do { if (!(expr)) { ::AltTest::ReportFailure(__FILE__, __LINE__, #expr); throw ::AltTest::RequireFailed{}; } } while (0)

#define CHECK_EQ(actual, expected)                                                          \
    do {                                                                                    \
        const auto& altA_ = (actual); const auto& altE_ = (expected);                       \
        if (!(altA_ == altE_)) {                                                            \
            ::AltTest::ReportFailure(__FILE__, __LINE__, std::string(#actual " == " #expected) \
                + "  (got " + std::to_string(altA_) + ", expected " + std::to_string(altE_) + ")"); \
        }                                                                                   \
    } while (0)
