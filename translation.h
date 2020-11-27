#ifndef TRANSLATION_H
#define TRANSLATION_H

#include <libintl.h>
#include <QString>

QString keyboardtr(QString input)
{
    return QString::fromStdString(dgettext("xkeyboard-config", input.toStdString().c_str()));
}

#endif // TRANSLATION_H
