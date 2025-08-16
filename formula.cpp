#include "formula.h"

#include "FormulaAST.h"

#include <algorithm>
#include <cassert>
#include <cctype>
#include <sstream>

using namespace std::literals;

FormulaError::FormulaError(Category category) 
: category_(std::move(category))
{

}

FormulaError::Category FormulaError::GetCategory() const {
    return category_;
}

bool FormulaError::operator==(FormulaError rhs) const {
    return category_ == rhs.category_;
}

std::string_view FormulaError::ToString() const {
    switch (category_) {
        case Category::Ref:
            return "#REF!"sv;
        case Category::Value:
            return "#VALUE!"sv;
        case Category::Arithmetic:
            return "#ARITHM!"sv;
        default:
            return "";
    }
}

std::ostream& operator<<(std::ostream& output, FormulaError fe) {
    output << fe.ToString();
    return output;
}

namespace {
class Formula : public FormulaInterface {
public:
// Реализуйте следующие методы:
    explicit Formula(std::string expression) try
    : ast_(ParseFormulaAST(std::move(expression))) {
    } catch (const std::exception& exc) {
        throw FormulaException("Error parsing formula");
    }

    Value Evaluate(const SheetInterface& sheet) const override {
        const FormulaFunction formula_function = [&sheet](const Position pos)->double {
            if (!pos.IsValid()) {
                throw FormulaError{FormulaError::Category::Ref};
            }

            const CellInterface* cell = sheet.GetCell(pos);
            // Если пользователь в формуле указал на пустую ячейку, то ёё надо интерпретировать как 0
            if (!cell || cell->GetText().empty()) {
                return 0.0;
            }
            const CellInterface::Value cell_value = cell->GetValue();
            // Если пользователь указал на ячейку с текстом, то нужно попытаться
            // Интерпретировать её как число
            if (std::holds_alternative<std::string>(cell_value)) {
                std::string string_value = std::get<std::string>(cell_value);
                size_t pos;
                double value;
                try {
                    value = std::stod(string_value, &pos);
                } catch (...) {
                    throw FormulaError{FormulaError::Category::Value};
                }
                if (pos < string_value.length()) {
                    throw FormulaError{FormulaError::Category::Value};
                }
                return value;
            }
            // Если пользователь указал на ячейку с числом, то возвращаем значение
            return std::get<double>(cell_value);
        };

        try {
            return ast_.Execute(formula_function);
        } catch (const FormulaError& error) {
            return error;
        }
    }

    std::string GetExpression() const override {
        std::ostringstream out;
        ast_.PrintFormula(out);
        return out.str();
    }

    std::vector<Position> GetReferencedCells() const override {
        std::vector<Position> positions;

        for (const Position& pos : ast_.GetCells()) {
            if (pos.IsValid()) {
                positions.push_back(pos);
            }
        }

        // Сортируем вектор
        std::sort(positions.begin(), positions.end());

        // Удаляем дубликаты
        auto last = std::unique(positions.begin(), positions.end());

        // Оставляем только уникальные элементы, изменяя размер вектора
        positions.erase(last, positions.end());

        return positions;
    }

private:
    FormulaAST ast_;
};

}  // namespace

std::unique_ptr<FormulaInterface> ParseFormula(std::string expression) {
    return std::make_unique<Formula>(std::move(expression));
}