// ---------------------------------------------------------------------------
// IMLAB
// ---------------------------------------------------------------------------

#include <iostream>
#include <sstream>
#include <imlab/Database.h>
#include <imlab/parser/gen/parser.h>
#include <imlab/semana/SemanticAnalysis.h>
#include <imlab/statement/CopyTableStatement.h>
#include <imlab/statement/CreateTableStatement.h>

using namespace imlab;

int main() {
    Database db;
    std::string line;
    std::string fullInput;

    while (true) {

        // Read one line from standard input
        std::cout << "> ";
        if (!std::getline(std::cin, line)) {
            break;
        }
        if (line.empty()) {
            continue;
        }

        fullInput += line + "\n";


        if (line.find(';') != std::string::npos) {
            // Run the parser on the full statement, including earlier lines.
            std::istringstream inputStream(fullInput);
            parser::ParseContext parseContext;
            std::vector<std::unique_ptr<parser::AST>> astList;

            try {
                astList = parseContext.Parse(inputStream);
            } catch (std::exception& e) {
                std::cout << e.what() << '\n';
                fullInput.clear();
                continue;
            }

            // For each statement of the AST
            for (const auto& ast : astList) {
                // Run the semantic analysis on the parsed AST and return the corresponding Statement
                semana::SemanticAnalysis semanticAnalysis;
                const std::unique_ptr<statement::Statement> statement = semanticAnalysis.buildStatement(*ast, db);

                // Execute the Statement returned by the semantic analysis
                statement->run(db);
            }
            fullInput.clear();
        }
    }
    return 0;
}