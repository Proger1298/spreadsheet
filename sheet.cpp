#include "sheet.h"

#include "cell.h"
#include "common.h"

#include <algorithm>
#include <functional>
#include <iostream>
#include <optional>

using namespace std::literals;

Sheet::~Sheet() {}

void Sheet::SetCell(Position pos, std::string text) {
    // Проверка позиции на валидность
    if (!pos.IsValid()) {
        throw InvalidPositionException("The position is not valid!");
    }

    // Если в ячейку с такой позицией ещё ничего не записывали
    // То создаем пустую ячейку
    if (sheet_.find(pos) == sheet_.end()) {
        sheet_.emplace(pos, std::make_unique<Cell>(*this));
    }

    // Устанавливаем в ячейке значение при помощи Cell::Set
    sheet_.at(pos)->Set(std::move(text));
}

// Метод, содержащий общую логику получения ячейки
CellInterface* Sheet::GetCellInternal(Position pos) const {
    if (!pos.IsValid()) {
        throw InvalidPositionException("The position is not valid!");
    }

    const auto it = sheet_.find(pos);

    // Возвращаем указатель на Cell или nullptr
    return (it != sheet_.end()) ? it->second.get() : nullptr;
}

const CellInterface* Sheet::GetCell(Position pos) const {
    return GetCellInternal(pos);
}

CellInterface* Sheet::GetCell(Position pos) {
    return const_cast<CellInterface*>(GetCellInternal(pos));
}

void Sheet::ClearCell(Position pos) {
    if (!pos.IsValid()) {
        throw InvalidPositionException("The position is not valid!");
    }

    auto it = sheet_.find(pos);
    if (it != sheet_.end()) {
        // Если ячейка существует, вызываем метод Clear
        it->second->Clear();
        if(!it->second->IsReferenced()) {
            sheet_.erase(it);
        }
    }
}

Size Sheet::GetPrintableSize() const {
    Size result;
    for (auto it = sheet_.begin(); it != sheet_.end(); ++it) {
        if (!it->second->GetText().empty()) {
            result.rows = std::max(result.rows, it->first.row + 1);
            result.cols = std::max(result.cols, it->first.col + 1);
        }
    }

    return result;
}

void Sheet::PrintValues(std::ostream& output) const {
    const Size size = GetPrintableSize();
    for (int row = 0; row < size.rows; ++row) {
        for (int col = 0; col < size.cols; ++col) {
            if (col > 0) {
                output << "\t";
            }
            const auto& it = sheet_.find({ row, col });
            if (it != sheet_.end()) {
                std::visit([&](const auto value) { output << value; }, it->second->GetValue());
            }
        }
        output << "\n";
    }
}

void Sheet::PrintTexts(std::ostream& output) const {
    const Size size = GetPrintableSize();
    for (int row = 0; row < size.rows; ++row) {
        for (int col = 0; col < size.cols; ++col) {
            if (col > 0) {
                output << "\t";
            }
            const auto& it = sheet_.find({ row, col });
            if (it != sheet_.end()) {
                output << it->second->GetText();
            }
        }
        output << "\n";
    }
}

std::unique_ptr<SheetInterface> CreateSheet() {
    return std::make_unique<Sheet>();
}