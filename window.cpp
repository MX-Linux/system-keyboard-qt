#include "window.h"
#include "ui_window.h"

#include <QDebug>
#include <QFile>
#include <QTextStream>
#include "selectlayoutdialog.h"
#include "posixconfigparser.h"
#include <QProcess>
#include <QEventLoop>
#include <QMessageBox>
#include "unistd.h"
#include <QTemporaryFile>


Window::Window(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::Window)

{
    ui->setupUi(this);
    ui->listWidget_KeyboardLayouts->setSelectionBehavior(QListWidget::SelectItems);
    ui->listWidget_KeyboardLayouts->setSelectionMode(QListWidget::SingleSelection);

    for(const auto& model : m_keyboardInfo.models())
    {
        ui->comboBox_KeyboardModel->addItem(keyboardtr(model.description));
        int index = ui->comboBox_KeyboardModel->count() - 1;
        ui->comboBox_KeyboardModel->setItemData(index, {model.name}, OptionName);
    }
    ui->comboBox_KeyboardModel->model()->sort(0);

    //fallback icons are used when the theme icon is not supported...or theme is not present
    ui->pushButton_AddLayout->setIcon(QIcon::fromTheme("list-add", QIcon(":/icons/list-add.png")));
    ui->pushButton_MoveLayoutUp->setIcon(QIcon::fromTheme("go-up", QIcon(":/icons/go-up.png")));
    ui->pushButton_MoveLayoutDown->setIcon(QIcon::fromTheme("go-down", QIcon(":/icons/go-down.png")));
    ui->pushButton_RemoveLayout->setIcon(QIcon::fromTheme("list-remove", QIcon(":/icons/list-remove.png")));
    ui->pushButton_Help->setIcon(QIcon::fromTheme("help-about", QIcon(":/icons/help-about.png")));

    connect(ui->pushButton_AddLayout, &QPushButton::clicked, [this](){
        SelectLayoutDialog dialog{m_keyboardInfo, this};
        int result = dialog.exec();
        if(result == QDialog::Accepted)
        {
            auto r = dialog.selectedLayout();
            ui->listWidget_KeyboardLayouts->addItem(new KeyboardLayoutListWidgetItem{r.first, r.second, m_keyboardInfo.layoutIcon(r.first.name)});
        }
    });

    connect(ui->pushButton_MoveLayoutUp, &QPushButton::clicked, [this](){
        if(ui->listWidget_KeyboardLayouts->selectedItems().isEmpty()) return;
        auto item = ui->listWidget_KeyboardLayouts->selectedItems().at(0);
        int row = ui->listWidget_KeyboardLayouts->row(item);
        if(row == 0) return;
        item = ui->listWidget_KeyboardLayouts->takeItem(row);
        ui->listWidget_KeyboardLayouts->insertItem(row - 1, item);
        ui->listWidget_KeyboardLayouts->setCurrentRow(row - 1);
        refreshLayoutButtonStates();
    });

    connect(ui->pushButton_MoveLayoutDown, &QPushButton::clicked, [this](){
        if(ui->listWidget_KeyboardLayouts->selectedItems().isEmpty()) return;
        auto item = ui->listWidget_KeyboardLayouts->selectedItems().at(0);
        int row = ui->listWidget_KeyboardLayouts->row(item);
        if(row == ui->listWidget_KeyboardLayouts->count() - 1) return;
        item = ui->listWidget_KeyboardLayouts->takeItem(row);
        ui->listWidget_KeyboardLayouts->insertItem(row + 1, item);
        ui->listWidget_KeyboardLayouts->setCurrentRow(row + 1);
        refreshLayoutButtonStates();
    });

    connect(ui->pushButton_RemoveLayout, &QPushButton::clicked, [this](){
        if(ui->listWidget_KeyboardLayouts->selectedItems().isEmpty()) return;
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

    connect(ui->pushButton_Help, &QPushButton::clicked, [](){
        QLocale locale;
        QString lang = locale.bcp47Name();
        QFileInfo viewer("/usr/bin/mx-viewer");
        QFileInfo viewer2("/usr/bin/antix-viewer");
        QString rootrunoption;
        QString url = "file:///usr/share/doc/system-keyboard-qt/help/help.html";
        QString cmd;

        rootrunoption.clear();

        if (getuid() == 0){
            rootrunoption = "runuser -l $(logname) -c ";
        }

        if (viewer.exists())
        {
            cmd = QString("mx-viewer %1 '%2' &").arg(url).arg(tr("System Keyboard"));
        }
        else if (viewer2.exists())
        {
            cmd = QString("antix-viewer %1 '%2' &").arg(url).arg(tr("System Keyboard"));
        }
        else
        {   if (rootrunoption.isEmpty()){
                cmd = QString("xdg-open %1").arg(url);
            } else {
                cmd = QString(rootrunoption + "\"DISPLAY=$DISPLAY xdg-open %1\" &").arg(url);
            }
        }
        system(cmd.toUtf8());
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
    if(ui->listWidget_KeyboardLayouts->selectedItems().isEmpty())
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
    QString model = getModel();
    QStringList layouts = getLayoutsAndVariants().first;
    QStringList variants = getLayoutsAndVariants().second;
    QStringList options = getOptions();

    qDebug() << "Applying keyboard configuration:";
    qDebug() << "  Model:" << model;
    qDebug() << "  Layouts:" << layouts.join(',');
    qDebug() << "  Variants:" << variants.join(',');
    qDebug() << "  Options:" << options.join(',');

#ifdef DISTRO_ARCH
    // On Arch Linux, use localectl to set keyboard configuration
    QString layoutArg = layouts.join(',');
    QString modelArg = model;
    QString variantArg = variants.join(',');
    QString optionsArg = options.join(',');

    QString localectlCmd = QString("localectl set-x11-keymap '%1' '%2' '%3' '%4'")
        .arg(layoutArg)
        .arg(modelArg)
        .arg(variantArg)
        .arg(optionsArg);

    qDebug() << "Running localectl:" << localectlCmd;

    bool success;
    if (getuid() != 0) {
        success = Cmd().runAsRoot(localectlCmd);
    } else {
        success = Cmd().run(localectlCmd);
    }

    if (!success) {
        QMessageBox::critical(this, tr("Error"), tr("Failed to apply keyboard configuration using localectl"), QMessageBox::Close);
        return false;
    }
#else
    // On Debian/Ubuntu, write to /etc/default/keyboard
    QString content;
    {
        QFile io{KeyboardDefaultFile};
        if(!io.open(QFile::ReadOnly))
        {
            qDebug() << "Keyboard config file not found, will create:" << KeyboardDefaultFile;
        }
        QTextStream stream{&io};
        content = stream.readAll();
        io.close();
    }
    PosixConfigParser parser{content};
    parser.set("XKBMODEL", model);
    parser.set("XKBLAYOUT", layouts.join(','));
    parser.set("XKBVARIANT", variants.join(','));
    parser.set("XKBOPTIONS", options.join(','));
    qDebug() << parser.source();

    QTemporaryFile tempfile;
    if(!tempfile.open()){
        qDebug() << "Failed to open file write-only:" << tempfile.fileName();
        QMessageBox::critical(this, tr("Error"), tr("Failed to open file: ") + tempfile.fileName(), QMessageBox::Close);
        return false;
    }

    QTextStream stream{&tempfile};
    stream << parser.source();
    tempfile.close();

    if (getuid() != 0) {
        Cmd().runAsRoot("cp " + tempfile.fileName() + " " + KeyboardDefaultFile);
        Cmd().runAsRoot("udevadm trigger -t subsystems --subsystem-match=input --action=change");
    } else {
        Cmd().run("cp " + tempfile.fileName() + " " + KeyboardDefaultFile);
        Cmd().run("udevadm trigger -t subsystems --subsystem-match=input --action=change");
    }
#endif

    // Apply settings immediately using setxkbmap (works on both Arch and Debian)
    // Clear existing options first to prevent accumulation
    QString setxkbmapCmd = QString("setxkbmap -option '' -model '%1' -layout '%2'")
        .arg(model)
        .arg(layouts.join(','));

    if (!variants.isEmpty() && !variants.join(',').isEmpty()) {
        setxkbmapCmd += QString(" -variant '%1'").arg(variants.join(','));
    }

    if (!options.isEmpty() && !options.join(',').isEmpty()) {
        setxkbmapCmd += QString(" -option '%1'").arg(options.join(','));
    }

    qDebug() << "Applying immediately with setxkbmap:" << setxkbmapCmd;
    Cmd().run(setxkbmapCmd);

    return true;
}

void Window::loadDefaults()
{
#ifdef DISTRO_ARCH
    // On Arch Linux, read configuration from localectl
    QString localectlOutput = Cmd().getOut("localectl status", true);
    QMap<QString, QString> config;

    // Parse localectl output
    QStringList lines = localectlOutput.split('\n');
    for (const QString &line : lines) {
        if (line.contains("X11 Layout:")) {
            config["XKBLAYOUT"] = line.split(':').last().trimmed();
        } else if (line.contains("X11 Model:")) {
            config["XKBMODEL"] = line.split(':').last().trimmed();
        } else if (line.contains("X11 Variant:")) {
            config["XKBVARIANT"] = line.split(':').last().trimmed();
        } else if (line.contains("X11 Options:")) {
            config["XKBOPTIONS"] = line.split(':').last().trimmed();
        }
    }

    qDebug() << "Loaded configuration from localectl:";
    for(const auto& key : config.keys())
    {
        qDebug() << key << "=" << config[key];
    }
    QString modelName = config["XKBMODEL"];
#else
    // On Debian/Ubuntu, read from /etc/default/keyboard
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
    QMap<QString, QString> config = parser.config;
    for(const auto& key : config.keys())
    {
        qDebug() << key << "=" << config[key];
    }
    QString modelName = config["XKBMODEL"];
#endif
    for(int i = 0; i < ui->comboBox_KeyboardModel->count(); i++)
    {
        if(ui->comboBox_KeyboardModel->itemData(i, OptionName).value<QString>() == modelName)
        {
            ui->comboBox_KeyboardModel->setCurrentIndex(i);
            break;
        }
    }
    QStringList layouts = config["XKBLAYOUT"].split(',');
    QStringList variants = config["XKBVARIANT"].split(',', Qt::KeepEmptyParts);

    while ( variants.size() < layouts.size()){
        variants.append(" ");
    }


    for(int i = 0; i < layouts.size(); i++)
    {
        for(const auto& l : m_keyboardInfo.layouts())
        {
            if(l.config.name == layouts[i])
            {
                KeyboardConfigItem layout = l.config;
                KeyboardConfigItem variant;
                if(i < variants.size())
                {
                    for(const auto& v : l.variants)
                    {
                        if(v.name == variants[i])
                        {
                            variant = v;
                        }
                    }
                }
                ui->listWidget_KeyboardLayouts->addItem(new KeyboardLayoutListWidgetItem{layout, variant, m_keyboardInfo.layoutIcon(layout.name)});
                break;
            }
        }
    }
    m_extraOptions.clear();
    for(const auto& optionString : config["XKBOPTIONS"].split(','))
    {
        bool complete = false;
        for(auto* comboBox : m_comboBoxInfo.keys())
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
    for(auto* box : m_comboBoxInfo.keys())
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
    for(const auto& allowed : options)
    {
        for(const auto& option : m_keyboardInfo.options())
        {
            if(option.config.name == allowed)
            {
                QGroupBox* box = new QGroupBox{keyboardtr(option.config.description)};
                QComboBox* comboBox = new QComboBox;
                box->setLayout(new QVBoxLayout);
                box->layout()->addWidget(comboBox);
                comboBox->addItem("-");
                int index = 1;
                for(const auto& opt : option.options)
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
