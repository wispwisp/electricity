// SPDX-License-Identifier: GPL-3.0-or-later
#include "app/propertiespanel.h"

#include "app/editcommands.h"
#include "app/editorscene.h"
#include "app/language.h"
#include "app/sectiontext.h"
#include "core/catalog.h"
#include "core/project.h"

#include <QComboBox>
#include <QCoreApplication>
#include <QDoubleSpinBox>
#include <QEvent>
#include <QFormLayout>
#include <QJsonArray>
#include <QLabel>
#include <QLocale>
#include <QSpinBox>
#include <QUndoStack>
#include <QVBoxLayout>

namespace {
QString catalogText(const QString &source)
{
    return QCoreApplication::translate("Catalog", source.toUtf8().constData());
}

QLabel *namedLabel(const char *name, QWidget *parent)
{
    auto *label = new QLabel(parent);
    label->setObjectName(QString::fromLatin1(name));
    return label;
}

QComboBox *namedCombo(const char *name, QWidget *parent)
{
    auto *combo = new QComboBox(parent);
    combo->setObjectName(QString::fromLatin1(name));
    return combo;
}
} // namespace

PropertiesPanel::PropertiesPanel(Project &project, QUndoStack &undoStack, EditorScene &scene,
                                 QWidget *parent)
    : QWidget(parent)
    , m_project(project)
    , m_undoStack(undoStack)
    , m_scene(scene)
{
    setObjectName(QStringLiteral("propertiesPanel"));
    auto *layout = new QVBoxLayout(this);

    m_hint = namedLabel("propertiesHint", this);
    m_hint->setWordWrap(true);
    m_hint->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    layout->addWidget(m_hint);

    m_deviceBox = new QWidget(this);
    auto *deviceLayout = new QVBoxLayout(m_deviceBox);
    deviceLayout->setContentsMargins(0, 0, 0, 0);
    m_designation = namedLabel("designationLabel", m_deviceBox);
    QFont bold = m_designation->font();
    bold.setBold(true);
    m_designation->setFont(bold);
    m_typeName = namedLabel("typeNameLabel", m_deviceBox);
    m_typeName->setWordWrap(true);
    deviceLayout->addWidget(m_designation);
    deviceLayout->addWidget(m_typeName);
    m_deviceForm = new QFormLayout;
    m_deviceForm->setRowWrapPolicy(QFormLayout::WrapAllRows);
    deviceLayout->addLayout(m_deviceForm);
    m_ratingCaption = new QLabel(m_deviceBox);
    m_rating = namedCombo("ratingCombo", m_deviceBox);
    m_deviceForm->addRow(m_ratingCaption, m_rating);
    // The pole count is a drop-down when the type has several variants, otherwise plain text.
    m_polesCaption = new QLabel(m_deviceBox);
    auto *polesField = new QWidget(m_deviceBox);
    auto *polesLayout = new QVBoxLayout(polesField);
    polesLayout->setContentsMargins(0, 0, 0, 0);
    m_poles = namedCombo("polesCombo", polesField);
    m_polesValue = namedLabel("polesValueLabel", polesField);
    polesLayout->addWidget(m_poles);
    polesLayout->addWidget(m_polesValue);
    m_deviceForm->addRow(m_polesCaption, polesField);
    m_curveCaption = new QLabel(m_deviceBox);
    m_curve = namedLabel("curveValueLabel", m_deviceBox);
    m_deviceForm->addRow(m_curveCaption, m_curve);
    m_residualCaption = new QLabel(m_deviceBox);
    m_residual = namedLabel("residualValueLabel", m_deviceBox);
    m_deviceForm->addRow(m_residualCaption, m_residual);
    layout->addWidget(m_deviceBox);

    m_loadBox = new QWidget(this);
    m_loadBox->setObjectName(QStringLiteral("loadBox"));
    auto *loadLayout = new QVBoxLayout(m_loadBox);
    loadLayout->setContentsMargins(0, 0, 0, 0);
    m_loadName = namedLabel("loadNameLabel", m_loadBox);
    m_loadName->setFont(bold);
    m_loadType = namedLabel("loadTypeLabel", m_loadBox);
    m_loadType->setWordWrap(true);
    loadLayout->addWidget(m_loadName);
    loadLayout->addWidget(m_loadType);
    m_loadForm = new QFormLayout;
    m_loadForm->setRowWrapPolicy(QFormLayout::WrapAllRows);
    loadLayout->addLayout(m_loadForm);
    m_powerCaption = new QLabel(m_loadBox);
    m_power = new QSpinBox(m_loadBox);
    m_power->setObjectName(QStringLiteral("powerSpin"));
    m_power->setRange(1, MaxLoadPower);
    m_power->setKeyboardTracking(false);
    m_loadForm->addRow(m_powerCaption, m_power);
    // Wrapped labels go below the forms: inside a QFormLayout one squeezes the rows above it.
    m_plugStatus = namedLabel("plugStatusLabel", m_loadBox);
    m_plugStatus->setWordWrap(true);
    loadLayout->addWidget(m_plugStatus);
    m_socketContentsCaption = new QLabel(m_loadBox);
    m_socketContents = namedLabel("socketContentsLabel", m_loadBox);
    m_socketContents->setWordWrap(true);
    loadLayout->addWidget(m_socketContentsCaption);
    loadLayout->addWidget(m_socketContents);
    layout->addWidget(m_loadBox);

    m_wireBox = new QWidget(this);
    auto *wireLayout = new QVBoxLayout(m_wireBox);
    wireLayout->setContentsMargins(0, 0, 0, 0);
    m_wireTitle = new QLabel(m_wireBox);
    m_wireTitle->setFont(bold);
    m_wireEnds = namedLabel("wireEndsLabel", m_wireBox);
    m_wireEnds->setWordWrap(true);
    wireLayout->addWidget(m_wireTitle);
    wireLayout->addWidget(m_wireEnds);
    m_wireForm = new QFormLayout;
    m_wireForm->setRowWrapPolicy(QFormLayout::WrapAllRows);
    wireLayout->addLayout(m_wireForm);
    m_sectionCaption = new QLabel(m_wireBox);
    m_section = namedCombo("sectionCombo", m_wireBox);
    m_wireForm->addRow(m_sectionCaption, m_section);
    m_lengthCaption = new QLabel(m_wireBox);
    m_length = new QDoubleSpinBox(m_wireBox);
    m_length->setObjectName(QStringLiteral("lengthSpin"));
    m_length->setDecimals(1);
    m_length->setRange(0.5, MaxWireLength);
    m_length->setSingleStep(0.5);
    m_length->setKeyboardTracking(false);
    m_wireForm->addRow(m_lengthCaption, m_length);
    m_cableNote = namedLabel("cableNoteLabel", m_wireBox);
    m_cableNote->setWordWrap(true);
    wireLayout->addWidget(m_cableNote);
    layout->addWidget(m_wireBox);

    m_enclosureBox = new QWidget(this);
    auto *enclosureLayout = new QVBoxLayout(m_enclosureBox);
    enclosureLayout->setContentsMargins(0, 0, 0, 0);
    m_enclosureTitle = new QLabel(m_enclosureBox);
    m_enclosureTitle->setFont(bold);
    enclosureLayout->addWidget(m_enclosureTitle);
    auto *enclosureForm = new QFormLayout;
    enclosureForm->setRowWrapPolicy(QFormLayout::WrapAllRows);
    enclosureLayout->addLayout(enclosureForm);
    m_sizeCaption = new QLabel(m_enclosureBox);
    m_size = namedCombo("enclosureSizeCombo", m_enclosureBox);
    for (const EnclosureType &enclosure : m_project.catalog().enclosures)
        m_size->addItem(QString::number(enclosure.modules), enclosure.id);
    enclosureForm->addRow(m_sizeCaption, m_size);
    layout->addWidget(m_enclosureBox);
    layout->addStretch();
    m_wireBox->setObjectName(QStringLiteral("wireBox"));

    connect(m_rating, &QComboBox::activated, this, [this](int index) {
        const int current = m_rating->itemData(index).toInt();
        editDevice(
                [current](PlacedDevice &device) { device.parameters["ratedCurrent"] = current; });
    });
    connect(m_poles, &QComboBox::activated, this, [this](int index) {
        const QString poles = m_poles->itemText(index);
        editDevice([poles](PlacedDevice &device) { device.poles = poles; });
    });
    connect(m_size, &QComboBox::activated, this, &PropertiesPanel::changeSize);
    connect(m_section, &QComboBox::activated, this, &PropertiesPanel::changeSection);
    connect(m_power, &QSpinBox::valueChanged, this, &PropertiesPanel::changePower);
    connect(m_length, &QDoubleSpinBox::valueChanged, this, &PropertiesPanel::changeLength);
    // A value still being typed is sent to its object before the panel shows another one. Not on
    // Project::changed: that also comes from inside an undo, where a push would break the stack.
    connect(&m_scene, &QGraphicsScene::selectionChanged, this, [this] {
        m_power->interpretText();
        m_length->interpretText();
        refresh();
    });
    connect(&m_project, &Project::changed, this, &PropertiesPanel::refresh);
    retranslate();
}

