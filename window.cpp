#include "window.h"
#include "ui_window.h"

#include <QDebug>
#include <QFile>
#include <QTextStream>
#include "selectlayoutdialog.h"
#include "posixconfigparser.h"
#include "translation.h"
#include <QProcess>
#include <QEventLoop>
#include <QMessageBox>

Window::Window(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::Window)
{
    ui->setupUi(this);
    ui->listWidget_KeyboardLayouts->setSelectionBehavior(QListWidget::SelectItems);
    ui->listWidget_KeyboardLayouts->setSelectionMode(QListWidget::SingleSelection);
    for(auto model : m_keyboardInfo.models())
    {
        ui->comboBox_KeyboardModel->addItem(model.description);
        int index = ui->comboBox_KeyboardModel->count() - 1;
        ui->comboBox_KeyboardModel->setItemData(index, {model.name}, OptionName);
    }

    connect(ui->pushButton_AddLayout, &QPushButton::clicked, [this](){
        SelectLayoutDialog dialog{m_keyboardInfo, this};
        int result = dialog.exec();
        if(result == QDialog::Accepted)
        {
            auto r = dialog.selectedLayout();
            ui->listWidget_KeyboardLayouts->addItem(new KeyboardLayoutListWidgetItem{r.first, r.second});
        }
    });

    connect(ui->pushButton_MoveLayoutUp, &QPushButton::clicked, [this](){
        if(ui->listWidget_KeyboardLayouts->selectedItems().length() == 0) return;
        auto item = ui->listWidget_KeyboardLayouts->selectedItems().at(0);
        int row = ui->listWidget_KeyboardLayouts->row(item);
        if(row == 0) return;
        item = ui->listWidget_KeyboardLayouts->takeItem(row);
        ui->listWidget_KeyboardLayouts->insertItem(row - 1, item);
        ui->listWidget_KeyboardLayouts->setCurrentRow(row - 1);
        refreshLayoutButtonStates();
    });

    connect(ui->pushButton_MoveLayoutDown, &QPushButton::clicked, [this](){
        if(ui->listWidget_KeyboardLayouts->selectedItems().length() == 0) return;
        auto item = ui->listWidget_KeyboardLayouts->selectedItems().at(0);
        int row = ui->listWidget_KeyboardLayouts->row(item);
        if(row == ui->listWidget_KeyboardLayouts->count() - 1) return;
        item = ui->listWidget_KeyboardLayouts->takeItem(row);
        ui->listWidget_KeyboardLayouts->insertItem(row + 1, item);
        ui->listWidget_KeyboardLayouts->setCurrentRow(row + 1);
        refreshLayoutButtonStates();
    });

    connect(ui->pushButton_RemoveLayout, &QPushButton::clicked, [this](){
        if(ui->listWidget_KeyboardLayouts->selectedItems().length() == 0) return;
        auto item = ui->listWidget_KeyboardLayouts->selectedItems().at(0);
        int row = ui->listWidget_KeyboardLayouts->row(item);
        item = ui->listWidget_KeyboardLayouts->takeItem(row);
        delete item;
        refreshLayoutButtonStates();
    });

    connect(ui->buttonBox, &QDialogButtonBox::clicked, [this](QAbstractButton* button){
        int role = ui->buttonBox->buttonRole(button);
        if(role == QDialogButtonBox::ButtonRole::ApplyRole)
        {
            apply();
        }
        else if(role == QDialogButtonBox::ButtonRole::AcceptRole)
        {
            if(apply()) close();
        }
        else if(role == QDialogButtonBox::ButtonRole::RejectRole)
        {
            close();
        }
    });

    connect(ui->listWidget_KeyboardLayouts, &QListWidget::itemSelectionChanged, this, &Window::refreshLayoutButtonStates);

    populateLayout(ui->verticalLayout_HotkeysLayout, hotkeyOptions);
    populateLayout(ui->verticalLayout_AdvancedLayout, advancedOptions);

    refreshLayoutButtonStates();

    loadDefaults();

    ui->tabWidget->setCurrentIndex(0);
}

