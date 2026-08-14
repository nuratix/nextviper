#include "test_runner.hpp"
#include "nextviper/lexer.hpp"
#include "nextviper/parser.hpp"
#include "nextviper/interpreter.hpp"
#include "nextviper/module.hpp"
#include <fstream>

using namespace nextviper;

static bool eval_script(const std::string& src, DiagnosticEngine& diag, Interpreter& interp, const std::string& path = "test_mod.nv") {
    interp.set_current_file(path);
    Lexer lexer(src, path, diag);
    auto tokens = lexer.tokenize();
    Parser parser(tokens, diag);
    auto program = parser.parse_program();
    if (diag.has_errors() || !program) return false;
    return interp.execute(*program);
}

NV_TEST(Modules, ImportMathFullAndSelective) {
    SourceManager sm;
    DiagnosticEngine diag(sm, false);
    Interpreter interp(diag);

    std::string src = "import math\n"
                      "from math import sqrt, pi\n"
                      "from math import pow as power\n"
                      "\n"
                      "let root16 = math.sqrt(16.0)\n"
                      "let root25 = sqrt(25.0)\n"
                      "let my_pi = pi\n"
                      "let two_cubed = power(2.0, 3.0)\n";

    NV_ASSERT_TRUE(eval_script(src, diag, interp));

    auto r16 = interp.globals()->get("root16");
    NV_ASSERT_TRUE(r16.has_value() && r16->is_float() && r16->as_float() == 4.0);

    auto r25 = interp.globals()->get("root25");
    NV_ASSERT_TRUE(r25.has_value() && r25->is_float() && r25->as_float() == 5.0);

    auto p = interp.globals()->get("my_pi");
    NV_ASSERT_TRUE(p.has_value() && p->is_float());

    auto cubed = interp.globals()->get("two_cubed");
    NV_ASSERT_TRUE(cubed.has_value() && cubed->is_float() && cubed->as_float() == 8.0);
}

NV_TEST(Modules, ImportDataAndSysStdlib) {
    SourceManager sm;
    DiagnosticEngine diag(sm, false);
    Interpreter interp(diag);

    std::string src = "import data\n"
                      "import sys\n"
                      "\n"
                      "let avg = data.mean([10, 20, 30])\n"
                      "let total = data.sum([1, 2, 3, 4, 5])\n"
                      "let chunks = data.chunk([1, 2, 3, 4, 5], 2)\n"
                      "let sys_ver = sys.version\n";

    NV_ASSERT_TRUE(eval_script(src, diag, interp));

    auto avg = interp.globals()->get("avg");
    NV_ASSERT_TRUE(avg.has_value() && avg->is_float() && avg->as_float() == 20.0);

    auto tot = interp.globals()->get("total");
    NV_ASSERT_TRUE(tot.has_value() && tot->is_int() && tot->as_int() == 15);

    auto ch = interp.globals()->get("chunks");
    NV_ASSERT_TRUE(ch.has_value() && ch->is_array() && ch->as_array()->size() == 3);

    auto ver = interp.globals()->get("sys_ver");
    NV_ASSERT_TRUE(ver.has_value() && ver->is_string() && ver->as_string() == "0.1.0");
}

NV_TEST(Modules, FileBasedCustomModuleResolution) {
    // Create temporary custom module file
    std::string mod_filename = "tests/my_test_calc.nv";
    {
        std::ofstream f(mod_filename);
        f << "export fn multiply(a, b):\n"
          << "    return a * b\n\n"
          << "export let SECRET_KEY = 42\n";
    }

    SourceManager sm;
    DiagnosticEngine diag(sm, false);
    Interpreter interp(diag);

    std::string src = "import \"./tests/my_test_calc.nv\" as calc\n"
                      "from \"./tests/my_test_calc.nv\" import multiply\n"
                      "\n"
                      "let res1 = calc.multiply(6, 7)\n"
                      "let res2 = multiply(3, 4)\n"
                      "let secret = calc.SECRET_KEY\n";

    NV_ASSERT_TRUE(eval_script(src, diag, interp));

    auto r1 = interp.globals()->get("res1");
    NV_ASSERT_TRUE(r1.has_value() && r1->is_int() && r1->as_int() == 42);

    auto r2 = interp.globals()->get("res2");
    NV_ASSERT_TRUE(r2.has_value() && r2->is_int() && r2->as_int() == 12);

    auto sec = interp.globals()->get("secret");
    NV_ASSERT_TRUE(sec.has_value() && sec->is_int() && sec->as_int() == 42);
}

NV_TEST(Modules, SecurityAndResolutionErrors) {
    SourceManager sm;
    DiagnosticEngine diag(sm, false);
    Interpreter interp(diag);

    // 1. Missing module error
    std::string src_missing = "import definitely_non_existent_module_xyz\n";
    NV_ASSERT_FALSE(eval_script(src_missing, diag, interp));
    NV_ASSERT_TRUE(diag.has_errors());

    // 2. Missing export symbol error
    SourceManager sm2;
    DiagnosticEngine diag2(sm2, false);
    Interpreter interp2(diag2);
    std::string src_sym = "from math import non_existent_symbol_123\n";
    NV_ASSERT_FALSE(eval_script(src_sym, diag2, interp2));
    NV_ASSERT_TRUE(diag2.has_errors());
}