void PropertiesPanel::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::LanguageChange)
        retranslate();
    QWidget::changeEvent(event);
}

void PropertiesPanel::retranslate()
{
    m_hint->setText(tr("Select a device on the panel to see its properties."));
    m_enclosureTitle->setText(tr("Enclosure"));
    m_wireTitle->setText(tr("Wire"));
    m_sectionCaption->setText(tr("Section"));
    m_lengthCaption->setText(tr("Length"));
    m_length->setSuffix(tr(" m"));
    m_length->setLocale(QLocale(languageCode(currentLanguage()))); // "15,0 м", as sectionText()
    m_cableNote->setText(tr("Applies to the whole cable: every wire between the same two places."));
    m_powerCaption->setText(tr("Power"));
    m_power->setSuffix(tr(" W"));
    m_socketContentsCaption->setText(tr("Plugged in"));
    m_ratingCaption->setText(tr("Rating"));
    m_polesCaption->setText(tr("Poles"));
    m_curveCaption->setText(tr("Curve"));
    m_residualCaption->setText(tr("Residual current"));
    m_sizeCaption->setText(tr("Size, modules"));
    refresh();
}

// Fills the widgets with signals blocked, so only user actions reach the activated handlers.
void PropertiesPanel::refresh()
{
    const PlacedDevice *device = m_project.device(m_scene.selectedDeviceId());
    const DeviceType *type = device ? m_project.catalog().deviceType(device->type) : nullptr;
    const Wire *wire = m_project.wire(m_scene.selectedWireId());
    const PlacedLoad *load = m_project.load(m_scene.selectedLoadId());
    const bool enclosure = !type && !wire && !load && m_scene.isEnclosureSelected();
    m_hint->setVisible(!type && !wire && !load && !enclosure);
    m_deviceBox->setVisible(type);
    m_loadBox->setVisible(load);
    m_wireBox->setVisible(wire);
    m_enclosureBox->setVisible(enclosure);
    // Only now: hiding a field that has the focus sends its typed value, which goes to these.
    m_loadId.clear();
    m_wireId.clear();

    if (type) {
        m_designation->setText(m_project.designation(device->id));
        m_typeName->setText(catalogText(type->name));
        const QSignalBlocker ratingBlocker(m_rating);
        m_rating->clear();
        for (const auto &value : type->allowedValues["ratedCurrent"].toArray())
            m_rating->addItem(QStringLiteral("%1 A").arg(value.toInt()), value.toInt());
        m_rating->setCurrentIndex(m_rating->findData(device->parameters["ratedCurrent"].toInt()));

        const bool severalPoles = type->variants.size() > 1;
        const QSignalBlocker polesBlocker(m_poles);
        m_poles->clear();
        for (const DeviceVariant &variant : type->variants)
            m_poles->addItem(variant.poles);
        m_poles->setCurrentText(device->poles);
        m_poles->setVisible(severalPoles);
        m_polesValue->setText(device->poles);
        m_polesValue->setVisible(!severalPoles);

        const bool hasCurve = device->parameters.contains("curve");
        m_curve->setText(device->parameters["curve"].toString());
        m_deviceForm->setRowVisible(m_curve, hasCurve);
        const bool hasResidual = device->parameters.contains("residualCurrent");
        m_residual->setText(
                QStringLiteral("%1 mA").arg(device->parameters["residualCurrent"].toInt()));
        m_deviceForm->setRowVisible(m_residual, hasResidual);
    } else if (load) {
        m_loadId = load->id;
        const LoadType *loadType = m_project.catalog().loadType(load->type);
        m_loadName->setText(loadName(*load));
        m_loadType->setText(catalogText(loadType->name));
        m_loadType->setVisible(!loadType->plugsIn); // an appliance's name is already its type name
        const bool isSocket = !loadType->places.isEmpty();
        m_loadForm->setRowVisible(m_power, !isSocket);
        const QSignalBlocker powerBlocker(m_power);
        m_power->setValue(load->power);

        m_plugStatus->setVisible(loadType->plugsIn);
        const PlacedLoad *socket = m_project.load(load->socket);
        m_plugStatus->setText(socket ? tr("Plugged into %1").arg(loadName(*socket))
                                     : tr("Not plugged in"));
        m_socketContentsCaption->setVisible(isSocket);
        m_socketContents->setVisible(isSocket);
        QStringList appliances;
        for (const QString &id : m_project.pluggedInto(load->id)) {
            const PlacedLoad *appliance = m_project.load(id);
            appliances
                    << tr("%1, %2 W").arg(loadName(*appliance), QString::number(appliance->power));
        }
        m_socketContents->setText(appliances.isEmpty() ? tr("Nothing plugged in")
                                                       : appliances.join(QLatin1Char('\n')));
    } else if (wire) {
        m_wireId = wire->id;
        m_wireEnds->setText(terminalText(wire->from) + QStringLiteral(" \u2192 ")
                            + terminalText(wire->to));
        const QSignalBlocker sectionBlocker(m_section);
        m_section->clear();
        for (double value : WireSections)
            m_section->addItem(sectionText(value), value);
        m_section->setCurrentIndex(m_section->findData(wire->section));

        const bool toLoad = m_project.load(terminalObject(wire->from))
                || m_project.load(terminalObject(wire->to));
        m_wireForm->setRowVisible(m_length, toLoad);
        m_cableNote->setVisible(toLoad);
        const QSignalBlocker lengthBlocker(m_length);
        m_length->setValue(wire->length);
    } else if (enclosure) {
        const QSignalBlocker sizeBlocker(m_size);
        m_size->setCurrentIndex(m_size->findData(m_project.enclosure().id));
    }
}

