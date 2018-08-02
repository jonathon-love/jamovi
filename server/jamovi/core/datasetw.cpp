//
// Copyright (C) 2016 Jonathon Love
//

#include "datasetw.h"

#include <cstring>
#include <climits>
#include <stdexcept>
#include <cmath>

using namespace std;

DataSetW *DataSetW::create(MemoryMapW *mm)
{
    DataSetW *ds = new DataSetW(mm);
    DataSetStruct *rel = mm->allocateBase<DataSetStruct>();
    ds->_rel = rel;

    ColumnStruct **columns = mm->allocateBase<ColumnStruct *>(65536);
    DataSetStruct *dss = mm->resolve(rel);

    dss->columns = columns;
    dss->capacity = 65536;  // "ought to be enough for anybody"
    dss->columnCount = 0;
    dss->nextColumnId = 0;
    dss->scratch = NULL;

    return ds;
}

DataSetW *DataSetW::retrieve(MemoryMapW *mm)
{
    DataSetW *ds = new DataSetW(mm);

    ds->_rel = mm->root<DataSetStruct>();

    return ds;
}

DataSetW::DataSetW(MemoryMapW *mm)
    : DataSet(mm)
{
    _mm = mm;
    _edited = false;
    _blank = false;
}

void DataSetW::setEdited(bool edited)
{
    _edited = edited;
}

bool DataSetW::isEdited() const
{
    return _edited;
}

void DataSetW::setBlank(bool blank)
{
    _blank = blank;
}

bool DataSetW::isBlank() const
{
    return _blank;
}

ColumnW DataSetW::operator[](const char *name)
{
    for (int i = 0; i < columnCount(); i++)
    {
        ColumnW column = (*this)[i];
        if (strcmp(column.name(), name) == 0)
            return column;
    }

    throw runtime_error("no such column");
}

ColumnW DataSetW::operator[](int index)
{
    DataSetStruct *dss = _mm->resolve<DataSetStruct>(_rel);

    if (index >= dss->columnCount)
        throw runtime_error("index out of bounds");

    ColumnStruct **columns = _mm->resolve<ColumnStruct*>(dss->columns);
    ColumnStruct *rel = columns[index];

    return ColumnW(this, _mm, rel);
}

ColumnW DataSetW::getColumnById(int id)
{
    for (int i = 0; i < columnCount(); i++)
    {
        ColumnW column = (*this)[i];
        if (column.id() == id)
            return column;
    }

    throw runtime_error("no such column");
}

ColumnW DataSetW::insertColumn(int index, const char *name, const char *importName)
{
    appendColumn(name, importName);

    int nCols = columnCount();
    ColumnStruct **columns = _mm->resolve(struc()->columns);
    ColumnStruct *toMove = columns[nCols - 1];
    int nMove = nCols - index - 1;
    size_t szMove = nMove * sizeof(ColumnStruct*);

    memmove(&columns[index+1], &columns[index], szMove);
    columns[index] = toMove;

    ColumnW wrapper = ColumnW(this, _mm, toMove);
    wrapper.setRowCount<int>(rowCount());

    return wrapper;
}

ColumnW DataSetW::appendColumn(const char *name, const char *importName)
{
    DataSetStruct *dss = struc();
    int columnId = dss->nextColumnId;
    dss->nextColumnId += 1;

    int columnCount = struc()->columnCount;

    if (columnCount >= struc()->capacity)
        throw runtime_error("Too many columns");

    int n = strlen(name);
    char *chars = _mm->allocate<char>(n + 1);  // +1 for null terminator
    memcpy(chars, name, n + 1);
    chars = _mm->base<char>(chars);

    int n2 = strlen(importName);
    char *chars2 = _mm->allocate<char>(n2 + 1);  // +1 for null terminator
    memcpy(chars2, importName, n2 + 1);
    chars2 = _mm->base<char>(chars2);

    ColumnStruct **columns;
    ColumnStruct *column;

    column = _mm->allocateBase<ColumnStruct>();
    dss = struc();
    columns = _mm->resolve(dss->columns);
    columns[columnCount] = column;

    column = strucC(columnCount);

    initColumn(column);

    column->name = chars;
    column->importName = chars2;
    column->id = columnId;

    struc()->columnCount++;

    ColumnW wrapper = ColumnW(this, _mm, _mm->base<ColumnStruct>(column));
    wrapper.setRowCount<int>(rowCount());

    return wrapper;
}

void DataSetW::initColumn(ColumnStruct *&columnp)
{
    ColumnStruct *column = columnp;

    column->name = NULL;
    column->importName = NULL;
    column->id = -1;

    column->columnType = ColumnType::DATA;
    column->dataType = DataType::INTEGER;
    column->measureType = MeasureType::NOMINAL;
    column->autoMeasure = false;
    column->dps = 0;
    column->active = true;
    column->trimLevels = true;
    column->changes = 0;

    column->formula = NULL;
    column->formulaCapacity = 0;
    column->formulaMessage = NULL;
    column->formulaMessageCapacity = 0;

    column->rowCount = 0;
    column->blocksUsed = 0;
    column->blockCapacity = 1024;
    column->levelsUsed = 0;
    column->levelsCapacity = 0;

    ColumnStruct *rel = _mm->base(column);
    Block** blocks = _mm->allocateBase<Block*>(column->blockCapacity);

    column = _mm->resolve(rel);
    column->blocks = blocks;

    columnp = column;
}

