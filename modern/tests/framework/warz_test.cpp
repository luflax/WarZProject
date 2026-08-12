// Runner for the minimal test framework. See warz_test.h for why it exists.

#include "warz_test.h"

#include <algorithm>
#include <cstring>
#include <exception>

namespace warz_test {

std::vector<TestCase>& registry()
{
    // Function-local static: test cases register from static initialisers in other
    // translation units, and the order between those is unspecified. A namespace-scope
    // vector could still be unconstructed when the first Registrar runs.
    static std::vector<TestCase> cases;
    return cases;
}

Stats& stats()
{
    static Stats s;
    return s;
}

namespace {
// Set by run() around each case so report_failure can attribute a failure and count
// each failing case exactly once however many assertions inside it fail.
const TestCase* g_current      = nullptr;
bool            g_current_bad  = false;
bool            g_verbose      = false;
} // namespace

std::string to_string(long long v)
{
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%lld", v);
    return buf;
}

std::string to_string(unsigned long long v)
{
    char buf[32];
    std::snprintf(buf, sizeof(buf), "%llu", v);
    return buf;
}

std::string to_string(double v)
{
    // %.9g round-trips a float32 exactly and stays readable for the whole-number
    // matrix entries that make up most of what these tests compare.
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%.9g", v);
    return buf;
}

std::string to_string(const char* v)      { return v ? std::string(v) : std::string("(null)"); }
std::string to_string(const std::string& v) { return v; }

bool report_failure(const char* file, int line, const char* expr, const std::string& detail)
{
    ++stats().checks_failed;
    if (!g_current_bad) {
        g_current_bad = true;
        ++stats().cases_failed;
    }

    std::printf("  FAIL  %s:%d\n", file, line);
    std::printf("        %s\n", expr);
    if (!detail.empty())
        std::printf("        %s\n", detail.c_str());
    std::fflush(stdout);
    return false;
}

namespace {

void print_usage(const char* argv0)
{
    std::printf(
        "usage: %s [--list] [--filter=SUBSTRING] [--verbose]\n"
        "\n"
        "  --list             print registered cases as 'suite.name' and exit\n"
        "  --filter=SUB       run only cases whose 'suite.name' contains SUB\n"
        "  --verbose          print every case as it runs, not just failures\n",
        argv0);
}

bool matches(const TestCase& tc, const std::string& filter)
{
    if (filter.empty()) return true;
    const std::string full = std::string(tc.suite) + "." + tc.name;
    return full.find(filter) != std::string::npos;
}

} // namespace

int run(int argc, char** argv)
{
    std::string filter;
    bool        list_only = false;

    for (int i = 1; i < argc; ++i) {
        const char* a = argv[i];
        if (std::strcmp(a, "--list") == 0) {
            list_only = true;
        } else if (std::strcmp(a, "--verbose") == 0) {
            g_verbose = true;
        } else if (std::strncmp(a, "--filter=", 9) == 0) {
            filter = a + 9;
        } else if (std::strcmp(a, "--help") == 0 || std::strcmp(a, "-h") == 0) {
            print_usage(argv[0]);
            return 0;
        } else {
            std::printf("unknown argument: %s\n\n", a);
            print_usage(argv[0]);
            return 2;
        }
    }

    // Deterministic order. Registration order is link order, which is stable for a
    // given build but changes when sources are added or reordered -- and a test suite
    // whose order depends on the link line is a suite that hides order dependencies.
    std::vector<TestCase> cases = registry();
    std::sort(cases.begin(), cases.end(), [](const TestCase& a, const TestCase& b) {
        const int by_suite = std::strcmp(a.suite, b.suite);
        return by_suite != 0 ? by_suite < 0 : std::strcmp(a.name, b.name) < 0;
    });

    if (list_only) {
        for (const TestCase& tc : cases)
            std::printf("%s.%s\n", tc.suite, tc.name);
        return 0;
    }

    for (const TestCase& tc : cases) {
        if (!matches(tc, filter)) continue;

        g_current     = &tc;
        g_current_bad = false;
        ++stats().cases_run;

        if (g_verbose) {
            std::printf("RUN   %s.%s\n", tc.suite, tc.name);
            std::fflush(stdout);
        }

        try {
            tc.fn();
        } catch (const AbortTestCase&) {
            // REQUIRE already reported; nothing more to say.
        } catch (const std::exception& e) {
            // Count it as a check as well as a failure. report_failure only bumps the
            // failure counter -- the assertion macros bump checks_run themselves -- so
            // without this an escaping exception makes checks_failed exceed the number
            // of checks attributed to the run, and the "N/M checks" summary reads as
            // though passing checks had failed.
            ++stats().checks_run;
            report_failure(tc.file, tc.line, "unhandled std::exception",
                           std::string("what(): ") + e.what());
        } catch (...) {
            ++stats().checks_run;
            report_failure(tc.file, tc.line, "unhandled unknown exception", std::string());
        }

        if (g_current_bad)
            std::printf("FAILED %s.%s\n\n", tc.suite, tc.name);
        else if (g_verbose)
            std::printf("  ok   %s.%s\n", tc.suite, tc.name);
    }

    g_current = nullptr;

    const Stats& s = stats();
    std::printf("\n%s: %d/%d cases, %d/%d checks\n",
                s.cases_failed == 0 ? "PASSED" : "FAILED",
                s.cases_run - s.cases_failed, s.cases_run,
                s.checks_run - s.checks_failed, s.checks_run);

    if (s.cases_run == 0) {
        // An empty run is a configuration failure, not a pass. This is what catches a
        // static library linked without --whole-archive: every case is registered from
        // a static initialiser in an object file nothing references, so the linker
        // drops them all and the suite reports a cheerful zero.
        std::printf("ERROR: no test cases ran%s\n",
                    filter.empty() ? "" : " (check --filter)");
        return 1;
    }

    return s.cases_failed == 0 ? 0 : 1;
}

} // namespace warz_test

int main(int argc, char** argv)
{
    return warz_test::run(argc, argv);
}
