#ifndef FILITERABLECOMBOBOX_H
#define FILITERABLECOMBOBOX_H

#include <QComboBox>
#include <QStringListModel>
#include <QSortFilterProxyModel>
#include <QCompleter>
#include <QLineEdit>
#include <QFocusEvent>

// Based on this: https://stackoverflow.com/questions/4827207/how-do-i-filter-the-pyqt-qcombobox-items-based-on-the-text-input

class FilterableComboBox : public QComboBox
{
public:
    FilterableComboBox(QWidget* parent = nullptr);
    void setModel(QAbstractItemModel* model);
    void setModelColumn(int column);
    void setCurrentText(const QString& text);
    void setCurrentIndex(int index);
    QAbstractItemView* view();
    int index();
    void focusOutEvent(QFocusEvent* event);
private:
    QString m_saved;
    QCompleter* m_completer;
    QSortFilterProxyModel* m_filterModel;

};

#endif // FILITERABLECOMBOBOX_H
