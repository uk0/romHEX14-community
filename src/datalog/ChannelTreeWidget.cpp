#include "ChannelTreeWidget.h"
#include "LogTable.h"
#include "EcuFamily.h"
#include "ChannelAlias.h"
#include "CanonicalSignal.h"

#include <QHeaderView>
#include <QHash>

namespace datalog {

ChannelTreeWidget::ChannelTreeWidget(QWidget *parent)
    : QTreeWidget(parent)
{
    setHeaderLabels({tr("Channel"), tr("Unit")});
    header()->setSectionResizeMode(0, QHeaderView::Stretch);
    header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    setRootIsDecorated(true);
    setUniformRowHeights(true);
    setSelectionMode(QAbstractItemView::NoSelection);
    setMinimumWidth(220);

    connect(this, &QTreeWidget::itemChanged, this, [this](QTreeWidgetItem *, int){
        QVector<int> cols;
        for (int i = 0; i < topLevelItemCount(); ++i) {
            auto *grp = topLevelItem(i);
            for (int j = 0; j < grp->childCount(); ++j) {
                auto *ch = grp->child(j);
                if (ch->checkState(0) == Qt::Checked) {
                    bool ok = false;
                    int colIdx = ch->data(0, Qt::UserRole).toInt(&ok);
                    if (ok) cols.push_back(colIdx);
                }
            }
        }
        emit selectionChanged(cols);
    });
}

void ChannelTreeWidget::setTable(const LogTable *t, EcuFamily family)
{
    m_t = t;
    m_family = family;
    rebuild();
}

void ChannelTreeWidget::rebuild()
{
    clear();
    if (!m_t) return;

    // Group columns by canonical category. Skip Time (col 0).
    QHash<QString, QTreeWidgetItem *> groups;
    auto getGroup = [&](const QString &name) -> QTreeWidgetItem * {
        auto it = groups.find(name);
        if (it != groups.end()) return *it;
        auto *grp = new QTreeWidgetItem(this);
        grp->setText(0, name);
        grp->setFirstColumnSpanned(true);
        QFont f = grp->font(0); f.setBold(true); grp->setFont(0, f);
        grp->setExpanded(true);
        groups.insert(name, grp);
        return grp;
    };

    for (int i = 1; i < m_t->colCount(); ++i) {
        const LogColumn &c = m_t->columns[i];
        AliasMatch m = ChannelAlias::resolve(c.name, m_family);
        QString cat = (m.signal == Signal::Unknown) ? tr("Other (raw)")
                                                    : signalCategory(m.signal);
        auto *grp = getGroup(cat);
        auto *item = new QTreeWidgetItem(grp);
        QString label = c.name;
        if (m.signal != Signal::Unknown) {
            label = signalName(m.signal);
            if (m.signal == Signal::KnockRetardCyl && m.cylIndex > 0)
                label += QStringLiteral(" cyl %1").arg(m.cylIndex);
            label += QStringLiteral("  (%1)").arg(c.name);
        }
        item->setText(0, label);
        item->setText(1, c.unitRaw);
        item->setData(0, Qt::UserRole, i);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(0, Qt::Unchecked);
        if (!c.description.isEmpty()) item->setToolTip(0, c.description);
    }
}

} // namespace datalog