QString PropertiesPanel::terminalText(const QString &terminalId) const
{
    const QString object = terminalObject(terminalId);
    QString name;
    if (object == QLatin1String("supply"))
        name = tr("Supply");
    else if (object == QLatin1String("nbar"))
        name = tr("N bar");
    else if (object == QLatin1String("pebar"))
        name = tr("PE bar");
    else if (const PlacedLoad *load = m_project.load(object))
        name = loadName(*load);
    else
        name = m_project.designation(object);
    return name + QLatin1Char(' ') + terminalName(terminalId);
}

QString PropertiesPanel::loadName(const PlacedLoad &load) const
{
    const LoadType *type = m_project.catalog().loadType(load.type);
    return type->plugsIn ? catalogText(type->name) : loadCaption(m_project, load, *type);
}

void PropertiesPanel::editDevice(const std::function<void(PlacedDevice &)> &change)
{
    const PlacedDevice *current = m_project.device(m_scene.selectedDeviceId());
    if (!current)
        return;
    PlacedDevice after = *current;
    change(after);
    if (after == *current)
        return; // the value already shown was chosen again
    const QStringList orphaned = m_project.orphanedTerminals(after);
    if (!orphaned.isEmpty())
        emit hint(tr("Remove the wires from terminals %1 first.").arg(orphaned.join(", ")));
    else if (m_project.placement(after) == Placement::Fits)
        m_undoStack.push(new UpdateDeviceCommand(m_project, after));
    else
        emit hint(tr("Not enough room next to the device for more poles."));
    refresh();
}

