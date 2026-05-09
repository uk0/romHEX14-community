#pragma once
#include <QTreeWidget>
#include <QVector>

namespace datalog {

class LogTable;
enum class EcuFamily : int;

class ChannelTreeWidget : public QTreeWidget {
    Q_OBJECT
public:
    explicit ChannelTreeWidget(QWidget *parent = nullptr);
    void setTable(const LogTable *t, EcuFamily family);

signals:
    void selectionChanged(const QVector<int> &columnIndices);

private:
    void rebuild();
    const LogTable *m_t = nullptr;
    EcuFamily       m_family;
};

} // namespace datalog
