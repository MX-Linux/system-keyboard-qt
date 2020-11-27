#include "selectlayoutdialog.h"
#include "ui_selectlayoutdialog.h"

SelectLayoutDialog::SelectLayoutDialog(const KeyboardInfo &info, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::SelectLayoutDialog),
    m_info(info),
    m_good(false)
{
    ui->setupUi(this);
    for(auto layout : m_info.layouts())
    {
        ui->comboBox_SelectedLayout->addItem(layout.config.description);
    }
    refreshSelectedVariants();

    connect(ui->comboBox_SelectedLayout, &QComboBox::currentTextChanged, [this](QString){
        refreshSelectedVariants();
    });
}

SelectLayoutDialog::~SelectLayoutDialog()
{
    delete ui;
}

QPair<KeyboardConfigItem, KeyboardConfigItem> SelectLayoutDialog::selectedLayout() const
{
    for(auto layout : m_info.layouts())
    {
        if(layout.config.description == ui->comboBox_SelectedLayout->currentText())
        {
            for(auto variant : layout.variants)
            {
                if(variant.description == ui->comboBox_SelectedVariant->currentText())
                {
                    return {layout.config, variant};
                }
            }
            return {layout.config, {}};
        }
    }
    return {{}, {}};
}

void SelectLayoutDialog::refreshSelectedVariants()
{
    for(auto layout : m_info.layouts())
    {
        if(layout.config.description == ui->comboBox_SelectedLayout->currentText())
        {
            ui->comboBox_SelectedVariant->clear();
            ui->comboBox_SelectedVariant->addItem(tr("No Variant"));
            for(auto variant : layout.variants)
            {
                ui->comboBox_SelectedVariant->addItem(variant.description);
            }
            break;
        }
    }
}
