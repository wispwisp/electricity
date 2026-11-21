// SPDX-License-Identifier: GPL-3.0-or-later
#include "app/propertiespanel.h"

#include "app/editcommands.h"
#include "app/editorscene.h"
#include "core/catalog.h"
#include "core/project.h"

#include <QComboBox>
#include <QCoreApplication>
#include <QEvent>
#include <QFormLayout>
#include <QJsonArray>
#include <QLabel>
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
    connect(&m_scene, &QGraphicsScene::selectionChanged, this, &PropertiesPanel::refresh);
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
    const bool enclosure = !type && m_scene.isEnclosureSelected();
    m_hint->setVisible(!type && !enclosure);
    m_deviceBox->setVisible(type);
    m_enclosureBox->setVisible(enclosure);

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
    } else if (enclosure) {
        const QSignalBlocker sizeBlocker(m_size);
        m_size->setCurrentIndex(m_size->findData(m_project.enclosure().id));
    }
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
    if (m_project.placement(after) == Placement::Fits)
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
    else
        emit hint(tr("Move or delete the devices on the rows that would be removed first."));
    refresh();
}
