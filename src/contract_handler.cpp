#include <iostream>
#include <fstream>
#include <contracts>

extern "C++" void handle_contract_violation(const std::contracts::contract_violation& violation) noexcept {
    std::cerr << "Contract violated!" << std::endl;
    std::cerr << "File: " << violation.location().file_name() << "\n";
    std::cerr << "Function name: " << violation.location().function_name() << "\n";
    std::cerr << "Line number: " << violation.location().line() << "\n";
    std::cerr << "Contract expression: " << violation.comment() << "\n";
    std::abort();
}