Window::~Window()
{
    delete ui;
}

void Window::refreshLayoutButtonStates()
{
    if(ui->listWidget_KeyboardLayouts->selectedItems().length() == 0)
    {
        ui->pushButton_MoveLayoutUp->setEnabled(false);
        ui->pushButton_MoveLayoutDown->setEnabled(false);
        ui->pushButton_RemoveLayout->setEnabled(false);
    }
    else
    {
        int row = ui->listWidget_KeyboardLayouts->row(ui->listWidget_KeyboardLayouts->selectedItems().at(0));
        ui->pushButton_MoveLayoutUp->setEnabled(true);
        ui->pushButton_MoveLayoutDown->setEnabled(true);
        ui->pushButton_RemoveLayout->setEnabled(true);
        if(row == 0)
            ui->pushButton_MoveLayoutUp->setEnabled(false);
        if(row == ui->listWidget_KeyboardLayouts->count() - 1)
            ui->pushButton_MoveLayoutDown->setEnabled(false);
    }
}

QString Window::generateOutput()
{
    QString result = QString("XKBMODEL=\"%1\"\nXKBLAYOUT=\"%2\"\nXKBVARIANT=\"%3\"\nXKBOPTIONS=\"%4\"\n")
            .arg(getModel())
            .arg(getLayoutsAndVariants().first.join(','))
            .arg(getLayoutsAndVariants().second.join(','))
            .arg(getOptions().join(','));
    return result;
}

bool Window::apply()
{
    // generateOutput() is not used to create file content incase there is some options the program doesn't support
    // so it just is used for debugging
    // qDebug() << generateOutput();
    QString content;
    {
        QFile io{KeyboardDefaultFile};
        if(!io.open(QFile::ReadOnly))
        {
            qDebug() << "Failed to open file read-only:" << KeyboardDefaultFile;
            QMessageBox::critical(this, tr("Error"), tr("Failed to open file: ") + KeyboardDefaultFile, QMessageBox::Close);
            return false;
        }
        QTextStream stream{&io};
        content = stream.readAll();
        io.close();
    }
    PosixConfigParser parser{content};
    parser.set("XKBMODEL", getModel());
    parser.set("XKBLAYOUT", getLayoutsAndVariants().first.join(','));
    parser.set("XKBVARIANT", getLayoutsAndVariants().second.join(','));
    parser.set("XKBOPTIONS", getOptions().join(','));
    qDebug() << parser.source();
    {
        QFile io{KeyboardDefaultFile};
        if(!io.open(QFile::WriteOnly))
        {
            qDebug() << "Failed to open file write-only:" << KeyboardDefaultFile;
            QMessageBox::critical(this, tr("Error"), tr("Failed to open file: ") + KeyboardDefaultFile, QMessageBox::Close);
            return false;
        }
        QTextStream stream{&io};
        stream << parser.source();
        io.close();
    }
    QEventLoop loop;
    QProcess proc;
    auto command = QString("setxkbmap");
    auto commandOptions = QStringList()
               << "-model" << getModel()
               << "-layout" << getLayoutsAndVariants().first
               << "-variant" << getLayoutsAndVariants().second
               << "-options" << getOptions();
    proc.start("setxkbmap", commandOptions);
    loop.exec();
    QObject::disconnect(&proc, nullptr, nullptr, nullptr);
    if(proc.exitCode() != 0)
    {
        QMessageBox::critical(this, tr("Error"), tr("Command exited with code non-zero: ") + (QStringList() << command << commandOptions).join(' '), QMessageBox::Close);
        return false;
    }
    proc.close();
    return true;
}

