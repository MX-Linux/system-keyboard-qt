#ifndef FILITERABLECOMBOBOX_H
#define FILITERABLECOMBOBOX_H

#include <QComboBox>
#include <QStringListModel>
#include <QSortFilterProxyModel>
#include <QCompleter>
#include <QLineEdit>

// Based on this: https://stackoverflow.com/questions/4827207/how-do-i-filter-the-pyqt-qcombobox-items-based-on-the-text-input

class FilterableComboBox : public QComboBox
{
public:
    FilterableComboBox(QWidget* parent = nullptr);
    void setModel(QAbstractItemModel* model);
    void setModelColumn(int column);
    QAbstractItemView* view();
    int index();
private:
    QCompleter* m_completer;
    QSortFilterProxyModel* m_filterModel;

};

#endif // FILITERABLECOMBOBOX_H
