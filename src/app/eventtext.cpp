// SPDX-License-Identifier: GPL-3.0-or-later
#include "app/eventtext.h"

#include "app/currenttext.h"
#include "app/editorscene.h"
#include "app/sectiontext.h"
#include "core/catalog.h"
#include "core/project.h"
#include "core/simulation.h"

#include <QCoreApplication>

#include <cmath>

namespace {
class EventText
{
    Q_DECLARE_TR_FUNCTIONS(EventText)
};

QString catalogText(const QString &source)
{
    return QCoreApplication::translate("Catalog", source.toUtf8().constData());
}

QString loadName(const Project &project, const PlacedLoad &load)
{
    const LoadType *type = project.catalog().loadType(load.type);
    return type->plugsIn ? catalogText(type->name) : loadCaption(project, load, *type);
}

QString objectName(const Project &project, const QString &object)
{
    if (object == QLatin1String("supply"))
        return EventText::tr("Supply");
    if (object == QLatin1String("nbar"))
        return EventText::tr("N bar");
    if (object == QLatin1String("pebar"))
        return EventText::tr("PE bar");
    if (const PlacedLoad *load = project.load(object))
        return loadName(project, *load);
    return project.designation(object);
}

QString wireName(const Project &project, const Wire &wire)
{
    return objectName(project, terminalObject(wire.from)) + QStringLiteral(" → ")
            + objectName(project, terminalObject(wire.to));
}

QString wholeAmperes(double amperes)
{
    return QCoreApplication::translate("CurrentText", "%1 A").arg(std::lround(amperes));
}

// Below 1 A in whole milliamperes, which is how leakage is read.
QString smallCurrentText(double amperes)
{
    return amperes < 1 ? EventText::tr("%1 mA").arg(std::lround(amperes * 1000))
                       : currentText(amperes);
}

// The device's rating with In as it was at the trip, as edits don't reset a trip: "C16".
QString ratingAtTrip(const PlacedDevice &device, double ratedCurrent)
{
    PlacedDevice atTrip = device;
    atTrip.parameters[QStringLiteral("ratedCurrent")] = ratedCurrent;
    return ratingText(atTrip);
}

QString trippedText(const Project &project, const SimulationEvent &event)
{
    const bool stairwell = event.id == SupplyBreakerId;
    const PlacedDevice *device = project.device(event.id);
    if (!stairwell && !device)
        return {};
    const QString name
            = stairwell ? EventText::tr("Stairwell breaker") : project.designation(event.id);
    switch (event.cause) {
    case TripCause::Leakage:
        return EventText::tr("%1 tripped: leakage %2, trips at %3")
                .arg(name, smallCurrentText(event.current), smallCurrentText(event.limit));
    case TripCause::Test:
        return EventText::tr("%1 tripped: test button").arg(name);
    default:
        break;
    }
    const QString rating = stairwell
            ? project.supply().upstreamCurve + QString::number(project.supply().upstreamRating)
            : ratingAtTrip(*device, event.limit);
    const QString current = currentText(event.current);
    return event.cause == TripCause::ShortCircuit
            ? EventText::tr("%1 tripped: short circuit, %2 on %3").arg(name, current, rating)
            : EventText::tr("%1 tripped: overload, %2 on %3").arg(name, current, rating);
}

QString wireText(const Project &project, const Wire &wire, const SimulationEvent &event)
{
    const QString name = wireName(project, wire);
    const QString current = currentText(event.current);
    const QString section = sectionText(wire.section);
    const QString allowed = wholeAmperes(event.limit);
    return event.kind == SimulationEvent::Kind::Overheating
            ? EventText::tr("Wire %1 overheating: %2 on %3, allowed %4")
                      .arg(name, current, section, allowed)
            : EventText::tr("Wire %1 overloaded: %2 on %3, allowed %4")
                      .arg(name, current, section, allowed);
}

QString dangerText(const Project &project, const SimulationEvent &event)
{
    const PlacedLoad *load = project.load(event.id);
    if (!load)
        return {};
    const QString name = loadName(project, *load);
    // Leakage carries the current through the person, an earth fault none.
    if (event.current > 0)
        return EventText::tr("%1: %2 through a person, no RCD turned it off")
                .arg(name, smallCurrentText(event.current));
    return EventText::tr("%1: its case is live, %2")
            .arg(name, EventText::tr("%1 V").arg(std::lround(event.voltage)));
}
} // namespace

QString eventText(const Project &project, const SimulationEvent &event)
{
    using Kind = SimulationEvent::Kind;
    switch (event.kind) {
    case Kind::Tripped:
        return trippedText(project, event);
    case Kind::Overloaded:
    case Kind::Overheating:
        if (const Wire *wire = project.wire(event.id))
            return wireText(project, *wire, event);
        if (const PlacedDevice *device = project.device(event.id))
            return EventText::tr("%1 overloaded: %2 on %3, an RCD doesn't protect against overload")
                    .arg(project.designation(event.id), currentText(event.current),
                         ratingText(*device));
        return {};
    case Kind::Dangerous:
        return dangerText(project, event);
    case Kind::Cleared:
        break;
    }
    return {};
}
