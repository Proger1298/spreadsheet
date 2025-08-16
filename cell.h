#pragma once

#include "common.h"
#include "formula.h"

#include <functional>
#include <unordered_set>

class Sheet;

class Cell : public CellInterface {
public:
    Cell(Sheet& sheet);
    ~Cell();

    void Set(std::string text);
    void Clear();

    Value GetValue() const override;
    std::string GetText() const override;
    std::vector<Position> GetReferencedCells() const override;

    bool IsReferenced() const;

private:
    class Impl;
    class EmptyImpl;
    class TextImpl;
    class FormulaImpl;

    // функция проверяет создаст ли новая ячейка циклическую зависимость
    bool CheckCircularDependencyToArise(const Impl& impl_to_include) const;

    // функция рекурсивно инвалидирует кэш во всех ячейках, которые зависят(ссылаются) от(на) текущей(ую)
    void InvalidateCacheRecursively();

    // Обновляет out_refs и in_refs
    void UpdateDependencies(const Impl& impl_to_include);

    Sheet& sheet_; // ссылка на таблицу
    std::unique_ptr<Impl> impl_; // умный указатель на текущее состояние ячейки
    // множество ячеек, которые ссылаются на текущую (для корректной инвалидации кэша и поиска циклических зависимостей)
    std::unordered_set<Cell*> in_refs_;
    std::unordered_set<Cell*> out_refs_; 
};