void PropertiesPanel::changeSize(int index)
{
    const QString id = m_size->itemData(index).toString();
    if (id == m_project.enclosure().id)
        return;
    if (m_project.fitsEnclosure(id))
        m_undoStack.push(new SetEnclosureCommand(m_project, id));
    else if (!m_project.keepsBarWires(id))
        emit hint(tr("Remove the wires from the bar holes that would be removed first."));
    else if (!m_project.keepsLoadsClear(id))
        emit hint(tr("Move the loads below the enclosure first: they would run into other loads."));
    else
        emit hint(tr("Move or delete the devices on the rows that would be removed first."));
    refresh();
}

void PropertiesPanel::changeSection(int index)
{
    const Wire *current = m_project.wire(m_scene.selectedWireId());
    if (!current)
        return;
    Wire after = *current;
    after.section = m_section->itemData(index).toDouble();
    if (after != *current)
        m_undoStack.push(new UpdateWireCommand(m_project, after));
    refresh();
}

void PropertiesPanel::changePower(int power)
{
    const PlacedLoad *current = m_project.load(m_loadId);
    if (!current)
        return;
    PlacedLoad after = *current;
    after.power = power;
    if (after != *current && m_project.loadPlacement(after) == LoadPlacement::Fits)
        m_undoStack.push(new UpdateLoadCommand(m_project, after));
    refresh();
}

void PropertiesPanel::changeLength(double length)
{
    const Wire *current = m_project.wire(m_wireId);
    if (!current)
        return;
    if (current->length != length)
        m_undoStack.push(new SetCableLengthCommand(m_project, m_wireId, length));
    refresh();
}
