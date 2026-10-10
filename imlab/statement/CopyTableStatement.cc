//---------------------------------------------------------------------------
#include "imlab/statement/CopyTableStatement.h"

#include "CodeGenHelper.h"
#include "imlab/infra/Types.h"

#include <fstream>
#include <sstream>
#include <dlfcn.h>
#include <iostream>

//---------------------------------------------------------------------------
using namespace std;
//---------------------------------------------------------------------------
namespace imlab::statement {
//---------------------------------------------------------------------------
CopyTableStatement::CopyTableStatement(string table, string file, string delimiter) : Statement(Statement::Type::CopyTableStatementType), table_(std::move(table)), file_(std::move(file)), delimiter_(std::move(delimiter)) {}
//---------------------------------------------------------------------------

void CopyTableStatement::run(Database& db) {
    auto &targetTable = db.getTable(table_);
    const auto &columns = targetTable.getSchema().columns;

    std::ostringstream code;
    code << "#include <iostream>\n";
    code << "#include <fstream>\n";
    code << "#include <sstream>\n";
    code << "#include \"imlab/Database.h\"\n";
    code << "#include \"imlab/infra/Types.h\"\n";
    code << "#include <stdexcept>\n";
    code << "#include <vector>\n";
    code << "extern \"C\" void execute(imlab::Database& db) {\n";
    code << "    auto &targetTable = db.getTable(\"" << table_ << "\");\n";
    code << "    std::ifstream infile(\"" << file_ << "\");\n";
    code << "    if (!infile.is_open()) throw std::runtime_error(\"Could not open file '" << file_ << "'\");\n";
    code << "    std::string line;\n";
    code << "    while (std::getline(infile, line)) {\n";
    code << "        std::istringstream lineStream(line);\n";
    code << "        std::string strVal;\n";
    code << "        std::vector<std::string> rowStrValues;\n";
    code << "        while (std::getline(lineStream, strVal, '" << delimiter_[0] << "')) {\n";
    code << "            rowStrValues.push_back(strVal);\n";
    code << "        }\n";

    code << "        std::vector<imlab::Value> rowTypedValues;\n";

    for (size_t i = 0; i < columns.size(); i++) {

        const auto &col = columns[i];
        switch (col.type.getClass()) {
            case imlab::Type::kInteger:
                code << "          rowTypedValues.emplace_back(imlab::Value(imlab::Integer::castString(rowStrValues[" << i << "].c_str(), rowStrValues[" << i << "].size())));\n";
                break;

            case imlab::Type::kTimestamp:
                code << "          rowTypedValues.emplace_back(imlab::Value(imlab::Timestamp::castString(rowStrValues[" << i << "].c_str(), rowStrValues[" << i << "].size())));\n";
                break;

            case imlab::Type::kNumeric: {
                code << "          rowTypedValues.emplace_back(imlab::Value(imlab::Numeric<" << col.type.getPrecision() << "," << col.type.getScale() << ">::castString(rowStrValues[" << i << "].c_str(), rowStrValues[" << i << "].size()).getRaw(), imlab::Type::Numeric(" << col.type.getPrecision() << "," << col.type.getScale() << ")));\n";
                break;
            }

            case imlab::Type::kChar: {
                code << "          {\n";
                code << "            auto field = imlab::Char<" << col.type.getLength() << ">::castString(rowStrValues[" << i << "].c_str(), rowStrValues[" << i << "].size());\n";
                code << "            rowTypedValues.emplace_back(imlab::Value(std::string(field.begin(), field.end()), imlab::Type::Char(" << col.type.getLength() << ")));\n";
                code << "          }\n";
                break;
            }

            case imlab::Type::kVarchar: {
                code << "          {\n";
                code << "            auto field = imlab::Varchar<" << col.type.getLength() << ">::castString(rowStrValues[" << i << "].c_str(), rowStrValues[" << i << "].size());\n";
                code << "            rowTypedValues.emplace_back(imlab::Value(std::string(field.begin(), field.end()), imlab::Type::Varchar(" << col.type.getLength() << ")));\n";
                code << "          }\n";
                break;
            }
        }
        code << "\n";
    }

    code << "        targetTable.insertRow(rowTypedValues);\n";
    code << "    }\n";
    code << "}\n";


    CodeGenHelper helper(code.str());
    using Function = void (*)(Database&);
    auto* func = reinterpret_cast<Function>(helper.getExecute().func);
    func(db);
}

//---------------------------------------------------------------------------
} // namespace imlab::statement
//---------------------------------------------------------------------------
