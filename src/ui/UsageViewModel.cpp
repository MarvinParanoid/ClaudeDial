#include "UsageViewModel.h"

#include "core/Config.h"
#include "core/Format.h"
#include "core/UsageLevel.h"
#include "core/UsageService.h"

#include <QTimer>

namespace claudedial::ui {
namespace {

using namespace claudedial::core;

} // namespace

UsageViewModel::UsageViewModel(UsageService* service, Config* config, QObject* parent)
    : QObject(parent)
    , m_service(service)
    , m_config(config)
    , m_tick(new QTimer(this))
{
    connect(m_service, &UsageService::stateChanged, this, &UsageViewModel::changed);
    connect(m_service, &UsageService::fetchingChanged, this, &UsageViewModel::changed);
    connect(m_config, &Config::changed, this, &UsageViewModel::changed);

    m_tick->setInterval(30 * 1000);
    connect(m_tick, &QTimer::timeout, this, &UsageViewModel::changed);
    // Started by setLive() when the popup appears, not here.
}

void UsageViewModel::setLive(bool live)
{
    if (live == m_tick->isActive())
        return;
    if (live) {
        // The strings are stale by however long the popup was closed, so refresh
        // them before the first tick rather than up to thirty seconds into it.
        Q_EMIT changed();
        m_tick->start();
    } else {
        m_tick->stop();
    }
}

bool UsageViewModel::available() const
{
    return m_service->state().isValid();
}

QString UsageViewModel::unavailableReason() const
{
    return m_service->unavailableReason();
}

bool UsageViewModel::stale() const
{
    return m_service->state().stale;
}

bool UsageViewModel::fetching() const
{
    return m_service->isFetching();
}

QString UsageViewModel::updatedText() const
{
    return format::updatedAgo(m_service->state().updatedAt);
}

bool UsageViewModel::fiveHourAvailable() const
{
    return m_service->state().fiveHour.has_value();
}

int UsageViewModel::fiveHourPercent() const
{
    const auto& period = m_service->state().fiveHour;
    return period ? qRound(period->percentage) : 0;
}

QString UsageViewModel::fiveHourReset() const
{
    const auto& period = m_service->state().fiveHour;
    return period ? format::resetSentence(PeriodKind::FiveHour, *period) : QString();
}

QString UsageViewModel::fiveHourLevel() const
{
    const auto& period = m_service->state().fiveHour;
    if (!period)
        return QStringLiteral("normal");
    return core::levelName(
        core::levelFor(period->percentage, m_config->warningThreshold(), m_config->criticalThreshold()));
}

QString UsageViewModel::fiveHourPace() const
{
    const auto& period = m_service->state().fiveHour;
    return period ? format::pace(PeriodKind::FiveHour, *period) : QString();
}

bool UsageViewModel::sevenDayAvailable() const
{
    return m_service->state().sevenDay.has_value();
}

int UsageViewModel::sevenDayPercent() const
{
    const auto& period = m_service->state().sevenDay;
    return period ? qRound(period->percentage) : 0;
}

QString UsageViewModel::sevenDayReset() const
{
    const auto& period = m_service->state().sevenDay;
    return period ? format::resetSentence(PeriodKind::SevenDay, *period) : QString();
}

QString UsageViewModel::sevenDayLevel() const
{
    const auto& period = m_service->state().sevenDay;
    if (!period)
        return QStringLiteral("normal");
    return core::levelName(
        core::levelFor(period->percentage, m_config->warningThreshold(), m_config->criticalThreshold()));
}

void UsageViewModel::refresh()
{
    m_service->refreshNow();
}

void UsageViewModel::openSettings()
{
    Q_EMIT settingsRequested();
}

void UsageViewModel::close()
{
    Q_EMIT closeRequested();
}

} // namespace claudedial::ui
