#pragma once

#include <QString>

// Двомовний інтерфейс: українська та англійська.
// Мову вибирають в інсталяторі або в налаштуваннях (діє після перезапуску).
namespace I18n {
bool isUkrainian();
void setUkrainian(bool on);
} // namespace I18n

// Рядок інтерфейсу в обох мовах: tr2("Українською", "In English")
inline QString tr2(const char *uk, const char *en)
{
    return QString::fromUtf8(I18n::isUkrainian() ? uk : en);
}
