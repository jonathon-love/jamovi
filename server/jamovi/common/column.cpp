//
// Copyright (C) 2016 Jonathon Love
//

#include "column.h"

#include <stdexcept>
#include <climits>
#include <sstream>
#include <cstring>

#include "dataset.h"

using namespace std;

Column::Column(DataSet *parent, MemoryMap *mm, ColumnStruct *rel)
{
    _parent = parent;
    _mm = mm;
    _rel = rel;
}

int Column::id() const {
    return struc()->id;
}

const char *Column::name() const
{
    return _mm->resolve(struc()->name);
}

const char *Column::importName() const
{
    return _mm->resolve(struc()->importName);
}

ColumnType::Type Column::columnType() const
{
    return (ColumnType::Type) struc()->columnType;
}

DataType::Type Column::dataType() const
{
    return (DataType::Type) struc()->dataType;
}

MeasureType::Type Column::measureType() const
{
    return (MeasureType::Type) struc()->measureType;
}

bool Column::autoMeasure() const {
    return struc()->autoMeasure;
}

int Column::rowCount() const {
    return struc()->rowCount;
}

int Column::dps() const
{
    return struc()->dps;
}

bool Column::active() const
{
    return struc()->active;
}

bool Column::trimLevels() const
{
    return struc()->trimLevels;
}

const char *Column::formula() const
{
    if (struc()->formula == NULL)
        return NULL;
    return _mm->resolve(struc()->formula);
}

const char *Column::formulaMessage() const
{
    if (struc()->formulaMessage == NULL)
        return NULL;
    return _mm->resolve(struc()->formulaMessage);
}

ColumnStruct *Column::struc() const
{
    return _mm->resolve(_rel);
}

int Column::levelCount() const
{
    return struc()->levelsUsed;
}

int Column::levelCountExFiltered() const
{
    int count = 0;
    ColumnStruct *s = struc();
    Level *levels = _mm->resolve(s->levels);
    for (int i = 0; i < s->levelsUsed; i++)
        count += (levels[i].countExFiltered > 0) ? 1 : 0;
    return count;
}

const char* Column::raws(int index)
{
    const char *value = cellAt<char*>(index);
    if (value == NULL)
        return "";
    else
        return _mm->resolve(value);
}

const vector<LevelData> Column::levels() const
{
    vector<LevelData> m;

    ColumnStruct *s = struc();
    Level *levels = _mm->resolve(s->levels);

    for (int i = 0; i < s->levelsUsed; i++)
    {
        Level &l = levels[i];
        bool filtered = l.countExFiltered == 0;

        if (dataType() == DataType::TEXT)
        {
            char *value = _mm->resolve(l.importValue);
            char *label = _mm->resolve(l.label);
            m.push_back(LevelData(value, label, filtered));
        }
        else
        {
            int value   = l.value;
            char *label = _mm->resolve(l.label);
            m.push_back(LevelData(value, label, filtered));
        }
    }

    return m;
}

bool Column::hasUnusedLevels() const
{
    ColumnStruct *s = struc();
    Level *levels = _mm->resolve(s->levels);

    for (int i = 0; i < s->levelsUsed; i++)
    {
        Level &l = levels[i];
        if (l.countExFiltered == 0)
            return true;
    }

    return false;
}

const char *Column::getLabel(const char* value) const
{
    if (value[0] == '\0')
        return value;

    ColumnStruct *s = struc();
    Level *levels = _mm->resolve(s->levels);

    for (int i = 0; i < s->levelsUsed; i++)
    {
        Level &l = levels[i];
        const char *importValue = _mm->resolve(l.importValue);
        if (strcmp(importValue, value) == 0)
            return _mm->resolve(l.label);
    }

    stringstream ss;
    ss << "level " << value << " not found in " << this->name();
    throw runtime_error(ss.str());
}

const char *Column::getLabel(int value) const
{
    if (value == INT_MIN)
        return "";

    ColumnStruct *s = struc();
    Level *levels = _mm->resolve(s->levels);

    for (int i = 0; i < s->levelsUsed; i++)
    {
        Level &l = levels[i];
        if (l.value == value)
            return _mm->resolve(l.label);
    }

    stringstream ss;
    ss << "level " << value << " not found in " << this->name();
    throw runtime_error(ss.str());
}

const char *Column::getImportValue(int value) const
{
    if (value == INT_MIN)
        return "";

    ColumnStruct *s = struc();
    Level *levels = _mm->resolve(s->levels);

    for (int i = 0; i < s->levelsUsed; i++)
    {
        Level &l = levels[i];
        if (l.value == value)
            return _mm->resolve(l.importValue);
    }

    stringstream ss;
    ss << "level " << value << " not found";
    throw runtime_error(ss.str());
}

int Column::valueForLabel(const char *label) const
{
    ColumnStruct *s = struc();
    Level *levels = _mm->resolve(s->levels);

    for (int i = 0; i < s->levelsUsed; i++)
    {
        Level &level = levels[i];
        const char *l = _mm->resolve(level.label);
        if (strcmp(l, label) == 0)
        {
            return level.value;
        }
        else
        {
            const char *iv = _mm->resolve(level.importValue);
            if (strcmp(iv, label) == 0)
                return level.value;
        }
    }

    stringstream ss;
    ss << "level '" << label << "' not found";
    throw runtime_error(ss.str());
}

bool Column::hasLevels() const
{
    return measureType() != MeasureType::CONTINUOUS &&
           measureType() != MeasureType::ID;
}

bool Column::hasLevel(const char *label) const
{
    ColumnStruct *s = struc();
    Level *levels = _mm->resolve(s->levels);

    for (int i = 0; i < s->levelsUsed; i++)
    {
        Level &level = levels[i];
        const char *l = _mm->resolve(level.label);
        if (strcmp(l, label) == 0)
        {
            return true;
        }
        else
        {
            const char *iv = _mm->resolve(level.importValue);
            if (strcmp(iv, label) == 0)
                return true;
        }
    }

    return false;
}

bool Column::hasLevel(int value) const
{
    return rawLevel(value) != NULL;
}

Level *Column::rawLevel(int value) const
{
    ColumnStruct *s = struc();
    Level *levels = _mm->resolve(s->levels);

    for (int i = 0; i < s->levelsUsed; i++)
    {
        Level &level = levels[i];
        if (level.value == value)
            return &level;
    }

    return NULL;
}
