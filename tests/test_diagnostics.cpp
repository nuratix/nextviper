#include "test_runner.hpp"
#include "nextviper/diagnostic.hpp"

using namespace nextviper;

NV_TEST(Diagnostics, RenderErrorReport) {
    SourceManager sm;
    sm.add_file("app.nv", "let x = 10\nlet y = (20 +\nlet z = 30");

    DiagnosticEngine diag(sm, false);
    diag.error("expected expression after '+'", SourceSpan(SourceLocation{2, 14, 25}, SourceLocation{2, 15, 26}, "app.nv"), "did you forget an operand?");

    NV_ASSERT_TRUE(diag.has_errors());
    NV_ASSERT_EQ(diag.error_count(), 1);

    std::ostringstream ss;
    diag.render(ss);
    std::string output = ss.str();

    NV_ASSERT(output.find("error[NV1002]:") != std::string::npos || output.find("error[NV100]:") != std::string::npos);
    NV_ASSERT(output.find("expected expression after '+'") != std::string::npos);
    NV_ASSERT(output.find("--> app.nv:2:14") != std::string::npos);
    NV_ASSERT(output.find("let y = (20 +") != std::string::npos);
    NV_ASSERT(output.find("help: did you forget an operand?") != std::string::npos);
}

NV_TEST(Diagnostics, MultipleDiagnosticLevels) {
    SourceManager sm;
    sm.add_file("types.nv", "let a = 1\nlet b = 2");

    DiagnosticEngine diag(sm, false);
    diag.warning("variable 'a' is unused", SourceSpan(SourceLocation{1, 5, 4}, SourceLocation{1, 6, 5}, "types.nv"), "consider prefixing with '_'");
    diag.note("type was inferred as Int", SourceSpan(SourceLocation{1, 5, 4}, SourceLocation{1, 6, 5}, "types.nv"));

    NV_ASSERT_FALSE(diag.has_errors());
    NV_ASSERT_EQ(diag.warning_count(), 1);

    std::ostringstream ss;
    diag.render(ss);
    std::string output = ss.str();

    NV_ASSERT(output.find("warning[NV200]:") != std::string::npos);
    NV_ASSERT(output.find("variable 'a' is unused") != std::string::npos);
    NV_ASSERT(output.find("note:") != std::string::npos);
    NV_ASSERT(output.find("type was inferred as Int") != std::string::npos);
}