void DataSetW::setRowCount(size_t count)
{
    DataSetStruct *dss = _mm->resolve<DataSetStruct>(_rel);
    ColumnStruct **columns = _mm->resolve<ColumnStruct*>(dss->columns);

    for (int i = 0; i < dss->columnCount; i++)
    {
        ColumnStruct *c = columns[i];
        ColumnW column(this, _mm, c);

        if (column.dataType() == DataType::DECIMAL)
            column.setRowCount<double>(count);
        else if (column.dataType() == DataType::TEXT && column.measureType() == MeasureType::ID)
            column.setRowCount<char*>(count);
        else
            column.setRowCount<int>(count);

        dss     = _mm->resolve(_rel);
        columns = _mm->resolve(dss->columns);
    }

    dss->rowCount = count;
}

void DataSetW::appendRows(int n)
{
    DataSetStruct *dss = _mm->resolve<DataSetStruct>(_rel);
    ColumnStruct **columns = _mm->resolve<ColumnStruct*>(dss->columns);

    for (int i = 0; i < dss->columnCount; i++)
    {
        ColumnStruct *c = columns[i];
        ColumnW column(this, _mm, c);

        if (column.dataType() == DataType::DECIMAL)
        {
            for (int j = 0; j < n; j++)
                column.append<double>(NAN);
        }
        else
        {
            for (int j = 0; j < n; j++)
                column.append<int>(INT_MIN);
        }

        dss     = _mm->resolve(_rel);
        columns = _mm->resolve(dss->columns);
    }

    dss->rowCount += n;
}

void DataSetW::insertRows(int insStart, int insEnd)
{
    int insCount = insEnd - insStart + 1;
    int finalCount = rowCount() + insCount;

    for (int i = 0; i < columnCount(); i++)
        (*this)[i].insertRows(insStart, insEnd);

    setRowCount(finalCount);
}

void DataSetW::deleteRows(int delStart, int delEnd)
{
    DataSetStruct *dss = _mm->resolve<DataSetStruct>(_rel);
    ColumnStruct **columns = _mm->resolve<ColumnStruct*>(dss->columns);

    int delCount = delEnd - delStart + 1;
    int startCount = dss->rowCount;
    int finalCount = dss->rowCount - delCount;

    for (int i = 0; i < dss->columnCount; i++)
    {
        ColumnStruct *c = columns[i];
        ColumnW column(this, _mm, c);

        if (column.dataType() == DataType::DECIMAL)
        {
            for (int j = delStart; j < finalCount; j++)
            {
                int from = j + delCount;
                int to = j;
                double value = column.raw<double>(from);
                column.setDValue(to, value);
            }

            column.setRowCount<double>(finalCount);
        }
        else
        {
            for (int j = delStart; j < finalCount; j++)
            {
                int from = j + delCount;
                int to = j;
                column.setIValue(to, INT_MIN);
                int value = column.raw<int>(from);
                column.setIValue(to, value);
            }

            column.setRowCount<int>(finalCount);
            column.updateLevelCounts();
        }
    }

    dss->rowCount = finalCount;
}

void DataSetW::deleteColumns(int delStart, int delEnd)
{
    // at the moment this just leaks memory, but we
    // should store the deleted columns for re-use

    DataSetStruct *dss = _mm->resolve<DataSetStruct>(_rel);

    int delCount = delEnd - delStart + 1;
    int startCount = dss->columnCount;
    int finalCount = dss->columnCount - delCount;
    int nToMove = startCount - delStart - delCount;

    ColumnStruct **columns = _mm->resolve(struc()->columns);
    memmove(&columns[delStart], &columns[delEnd+1], nToMove * sizeof(ColumnStruct*));

    dss->columnCount -= delCount;
}

ColumnW DataSetW::swapWithScratchColumn(ColumnW &column)
{
    ColumnStruct *scratch = struc()->scratch;

    if (scratch == NULL)
    {
        scratch = _mm->allocate<ColumnStruct>();
        initColumn(scratch);
        scratch = _mm->base(scratch);
    }

    ColumnStruct *old = _mm->resolve(column._rel);

    scratch = _mm->resolve(scratch);

    /*int rowCount = scratch->rowCount;
    int capacity = scratch->capacity;
    Block **blocks = scratch->blocks;
    int blocksUsed = scratch->blocksUsed;
    int blockCapacity = scratch->blockCapacity;
    Level *levels = scratch->levels;
    int levelsUsed = scratch->levelsUsed;
    int levelsCapacity = scratch->levelsCapacity;

    *scratch = *old;
    scratch->rowCount = rowCount;
    scratch->capacity = capacity;
    scratch->blocks = blocks;
    scratch->blocksUsed = blocksUsed;
    scratch->blockCapacity = blockCapacity;
    scratch->levels = levels;
    scratch->levelsUsed = levelsUsed;
    scratch->levelsCapacity = levelsCapacity;*/

    scratch->name = old->name;
    scratch->importName = old->importName;
    scratch->columnType = old->columnType;
    scratch->autoMeasure = old->autoMeasure;
    scratch->active = old->active;
    scratch->trimLevels = old->trimLevels;
    scratch->changes = old->changes;

    scratch->formula = old->formula;
    scratch->formulaCapacity = old->formulaCapacity;
    scratch->formulaMessage = old->formulaMessage;
    scratch->formulaMessageCapacity = old->formulaMessageCapacity;

    scratch = _mm->base(scratch);

    ColumnStruct *tmp = column._rel;
    column._rel = scratch;
    struc()->scratch = tmp;

    ColumnStruct **columns = _mm->resolve(struc()->columns);
    for (int i = 0; i < columnCount(); i++)
    {
        if (columns[i] == tmp)
        {
            columns[i] = column._rel;
            break;
        }
    }

    return ColumnW(this, _mm, tmp);
}

void DataSetW::discardScratchColumn(int id)
{
    ColumnStruct *scratch = struc()->scratch;
    if (scratch == NULL)
        return;

    scratch = _mm->resolve(scratch);
    if (scratch->id == id)
        scratch->id = -1;
}
