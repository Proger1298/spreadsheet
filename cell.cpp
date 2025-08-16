#include "cell.h"
#include "sheet.h"

#include <cassert>
#include <iostream>
#include <stack>
#include <string>
#include <optional>

using namespace std::literals;

class Cell::Impl {
public:

    virtual Value GetValue() const = 0;
    virtual std::string GetText() const = 0;

    virtual bool IsCacheValid() const { 
        return true; 
    }

    virtual void InvalidateCache() {}

    virtual std::vector<Position> GetReferencedCells() const { 
        return {}; 
    }

};

class Cell::EmptyImpl : public Impl {
public:

    Value GetValue() const override {
        return "";
    }

    std::string GetText() const override {
        return "";
    }

};

class Cell::TextImpl : public Impl {
public:
    TextImpl(std::string_view text) 
    :text_(std::move(text))
    {

    }

    Value GetValue() const override {
        if (text_.at(0) == ESCAPE_SIGN) {
            return text_.substr(1);
        }
        return text_;
    }

    std::string GetText() const override {
        return text_;
    }

private:
    std::string text_;
};

class Cell::FormulaImpl : public Impl {
public:
    FormulaImpl(std::string_view text, const SheetInterface& sheet) 
    : formula_ptr_(ParseFormula(std::string(text))), sheet_in_formula_(sheet)
    {

    }

    Value GetValue() const override {
        if (IsCacheValid()) {
            return FormulaValueToCellValue(cache_.value());
        }
        cache_ = formula_ptr_->Evaluate(sheet_in_formula_);
        return FormulaValueToCellValue(cache_.value());
    }

    std::string GetText() const override {
        return FORMULA_SIGN + formula_ptr_->GetExpression();
    }

    bool IsCacheValid() const override { 
        return cache_.has_value(); 
    }

    void InvalidateCache() override{
        cache_.reset();
    }

    std::vector<Position> GetReferencedCells() const override {
        return formula_ptr_->GetReferencedCells();
    }

private:
    std::unique_ptr<FormulaInterface> formula_ptr_;
    const SheetInterface& sheet_in_formula_;
    mutable std::optional<FormulaInterface::Value> cache_;

    CellInterface::Value FormulaValueToCellValue(FormulaInterface::Value value) const {
        if (std::holds_alternative<double>(value)) {
            return std::get<double>(value);
        }
        return std::get<FormulaError>(value);
    }
};

Cell::Cell(Sheet& sheet) 
: sheet_(sheet), impl_(std::make_unique<EmptyImpl>())
{

}

Cell::~Cell() {}

bool Cell::CheckCircularDependencyToArise(const Impl& impl_to_include) const {

    std::unordered_set<const CellInterface*> refernced_cells;
    for (const Position& pos : impl_to_include.GetReferencedCells()) {
        refernced_cells.insert(sheet_.GetCell(pos));
    }

    // Реализуем DFS с поиском циклов
    std::unordered_set<const Cell*> visited;
    std::stack<const Cell*> not_visited;

    not_visited.push(this);
    while (!not_visited.empty()) {
        const Cell* cur_cell = dynamic_cast<const Cell*>(not_visited.top());
        visited.insert(cur_cell);
        not_visited.pop();

        if (refernced_cells.find(cur_cell) != refernced_cells.end()) {
            return true;
        }

        for (const Cell* in_ref : cur_cell->in_refs_) {
            if (visited.find(in_ref) == visited.end()) {
                not_visited.push(in_ref);
            }
        }
    }

    return false;
}

void Cell::InvalidateCacheRecursively() {
    impl_->InvalidateCache();
    for (Cell* in_ref : in_refs_) {
        if (impl_->IsCacheValid()) {
            in_ref->InvalidateCacheRecursively();
        }
    }
}

void Cell::UpdateDependencies(const Impl& impl_to_include) {
    for (Cell* out_ref : out_refs_) {
        out_ref->in_refs_.erase(this);
    }
    out_refs_.clear();

    for (const Position& pos: impl_to_include.GetReferencedCells()) {
        Cell* out_ref = dynamic_cast<Cell*>(sheet_.GetCell(pos));
        if (!out_ref) {
            sheet_.SetCell(pos, std::string());
            out_ref = dynamic_cast<Cell*>(sheet_.GetCell(pos));
        }
        out_refs_.insert(out_ref);
        out_ref->in_refs_.insert(this);
    }
}

void Cell::Set(std::string text) {
    std::unique_ptr<Impl> impl_to_include;

    if (text.size() == 0) {
        impl_to_include = std::make_unique<EmptyImpl>();
    } else if (text.at(0) == FORMULA_SIGN && text.size() > 1) {
        impl_to_include = std::make_unique<FormulaImpl>(std::move(text.substr(1)), sheet_);
    } else {
        impl_to_include = std::make_unique<TextImpl>(std::move(text));
    }

    if (CheckCircularDependencyToArise(*impl_to_include)) {
        throw CircularDependencyException("New Cell creates circular dependency!");
    }

    UpdateDependencies(*impl_to_include);

    impl_ = std::move(impl_to_include);

    InvalidateCacheRecursively();
}

void Cell::Clear() {
    Set(std::string());
}

Cell::Value Cell::GetValue() const {
    return impl_->GetValue();
}

std::string Cell::GetText() const {
    return impl_->GetText();
}

std::vector<Position> Cell::GetReferencedCells() const {
    return impl_->GetReferencedCells();
}

bool Cell::IsReferenced() const {
    return !in_refs_.empty();
}