void Window::loadDefaults()
{
    QFile io{KeyboardDefaultFile};
    if(!io.open(QFile::ReadOnly))
    {
        QMessageBox::critical(this, tr("Error"), tr("Failed to open file: ") + KeyboardDefaultFile + "\n" + tr("Most settings will not be loaded"), QMessageBox::Close);
        return;
    }
    QTextStream stream{&io};
    QString content = stream.readAll();
    io.close();
    PosixConfigParser parser{content};
    for(auto key : parser.config.keys())
    {
        qDebug() << key << "=" << parser.config[key];
    }
    QString modelName = parser.config["XKBMODEL"];
    for(int i = 0; i < ui->comboBox_KeyboardModel->count(); i++)
    {
        if(ui->comboBox_KeyboardModel->itemData(i, OptionName).value<QString>() == modelName)
        {
            ui->comboBox_KeyboardModel->setCurrentIndex(i);
            break;
        }
    }
    QStringList layouts = parser.config["XKBLAYOUT"].split(',');
    QStringList variants = parser.config["XKBVARIANT"].split(',');
    for(int i = 0; i < layouts.size(); i++)
    {
        for(auto l : m_keyboardInfo.layouts())
        {
            if(l.config.name == layouts[i])
            {
                KeyboardConfigItem layout = l.config;
                KeyboardConfigItem variant;
                if(i >= variants.size())
                {
                    for(auto v : l.variants)
                    {
                        if(v.name == variants[i])
                        {
                            variant = v;
                        }
                    }
                }
                ui->listWidget_KeyboardLayouts->addItem(new KeyboardLayoutListWidgetItem{layout, variant});
                break;
            }
        }
    }
    m_extraOptions.clear();
    for(auto optionString : parser.config["XKBOPTIONS"].split(','))
    {
        bool complete = false;
        for(auto comboBox : m_comboBoxInfo.keys())
        {
            if(m_comboBoxInfo[comboBox].values().contains(optionString))
            {
                int index = m_comboBoxInfo[comboBox].key(optionString);
                comboBox->setCurrentIndex(index);
                complete = true;
                break;
            }
        }
        if(!complete)
            m_extraOptions.append(optionString);
    }
    refreshLayoutButtonStates();
}

QString Window::getModel()
{
    QString modelName = ui->comboBox_KeyboardModel->currentData(OptionName).value<QString>();
    return modelName;
}

QPair<QStringList, QStringList> Window::getLayoutsAndVariants()
{
    QStringList layouts;
    QStringList variants;
    for(int i = 0; i < ui->listWidget_KeyboardLayouts->count(); i++)
    {
        KeyboardLayoutListWidgetItem* item = dynamic_cast<KeyboardLayoutListWidgetItem*>(ui->listWidget_KeyboardLayouts->item(i));
        layouts.append(item->m_layout.name);
        variants.append(item->m_variant.name);
    }
    return {layouts, variants};
}

QStringList Window::getOptions()
{
    QStringList options;
    for(auto box : m_comboBoxInfo.keys())
    {
        int index = box->currentIndex();
        if(m_comboBoxInfo[box].contains(index))
        {
            options.append(m_comboBoxInfo[box].value(index));
        }
    }
    options.append(m_extraOptions);
    return options;
}

void Window::populateLayout(QLayout *layout, QStringList options)
{
    for(auto allowed : options)
    {
        for(auto option : m_keyboardInfo.options())
        {
            if(option.config.name == allowed)
            {
                QGroupBox* box = new QGroupBox{option.config.description};
                QComboBox* comboBox = new QComboBox;
                box->setLayout(new QVBoxLayout);
                box->layout()->addWidget(comboBox);
                comboBox->addItem(tr("Disabled"));
                int index = 1;
                for(auto opt : option.options)
                {
                    QString displayText = keyboardtr(opt.description);
                    comboBox->addItem(displayText);
                    m_comboBoxInfo[comboBox][index] = opt.name;
                    index++;
                }
                layout->addWidget(box);
                break;
            }
        }
    }

}

KeyboardLayoutListWidgetItem::~KeyboardLayoutListWidgetItem()
{
}
