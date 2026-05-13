#include "ChannelTreeWidget.h"
#include "LogTable.h"
#include "EcuFamily.h"
#include "ChannelAlias.h"
#include "CanonicalSignal.h"

#include <QHeaderView>
#include <QHash>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLineEdit>
#include <QPushButton>

namespace datalog {

ChannelTreeWidget::ChannelTreeWidget(QWidget *parent)
    : QWidget(parent)
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(4);

    // Search bar + deselect button row
    auto *topRow = new QHBoxLayout;
    topRow->setContentsMargins(2, 2, 2, 0);
    topRow->setSpacing(4);

    m_search = new QLineEdit(this);
    m_search->setPlaceholderText(tr("Search channels…"));
    m_search->setClearButtonEnabled(true);
    topRow->addWidget(m_search, 1);

    m_deselectBtn = new QPushButton(tr("Clear"), this);
    m_deselectBtn->setToolTip(tr("Deselect all channels"));
    m_deselectBtn->setFixedWidth(50);
    topRow->addWidget(m_deselectBtn);

    root->addLayout(topRow);

    // Tree
    m_tree = new QTreeWidget(this);
    m_tree->setHeaderLabels({tr("Channel"), tr("Unit")});
    m_tree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_tree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_tree->setRootIsDecorated(true);
    m_tree->setUniformRowHeights(true);
    m_tree->setSelectionMode(QAbstractItemView::NoSelection);
    root->addWidget(m_tree, 1);

    setMinimumWidth(260);

    // Connections
    connect(m_tree, &QTreeWidget::itemChanged, this, [this](QTreeWidgetItem *, int){
        emitSelection();
    });

    connect(m_search, &QLineEdit::textChanged, this, &ChannelTreeWidget::applyFilter);
    connect(m_deselectBtn, &QPushButton::clicked, this, &ChannelTreeWidget::deselectAll);
}

void ChannelTreeWidget::emitSelection()
{
    QVector<int> cols;
    for (int i = 0; i < m_tree->topLevelItemCount(); ++i) {
        auto *grp = m_tree->topLevelItem(i);
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
}

void ChannelTreeWidget::deselectAll()
{
    m_tree->blockSignals(true);
    for (int i = 0; i < m_tree->topLevelItemCount(); ++i) {
        auto *grp = m_tree->topLevelItem(i);
        for (int j = 0; j < grp->childCount(); ++j)
            grp->child(j)->setCheckState(0, Qt::Unchecked);
    }
    m_tree->blockSignals(false);
    emitSelection();
}

void ChannelTreeWidget::applyFilter(const QString &text)
{
    QString filter = text.trimmed();
    for (int i = 0; i < m_tree->topLevelItemCount(); ++i) {
        auto *grp = m_tree->topLevelItem(i);
        int visibleChildren = 0;
        for (int j = 0; j < grp->childCount(); ++j) {
            auto *ch = grp->child(j);
            if (filter.isEmpty()) {
                ch->setHidden(false);
                ++visibleChildren;
            } else {
                bool match = ch->text(0).contains(filter, Qt::CaseInsensitive)
                          || ch->text(1).contains(filter, Qt::CaseInsensitive);
                ch->setHidden(!match);
                if (match) ++visibleChildren;
            }
        }
        // Hide group if all children are hidden; also match on group name
        bool groupMatch = grp->text(0).contains(filter, Qt::CaseInsensitive);
        grp->setHidden(visibleChildren == 0 && !groupMatch);
        if (groupMatch && !filter.isEmpty()) {
            // If group name matches, show all its children
            for (int j = 0; j < grp->childCount(); ++j)
                grp->child(j)->setHidden(false);
            grp->setHidden(false);
        }
    }
}

void ChannelTreeWidget::setTable(const LogTable *t, EcuFamily family)
{
    m_t = t;
    m_family = family;
    m_search->clear();
    rebuild();
}

void ChannelTreeWidget::rebuild()
{
    m_tree->clear();
    if (!m_t) return;

    struct ColEntry {
        int colIdx;
        AliasMatch match;
        QString categoryKey;
    };
    QVector<ColEntry> entries;
    entries.reserve(m_t->colCount());

    for (int i = 1; i < m_t->colCount(); ++i) {
        const LogColumn &c = m_t->columns[i];
        AliasMatch m = ChannelAlias::resolve(c.name, m_family);
        QString catKey;
        if (m.signal == Signal::Unknown)
            catKey = QStringLiteral("_other");
        else
            catKey = signalCategory(m.signal);
        entries.append({i, m, catKey});
    }

    // Fixed category display order
    struct CatDef { QString key; QString displayName; };
    QVector<CatDef> orderedCats;

    auto addCat = [&](Signal representative) {
        QString cat = signalCategory(representative);
        for (const auto &c : orderedCats)
            if (c.key == cat) return;
        orderedCats.append({cat, cat});
    };
    addCat(Signal::EngineRpm);
    addCat(Signal::IntakeManifoldPressure);
    addCat(Signal::LambdaActual);
    addCat(Signal::IgnitionTimingOut);
    addCat(Signal::RailPressureActual);
    addCat(Signal::ThrottlePosition);

    QString otherName = tr("Other (raw)");
    orderedCats.append({QStringLiteral("_other"), otherName});

    for (const auto &e : entries) {
        if (e.categoryKey == QStringLiteral("_other")) continue;
        bool found = false;
        for (const auto &c : orderedCats)
            if (c.key == e.categoryKey) { found = true; break; }
        if (!found)
            orderedCats.append({e.categoryKey, e.categoryKey});
    }

    QHash<QString, QTreeWidgetItem *> groupMap;
    for (const auto &cat : orderedCats) {
        bool hasEntries = false;
        for (const auto &e : entries) {
            if (e.categoryKey == cat.key) { hasEntries = true; break; }
        }
        if (!hasEntries) continue;

        auto *grp = new QTreeWidgetItem(m_tree);
        grp->setText(0, cat.displayName);
        grp->setFirstColumnSpanned(true);
        QFont f = grp->font(0); f.setBold(true); grp->setFont(0, f);
        grp->setExpanded(true);
        groupMap.insert(cat.key, grp);
    }

    for (const auto &e : entries) {
        auto *grp = groupMap.value(e.categoryKey);
        if (!grp) continue;

        const LogColumn &c = m_t->columns[e.colIdx];
        auto *item = new QTreeWidgetItem(grp);
        QString label = c.name;
        if (e.match.signal != Signal::Unknown) {
            label = signalName(e.match.signal);
            if (e.match.signal == Signal::KnockRetardCyl && e.match.cylIndex > 0)
                label += QStringLiteral(" cyl %1").arg(e.match.cylIndex);
            label += QStringLiteral("  (%1)").arg(c.name);
        }
        item->setText(0, label);
        item->setText(1, c.unitRaw);
        item->setData(0, Qt::UserRole, e.colIdx);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(0, Qt::Unchecked);
        if (!c.description.isEmpty()) item->setToolTip(0, c.description);
    }
}

} // namespace datalog
