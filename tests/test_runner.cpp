#include "test_runner.hpp"

int main() {
    return nextviper::test::TestRegistry::instance().run_all();
}
