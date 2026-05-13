#pragma once
#include <QWidget>
#include <QTreeWidget>
#include <QVector>

class QLineEdit;
class QPushButton;
class QTreeWidgetItem;

namespace datalog {

class LogTable;
enum class EcuFamily : int;

// Composite widget: search bar + deselect button + channel tree
class ChannelTreeWidget : public QWidget {
    Q_OBJECT
public:
    explicit ChannelTreeWidget(QWidget *parent = nullptr);
    void setTable(const LogTable *t, EcuFamily family);

    // Delegate tree access for preselectDefaultChannels
    int topLevelItemCount() const { return m_tree->topLevelItemCount(); }
    QTreeWidgetItem *topLevelItem(int i) const { return m_tree->topLevelItem(i); }

signals:
    void selectionChanged(const QVector<int> &columnIndices);

private:
    void rebuild();
    void applyFilter(const QString &text);
    void deselectAll();
    void emitSelection();

    const LogTable *m_t = nullptr;
    EcuFamily       m_family;

    QLineEdit      *m_search = nullptr;
    QPushButton    *m_deselectBtn = nullptr;
    QTreeWidget    *m_tree = nullptr;
};

} // namespace datalog
