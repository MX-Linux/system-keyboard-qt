#ifndef SELECTLAYOUTDIALOG_H
#define SELECTLAYOUTDIALOG_H

#include <QDialog>
#include "keyboardlayouts.h"

namespace Ui {
class SelectLayoutDialog;
}

class SelectLayoutDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SelectLayoutDialog(const KeyboardInfo& info, QWidget *parent = nullptr);
    ~SelectLayoutDialog();

    QPair<KeyboardConfigItem, KeyboardConfigItem> selectedLayout() const;
private slots:
    void refreshSelectedVariants();

private:
    Ui::SelectLayoutDialog *ui;
    KeyboardInfo m_info;
    bool m_good;
};

#endif // SELECTLAYOUTDIALOG_H
