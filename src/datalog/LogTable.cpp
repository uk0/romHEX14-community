#include "LogTable.h"

namespace datalog {

int LogTable::findColumn(const QString &name) const
{
    for (int i = 0; i < columns.size(); ++i)
        if (columns[i].name == name) return i;
    return -1;
}

} // namespace datalog
