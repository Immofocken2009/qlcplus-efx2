/*
  Q Light Controller
  efxeditor.cpp

  Copyright (c) Heikki Junnila

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
*/

#include <QTreeWidgetItem>
#include <QTreeWidget>
#include <QMessageBox>
#include <QPaintEvent>
#include <QSettings>
#include <QComboBox>
#include <QCheckBox>
#include <QLineEdit>
#include <QCheckBox>
#include <QSpinBox>
#include <QGridLayout>
#include <QPainter>
#include <QLabel>
#include <QDebug>
#include <QPen>

#include "fixtureselection.h"
#include "speeddialwidget.h"
#include "efxpreviewarea.h"
#include "efxeditor.h"
#include "efxeffect.h"
#include "fixture.h"
#include "doc.h"
#include "simulatorclient.h"
#include "efxlibrary.h"

#include <QTableWidget>
#include <QTableWidgetItem>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFormLayout>
#include <QPushButton>
#include <QJsonObject>
#include <QJsonArray>
#include <QJsonDocument>

#define SETTINGS_GEOMETRY "efxeditor/geometry"

#define KColumnNumber  0
#define KColumnName    1
#define KColumnMode    2
#define KColumnReverse 3
#define KColumnStartOffset 4

#define PROPERTY_FIXTURE "fixture"
#define UI_STATE_TAB_INDEX "tabIndex"
#define UI_STATE_SHOW_DIAL "showDial"

#define KTabGeneral 0
#define KTabMovement 1

/*****************************************************************************
 * Initialization
 *****************************************************************************/

EFXEditor::EFXEditor(QWidget* parent, EFX* efx, Doc* doc)
    : QWidget(parent)
    , m_doc(doc)
    , m_efx(efx)
    , m_previewArea(NULL)
    , m_points(NULL)
    , m_speedDials(NULL)
{
    Q_ASSERT(doc != NULL);
    Q_ASSERT(efx != NULL);

    setupUi(this);

    connect(m_speedDial, SIGNAL(toggled(bool)),
            this, SLOT(slotSpeedDialToggle(bool)));

    initGeneralPage();
    initMovementPage();
    initSimulatorTab();

    QVariant tabIndex = efx->uiStateValue(UI_STATE_TAB_INDEX);
    if (tabIndex.isNull())
        m_tab->setCurrentIndex(0);
    else
        m_tab->setCurrentIndex(tabIndex.toInt());

    /* Tab widget */
    connect(m_tab, SIGNAL(currentChanged(int)),
            this, SLOT(slotTabChanged(int)));

    // Used for UI parameter changes
    m_testTimer.setSingleShot(true);
    m_testTimer.setInterval(500);
    connect(&m_testTimer, SIGNAL(timeout()), this, SLOT(slotRestartTest()));
    connect(m_doc, SIGNAL(modeChanged(Doc::Mode)), this, SLOT(slotModeChanged(Doc::Mode)));

    updateSpeedDials();

    QVariant showDial = efx->uiStateValue(UI_STATE_SHOW_DIAL);
    if (showDial.isNull() == false && showDial.toBool() == true)
        m_speedDial->setChecked(true);

    // Set focus to the editor
    m_nameEdit->setFocus();
}

EFXEditor::~EFXEditor()
{
    if (m_testButton->isChecked() == true)
        m_efx->stopAndWait();
}

void EFXEditor::stopTest()
{
    if (m_testButton->isChecked() == true)
        m_testButton->click();
}

void EFXEditor::slotFunctionManagerActive(bool active)
{
    if (active == true)
    {
        updateSpeedDials();
    }
    else
    {
        if (m_speedDials != NULL)
            m_speedDials->deleteLater();
        m_speedDials = NULL;
    }
}

void EFXEditor::initGeneralPage()
{
    // Doc
    connect(m_doc, SIGNAL(fixtureRemoved(quint32)), this, SLOT(slotFixtureRemoved()));
    connect(m_doc, SIGNAL(fixtureChanged(quint32)), this, SLOT(slotFixtureChanged()));

    /* Set the EFX's name to the name field */
    m_nameEdit->setText(m_efx->name());
    m_nameEdit->setSelection(0, m_nameEdit->text().length());

    /* Put all of the EFX's fixtures to the tree view */
    updateFixtureTree();

    /* Set propagation mode */
    if (m_efx->propagationMode() == EFX::Serial)
        m_serialRadio->setChecked(true);
    else if (m_efx->propagationMode() == EFX::Asymmetric)
        m_asymmetricRadio->setChecked(true);
    else
        m_parallelRadio->setChecked(true);

    /* Disable test button if we're in operate mode */
    if (m_doc->mode() == Doc::Operate)
        m_testButton->setEnabled(false);

    connect(m_nameEdit, SIGNAL(textEdited(const QString&)),
            this, SLOT(slotNameEdited(const QString&)));

    connect(m_tree, SIGNAL(itemChanged(QTreeWidgetItem*,int)),
            this, SLOT(slotFixtureItemChanged(QTreeWidgetItem*,int)));

    connect(m_addFixtureButton, SIGNAL(clicked()),
            this, SLOT(slotAddFixtureClicked()));
    connect(m_removeFixtureButton, SIGNAL(clicked()),
            this, SLOT(slotRemoveFixtureClicked()));

    connect(m_raiseFixtureButton, SIGNAL(clicked()),
            this, SLOT(slotRaiseFixtureClicked()));
    connect(m_lowerFixtureButton, SIGNAL(clicked()),
            this, SLOT(slotLowerFixtureClicked()));

    connect(m_parallelRadio, SIGNAL(toggled(bool)),
            this, SLOT(slotParallelRadioToggled(bool)));
    connect(m_serialRadio, SIGNAL(toggled(bool)),
            this, SLOT(slotSerialRadioToggled(bool)));
    connect(m_asymmetricRadio, SIGNAL(toggled(bool)),
            this, SLOT(slotAsymmetricRadioToggled(bool)));

    // Test slots
    connect(m_testButton, SIGNAL(clicked()),
            this, SLOT(slotTestClicked()));
    connect(m_raiseFixtureButton, SIGNAL(clicked()),
            this, SLOT(slotRestartTest()));
    connect(m_lowerFixtureButton, SIGNAL(clicked()),
            this, SLOT(slotRestartTest()));
    connect(m_parallelRadio, SIGNAL(toggled(bool)),
            this, SLOT(slotRestartTest()));
    connect(m_serialRadio, SIGNAL(toggled(bool)),
            this, SLOT(slotRestartTest()));
    connect(m_asymmetricRadio, SIGNAL(toggled(bool)),
            this, SLOT(slotRestartTest()));
}

void EFXEditor::initMovementPage()
{
    new QHBoxLayout(m_previewFrame);
    m_previewArea = new EFXPreviewArea(m_previewFrame);
    m_previewFrame->layout()->setContentsMargins(0, 0, 0, 0);
    m_previewFrame->layout()->addWidget(m_previewArea);

    /* Fill the algorithm combo from the EFXLibrary */
    m_effectIds.clear();
    for (const EFXEffectPtr &e : EFXLibrary::instance()->effects())
    {
        m_algorithmCombo->addItem(e->name());
        m_effectIds.append(e->id());
    }
    int algoIndex = m_effectIds.indexOf(m_efx->effectId());
    if (algoIndex < 0) algoIndex = 0;
    m_algorithmCombo->setCurrentIndex(algoIndex);

    /* Causes the EFX function to update the preview point array */
    slotAlgorithmSelected(algoIndex);

    /* Get the algorithm parameters */
    m_widthSpin->setValue(m_efx->width());
    m_heightSpin->setValue(m_efx->height());
    m_xOffsetSpin->setValue(m_efx->xOffset());
    m_yOffsetSpin->setValue(m_efx->yOffset());
    m_rotationSpin->setValue(m_efx->rotation());
    m_startOffsetSpin->setValue(m_efx->startOffset());
    m_isRelativeCheckbox->setChecked(m_efx->isRelative());

    m_xFrequencySpin->setValue((int)m_efx->effectParam("xFrequency"));
    m_yFrequencySpin->setValue((int)m_efx->effectParam("yFrequency"));
    m_xPhaseSpin->setValue((int)m_efx->effectParam("xPhase"));
    m_yPhaseSpin->setValue((int)m_efx->effectParam("yPhase"));

    /* Dimmer control. A single checkbox, not in the .ui file: when enabled the
       EFX drives each fixture's dimmer from its own tilt (full at maximum tilt,
       zero at minimum). Off by default. */
    QGridLayout *paramsLayout = qobject_cast<QGridLayout*>(m_parametersGroup->layout());
    if (paramsLayout != NULL)
    {
        int dimRow = paramsLayout->rowCount();

        m_dimmerControlCheck = new QCheckBox(tr("Enable dimmer control"), m_parametersGroup);
        m_dimmerControlCheck->setChecked(m_efx->dimmerControlEnabled());
        paramsLayout->addWidget(m_dimmerControlCheck, dimRow, 0, 1, 2);

        connect(m_dimmerControlCheck, SIGNAL(toggled(bool)),
                this, SLOT(slotDimmerControlToggled(bool)));
    }

    /* Running order */
    switch (m_efx->runOrder())
    {
        default:
        case Function::Loop:
            m_loop->setChecked(true);
        break;
        case Function::SingleShot:
            m_singleShot->setChecked(true);
        break;
        case Function::PingPong:
            m_pingPong->setChecked(true);
        break;
    }

    /* Direction */
    switch (m_efx->direction())
    {
        default:
        case Function::Forward:
            m_forward->setChecked(true);
        break;
        case Function::Backward:
            m_backward->setChecked(true);
        break;
    }

    connect(m_loop, SIGNAL(clicked()),
            this, SLOT(slotLoopClicked()));
    connect(m_singleShot, SIGNAL(clicked()),
            this, SLOT(slotSingleShotClicked()));
    connect(m_pingPong, SIGNAL(clicked()),
            this, SLOT(slotPingPongClicked()));

    connect(m_forward, SIGNAL(clicked()),
            this, SLOT(slotForwardClicked()));
    connect(m_backward, SIGNAL(clicked()),
            this, SLOT(slotBackwardClicked()));

    connect(m_algorithmCombo, SIGNAL(activated(int)),
            this, SLOT(slotAlgorithmSelected(int)));
    connect(m_widthSpin, SIGNAL(valueChanged(int)),
            this, SLOT(slotWidthSpinChanged(int)));
    connect(m_heightSpin, SIGNAL(valueChanged(int)),
            this, SLOT(slotHeightSpinChanged(int)));
    connect(m_xOffsetSpin, SIGNAL(valueChanged(int)),
            this, SLOT(slotXOffsetSpinChanged(int)));
    connect(m_yOffsetSpin, SIGNAL(valueChanged(int)),
            this, SLOT(slotYOffsetSpinChanged(int)));
    connect(m_rotationSpin, SIGNAL(valueChanged(int)),
            this, SLOT(slotRotationSpinChanged(int)));
    connect(m_startOffsetSpin, SIGNAL(valueChanged(int)),
            this, SLOT(slotStartOffsetSpinChanged(int)));
    connect(m_isRelativeCheckbox, SIGNAL(stateChanged(int)),
            this, SLOT(slotIsRelativeCheckboxChanged(int)));

    connect(m_xFrequencySpin, SIGNAL(valueChanged(int)),
            this, SLOT(slotXFrequencySpinChanged(int)));
    connect(m_yFrequencySpin, SIGNAL(valueChanged(int)),
            this, SLOT(slotYFrequencySpinChanged(int)));
    connect(m_xPhaseSpin, SIGNAL(valueChanged(int)),
            this, SLOT(slotXPhaseSpinChanged(int)));
    connect(m_yPhaseSpin, SIGNAL(valueChanged(int)),
            this, SLOT(slotYPhaseSpinChanged(int)));

    connect(m_colorCheck, SIGNAL(toggled(bool)),
            this, SLOT(slotSetColorBackground(bool)));

    redrawPreview();
}

void EFXEditor::slotTestClicked()
{
    if (m_testButton->isChecked() == true)
    {
        m_efx->start(m_doc->masterTimer(), functionParent());

        //Restart animation so preview it is in sync with real test
        m_previewArea->restart();
    }
    else
        m_efx->stopAndWait();
}

void EFXEditor::slotRestartTest()
{
    if (m_testButton->isChecked() == true)
    {
        // Toggle off, toggle on. Duh.
        m_testButton->click();
        m_testButton->click();
    }
}

void EFXEditor::slotModeChanged(Doc::Mode mode)
{
    if (mode == Doc::Operate)
    {
        m_efx->stopAndWait();
        m_testButton->setChecked(false);
        m_testButton->setEnabled(false);
    }
    else
    {
        m_testButton->setEnabled(true);
    }
}

void EFXEditor::slotTabChanged(int tab)
{
    m_efx->setUiStateValue(UI_STATE_TAB_INDEX, tab);

    //When preview animation is opened restart animation but avoid restart if test is running.
    if (tab == 1 && (m_testButton->isChecked () == false))
        m_previewArea->restart ();
}

void EFXEditor::slotSetColorBackground(bool checked)
{
    m_previewArea->showGradientBackground(checked);
}

bool EFXEditor::interruptRunning()
{
    if (m_testButton->isChecked() == true)
    {
        m_efx->stopAndWait();
        m_testButton->setChecked(false);
        return true;
    }
    else
    {
        return false;
    }
}

void EFXEditor::continueRunning(bool running)
{
    if (running == true)
    {
        if (m_doc->mode() == Doc::Operate)
            m_efx->start(m_doc->masterTimer(), functionParent());
        else
            m_testButton->click();
    }
}

FunctionParent EFXEditor::functionParent() const
{
    return FunctionParent::master();
}

/*****************************************************************************
 * General page
 *****************************************************************************/

void EFXEditor::updateFixtureTree()
{
    m_tree->clear();
    QListIterator <EFXFixture*> it(m_efx->fixtures());
    while (it.hasNext() == true)
        addFixtureItem(it.next());
    m_tree->header()->resizeSections(QHeaderView::ResizeToContents);
}

QTreeWidgetItem* EFXEditor::fixtureItem(EFXFixture* ef)
{
    QTreeWidgetItemIterator it(m_tree);
    while (*it != NULL)
    {
        QTreeWidgetItem* item = *it;
        EFXFixture* ef_item = reinterpret_cast<EFXFixture*>
                              (item->data(0, Qt::UserRole).toULongLong());
        if (ef_item == ef)
            return item;
        ++it;
    }

    return NULL;
}

const QList <EFXFixture*> EFXEditor::selectedFixtures() const
{
    QListIterator <QTreeWidgetItem*> it(m_tree->selectedItems());
    QList <EFXFixture*> list;

    /* Put all selected fixture IDs to a list and return it */
    while (it.hasNext() == true)
    {
        EFXFixture* ef = reinterpret_cast <EFXFixture*>
                         (it.next()->data(0, Qt::UserRole).toULongLong());
        list << ef;
    }

    return list;
}

void EFXEditor::updateIndices(int from, int to)
{
    for (int i = from; i <= to; i++)
    {
        QTreeWidgetItem *item = m_tree->topLevelItem(i);
        Q_ASSERT(item != NULL);

        item->setText(KColumnNumber,
                      QString("%1").arg(i + 1, 3, 10, QChar('0')));
    }
}

void EFXEditor::addFixtureItem(EFXFixture* ef)
{
    QTreeWidgetItem* item;
    Fixture* fxi;

    Q_ASSERT(ef != NULL);

    fxi = m_doc->fixture(ef->head().fxi);
    if (fxi == NULL)
        return;

    item = new QTreeWidgetItem(m_tree);

    if (fxi->heads() > 1)
    {
        item->setText(KColumnName, QString("%1 [%2]").arg(fxi->name()).arg(ef->head().head));
    }
    else
    {
        item->setText(KColumnName, fxi->name());
    }
    item->setData(0, Qt::UserRole, QVariant(reinterpret_cast<qulonglong> (ef)));
    item->setFlags(item->flags() | Qt::ItemIsUserCheckable);

    if (ef->direction() == Function::Backward)
        item->setCheckState(KColumnReverse, Qt::Checked);
    else
        item->setCheckState(KColumnReverse, Qt::Unchecked);

    updateModeColumn(item, ef);
    updateStartOffsetColumn(item, ef);

    updateIndices(m_tree->indexOfTopLevelItem(item),
                  m_tree->topLevelItemCount() - 1);

    /* Select newly-added fixtures so that they can be moved quickly */
    m_tree->setCurrentItem(item);
}

void EFXEditor::updateModeColumn(QTreeWidgetItem* item, EFXFixture* ef)
{
    Q_ASSERT(item != NULL);
    Q_ASSERT(ef != NULL);

    if (m_tree->itemWidget(item, KColumnMode) == NULL)
    {
        QComboBox* combo = new QComboBox(m_tree);
        combo->setAutoFillBackground(true);
        combo->addItems(ef->modeList());
        combo->setProperty(PROPERTY_FIXTURE, (qulonglong) ef);
        m_tree->setItemWidget(item, KColumnMode, combo);

        const int index = combo->findText(ef->modeToString(ef->mode()));
        combo->setCurrentIndex(index);

        connect(combo, SIGNAL(currentIndexChanged(int)),
                this, SLOT(slotFixtureModeChanged(int)));
    }
}

void EFXEditor::updateStartOffsetColumn(QTreeWidgetItem* item, EFXFixture* ef)
{
    Q_ASSERT(item != NULL);
    Q_ASSERT(ef != NULL);

    if (m_tree->itemWidget(item, KColumnStartOffset) == NULL)
    {
        QSpinBox* spin = new QSpinBox(m_tree);
        spin->setAutoFillBackground(true);
        spin->setRange(0, 359);
        spin->setValue(ef->startOffset());
        spin->setSuffix(QChar(0x00b0)); // degree
        m_tree->setItemWidget(item, KColumnStartOffset, spin);
        spin->setProperty(PROPERTY_FIXTURE, (qulonglong) ef);
        connect(spin, SIGNAL(valueChanged(int)),
                this, SLOT(slotFixtureStartOffsetChanged(int)));
    }
}

void EFXEditor::removeFixtureItem(EFXFixture* ef)
{
    QTreeWidgetItem* item;
    int from;

    Q_ASSERT(ef != NULL);

    item = fixtureItem(ef);
    Q_ASSERT(item != NULL);

    from = m_tree->indexOfTopLevelItem(item);
    delete item;

    updateIndices(from, m_tree->topLevelItemCount() - 1);
    redrawPreview();

    m_tree->header()->resizeSections(QHeaderView::ResizeToContents);
}

void EFXEditor::slotDialDestroyed(QObject *)
{
    m_speedDial->setChecked(false);
}

void EFXEditor::createSpeedDials()
{
    if (m_speedDials == NULL)
    {
        m_speedDials = new SpeedDialWidget(this);
        m_speedDials->setAttribute(Qt::WA_DeleteOnClose);
        connect(m_speedDials, SIGNAL(fadeInChanged(int)), this, SLOT(slotFadeInChanged(int)));
        connect(m_speedDials, SIGNAL(fadeOutChanged(int)), this, SLOT(slotFadeOutChanged(int)));
        connect(m_speedDials, SIGNAL(holdChanged(int)), this, SLOT(slotHoldChanged(int)));
        connect(m_speedDials, SIGNAL(destroyed(QObject*)), this, SLOT(slotDialDestroyed(QObject*)));
    }

    m_speedDials->show();
}

void EFXEditor::updateSpeedDials()
{
   if (m_speedDial->isChecked() == false)
        return;

    createSpeedDials();

    m_speedDials->setWindowTitle(m_efx->name());
    m_speedDials->setFadeInSpeed(m_efx->fadeInSpeed());
    m_speedDials->setFadeOutSpeed(m_efx->fadeOutSpeed());
    if ((int)m_efx->duration() < 0)
        m_speedDials->setDuration(m_efx->duration());
    else
        m_speedDials->setDuration(m_efx->duration() - m_efx->fadeInSpeed() - m_efx->fadeOutSpeed());
}

void EFXEditor::slotNameEdited(const QString &text)
{
    m_efx->setName(text);
    if (m_speedDials)
        m_speedDials->setWindowTitle(text);
}

void EFXEditor::slotSpeedDialToggle(bool state)
{
    if (state == true)
    {
        updateSpeedDials();
    }
    else
    {
        if (m_speedDials != NULL)
            m_speedDials->deleteLater();
        m_speedDials = NULL;
    }

    m_efx->setUiStateValue(UI_STATE_SHOW_DIAL, state);
}

void EFXEditor::slotFixtureItemChanged(QTreeWidgetItem* item, int column)
{
    if (column == KColumnReverse)
    {
        EFXFixture* ef = reinterpret_cast <EFXFixture*>
                         (item->data(0, Qt::UserRole).toULongLong());
        Q_ASSERT(ef != NULL);

        if (item->checkState(column) == Qt::Checked)
            ef->setDirection(Function::Backward);
        else
            ef->setDirection(Function::Forward);

        redrawPreview();
    }
}

void EFXEditor::slotFixtureModeChanged(int index)
{
    QComboBox *combo = qobject_cast<QComboBox*>(QObject::sender());
    Q_ASSERT(combo != NULL);

    EFXFixture *ef = (EFXFixture*) combo->property(PROPERTY_FIXTURE).toULongLong();
    Q_ASSERT(ef != NULL);

    ef->setMode(ef->stringToMode (combo->itemText(index)));

    // Restart the test after the latest mode change, delayed
    m_testTimer.start();
}

void EFXEditor::slotFixtureStartOffsetChanged(int startOffset)
{
    QSpinBox *spin = qobject_cast<QSpinBox*>(QObject::sender());
    Q_ASSERT(spin != NULL);
    EFXFixture *ef = (EFXFixture*) spin->property(PROPERTY_FIXTURE).toULongLong();
    Q_ASSERT(ef != NULL);
    ef->setStartOffset(startOffset);

    redrawPreview();

    // Restart the test after the latest offset change, delayed
    m_testTimer.start();
}

void EFXEditor::slotAddFixtureClicked()
{
    /* The following code is the original QLC+ code (EFX with only Pan-Tilt).
     * Now, with modes, the same fixture could be duplicated. */

    /* Put all fixtures already present into a list of fixtures that
       will be disabled in the fixture selection dialog */
    QList <GroupHead> disabled;
    QTreeWidgetItemIterator twit(m_tree);
    /*
    while (*twit != NULL)
    {
        EFXFixture* ef = reinterpret_cast <EFXFixture*>
                         ((*twit)->data(0, Qt::UserRole).toULongLong());
        Q_ASSERT(ef != NULL);

        disabled.append(ef->head());
        twit++;
    }
    */

    /* Disable all fixtures that don't have pan OR tilt, dimmer or RGB channels */
    /*
    QListIterator <Fixture*> fxit(m_doc->fixtures());
    while (fxit.hasNext() == true)
    {
        Fixture* fixture(fxit.next());
        Q_ASSERT(fixture != NULL);

        // If a channel with pan or tilt group exists, don't disable this fixture
        if (fixture->channel(QLCChannel::Pan) == QLCChannel::invalid() &&
            fixture->channel(QLCChannel::Tilt) == QLCChannel::invalid())
        {
            // Disable all fixtures without pan or tilt channels
            disabled << fixture->id();
        }
        else
        {
            QVector <QLCFixtureHead> const& heads = fixture->fixtureMode()->heads();
            for (int i = 0; i < heads.size(); ++i)
            {
                if (heads[i].panMsbChannel() == QLCChannel::invalid() &&
                    heads[i].tiltMsbChannel() == QLCChannel::invalid() &&
                    heads[i].panLsbChannel() == QLCChannel::invalid() &&
                    heads[i].tiltLsbChannel() == QLCChannel::invalid())
                {
                    // Disable heads without pan or tilt channels
                    disabled << GroupHead(fixture->id(), i);
                }
            }
        }
    }
    */

    FixtureSelection fs(this, m_doc);
    fs.setMultiSelection(true);
    fs.setSelectionMode(FixtureSelection::Heads);
    fs.setDisabledHeads(disabled);
    if (fs.exec() == QDialog::Accepted)
    {
        // Stop running while adding fixtures
        bool running = interruptRunning();

        QListIterator <GroupHead> it(fs.selectedHeads());
        while (it.hasNext() == true)
        {
            EFXFixture* ef = new EFXFixture(m_efx);
            ef->setHead(it.next());

            if (m_efx->addFixture(ef) == true)
                addFixtureItem(ef);
            else
                delete ef;
        }

        m_tree->header()->resizeSections(QHeaderView::ResizeToContents);

        redrawPreview();

        // Continue running if appropriate
        continueRunning(running);
    }
}

void EFXEditor::slotRemoveFixtureClicked()
{
    int r = QMessageBox::question(
                this, tr("Remove fixtures"),
                tr("Do you want to remove the selected fixture(s)?"),
                QMessageBox::Yes, QMessageBox::No);

    if (r == QMessageBox::Yes)
    {
        // Stop running while removing fixtures
        bool running = interruptRunning();

        QListIterator <EFXFixture*> it(selectedFixtures());
        while (it.hasNext() == true)
        {
            EFXFixture* ef = it.next();
            Q_ASSERT(ef != NULL);

            removeFixtureItem(ef);
            if (m_efx->removeFixture(ef) == true)
                delete ef;
        }

        redrawPreview();

        // Continue if appropriate
        continueRunning(running);
    }
}

void EFXEditor::slotRaiseFixtureClicked()
{
    // Stop running while moving fixtures
    bool running = interruptRunning();

    QTreeWidgetItem* item = m_tree->currentItem();
    if (item != NULL)
    {
        int index = m_tree->indexOfTopLevelItem(item);
        if (index == 0)
            return;

        EFXFixture* ef = reinterpret_cast <EFXFixture*>
                         (item->data(0, Qt::UserRole).toULongLong());
        Q_ASSERT(ef != NULL);

        if (m_efx->raiseFixture(ef) == true)
        {
            item = m_tree->takeTopLevelItem(index);

            m_tree->insertTopLevelItem(index - 1, item);

            updateModeColumn(item, ef);
            updateStartOffsetColumn(item, ef);
            updateIndices(index - 1, index);
            m_tree->setCurrentItem(item);

            redrawPreview();
        }
    }

    // Continue running if appropriate
    continueRunning(running);
}

void EFXEditor::slotLowerFixtureClicked()
{
    // Stop running while moving fixtures
    bool running = interruptRunning();

    QTreeWidgetItem* item = m_tree->currentItem();
    if (item != NULL)
    {
        int index = m_tree->indexOfTopLevelItem(item);
        if (index == (m_tree->topLevelItemCount() - 1))
            return;

        EFXFixture* ef = reinterpret_cast <EFXFixture*>
                         (item->data(0, Qt::UserRole).toULongLong());
        Q_ASSERT(ef != NULL);

        if (m_efx->lowerFixture(ef) == true)
        {
            item = m_tree->takeTopLevelItem(index);
            m_tree->insertTopLevelItem(index + 1, item);

            updateModeColumn(item, ef);
            updateStartOffsetColumn(item, ef);
            updateIndices(index, index + 1);
            m_tree->setCurrentItem(item);

            redrawPreview();
        }
    }

    // Continue running if appropriate
    continueRunning(running);
}

void EFXEditor::slotParallelRadioToggled(bool state)
{
    Q_ASSERT(m_efx != NULL);
    if (state == true)
    {
        m_efx->setPropagationMode(EFX::Parallel);
        redrawPreview();
    }
}

void EFXEditor::slotSerialRadioToggled(bool state)
{
    Q_ASSERT(m_efx != NULL);
    if (state == true)
    {

        m_efx->setPropagationMode(EFX::Serial);
        redrawPreview();
    }
}

void EFXEditor::slotAsymmetricRadioToggled(bool state)
{
    Q_ASSERT(m_efx != NULL);
    if (state == true)
    {

        m_efx->setPropagationMode(EFX::Asymmetric);
        redrawPreview();
    }
}

void EFXEditor::slotFadeInChanged(int ms)
{
    m_efx->setFadeInSpeed(ms);
    slotRestartTest();
}

void EFXEditor::slotFadeOutChanged(int ms)
{
    m_efx->setFadeOutSpeed(ms);
}

void EFXEditor::slotHoldChanged(int ms)
{
    uint duration = 0;
    if (ms < 0)
        duration = ms;
    else
        duration = m_efx->fadeInSpeed() + ms + m_efx->fadeOutSpeed();
    m_efx->setDuration(duration);
    redrawPreview();
}

void EFXEditor::slotFixtureRemoved()
{
    // EFX already catches fixture removals so just update the list
    updateFixtureTree();
    redrawPreview();
}

void EFXEditor::slotFixtureChanged()
{
    // Update the tree in case fixture's name changes
    updateFixtureTree();
}

/*****************************************************************************
 * Movement page
 *****************************************************************************/

void EFXEditor::slotAlgorithmSelected(int algoIndex)
{
    Q_ASSERT(m_efx != NULL);

    if (algoIndex >= 0 && algoIndex < m_effectIds.size())
        m_efx->setEffect(m_effectIds[algoIndex]);

    bool hasFreq = hasEffectParam("xFrequency");
    m_xFrequencyLabel->setEnabled(hasFreq);
    m_yFrequencyLabel->setEnabled(hasFreq);
    m_xFrequencySpin->setEnabled(hasFreq);
    m_yFrequencySpin->setEnabled(hasFreq);

    bool hasPhase = hasEffectParam("xPhase");
    m_xPhaseLabel->setEnabled(hasPhase);
    m_yPhaseLabel->setEnabled(hasPhase);
    m_xPhaseSpin->setEnabled(hasPhase);
    m_yPhaseSpin->setEnabled(hasPhase);

    redrawPreview();
}

void EFXEditor::slotWidthSpinChanged(int value)
{
    Q_ASSERT(m_efx != NULL);
    m_efx->setWidth(value);
    redrawPreview();
}

void EFXEditor::slotHeightSpinChanged(int value)
{
    Q_ASSERT(m_efx != NULL);
    m_efx->setHeight(value);
    redrawPreview();
}

void EFXEditor::slotRotationSpinChanged(int value)
{
    Q_ASSERT(m_efx != NULL);
    m_efx->setRotation(value);
    redrawPreview();
}

void EFXEditor::slotStartOffsetSpinChanged(int value)
{
    Q_ASSERT(m_efx != NULL);
    m_efx->setStartOffset(value);
    redrawPreview();
}

void EFXEditor::slotIsRelativeCheckboxChanged(int value)
{
    Q_ASSERT(m_efx != NULL);
    m_efx->setIsRelative(value == Qt::Checked);
}

void EFXEditor::slotXOffsetSpinChanged(int value)
{
    Q_ASSERT(m_efx != NULL);
    m_efx->setXOffset(value);
    redrawPreview();
}

void EFXEditor::slotYOffsetSpinChanged(int value)
{
    Q_ASSERT(m_efx != NULL);
    m_efx->setYOffset(value);
    redrawPreview();
}

void EFXEditor::slotXFrequencySpinChanged(int value)
{
    Q_ASSERT(m_efx != NULL);
    m_efx->setEffectParam("xFrequency", value);
    redrawPreview();
}

void EFXEditor::slotYFrequencySpinChanged(int value)
{
    Q_ASSERT(m_efx != NULL);
    m_efx->setEffectParam("yFrequency", value);
    redrawPreview();
}

void EFXEditor::slotXPhaseSpinChanged(int value)
{
    Q_ASSERT(m_efx != NULL);
    m_efx->setEffectParam("xPhase", value);
    redrawPreview();
}

void EFXEditor::slotYPhaseSpinChanged(int value)
{
    Q_ASSERT(m_efx != NULL);
    m_efx->setEffectParam("yPhase", value);
    redrawPreview();
}

void EFXEditor::slotDimmerControlToggled(bool checked)
{
    Q_ASSERT(m_efx != NULL);
    m_efx->setDimmerControlEnabled(checked);
}

/*****************************************************************************
 * Run order
 *****************************************************************************/

void EFXEditor::slotLoopClicked()
{
    Q_ASSERT(m_efx != NULL);
    m_efx->setRunOrder(Function::Loop);
}

void EFXEditor::slotSingleShotClicked()
{
    Q_ASSERT(m_efx != NULL);
    m_efx->setRunOrder(Function::SingleShot);
}

void EFXEditor::slotPingPongClicked()
{
    Q_ASSERT(m_efx != NULL);
    m_efx->setRunOrder(Function::PingPong);
}

/*****************************************************************************
 * Direction
 *****************************************************************************/

void EFXEditor::slotForwardClicked()
{
    Q_ASSERT(m_efx != NULL);
    m_efx->setDirection(Function::Forward);
    redrawPreview();
}

void EFXEditor::slotBackwardClicked()
{
    Q_ASSERT(m_efx != NULL);
    m_efx->setDirection(Function::Backward);
    redrawPreview();
}

void EFXEditor::redrawPreview()
{
    if (m_previewArea == NULL)
        return;

    QPolygonF polygon;
    m_efx->preview(polygon);

    QVector <QPolygonF> fixturePoints;
    m_efx->previewFixtures(fixturePoints);

    m_previewArea->setPolygon(polygon);
    m_previewArea->setFixturePolygons(fixturePoints);

    m_previewArea->draw(m_efx->duration() / polygon.size());
}

/***********************************************************************
 * Simulator tab
 ***********************************************************************/

void EFXEditor::initSimulatorTab()
{
    SimulatorClient *sim = SimulatorClient::instance();

    m_simTab = new QWidget(this);
    m_tab->addTab(m_simTab, tr("Simulator"));

    // --- Scenario row ---
    m_simScenario = new QComboBox(m_simTab);
    auto *refreshBtn = new QPushButton(tr("↻"), m_simTab);
    refreshBtn->setFixedWidth(28);
    refreshBtn->setToolTip(tr("Szenarien laden"));

    auto *scenRow = new QHBoxLayout;
    scenRow->addWidget(m_simScenario, 1);
    scenRow->addWidget(refreshBtn);

    // --- Fixture → Spot mapping table ---
    m_simMapping = new QTableWidget(0, 2, m_simTab);
    m_simMapping->setHorizontalHeaderLabels({tr("Fixture"), tr("Spot")});
    m_simMapping->horizontalHeader()->setStretchLastSection(true);
    m_simMapping->verticalHeader()->setVisible(false);
    m_simMapping->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_simMapping->setSelectionMode(QAbstractItemView::NoSelection);

    // Populate mapping rows from current EFX fixtures
    for (EFXFixture *ef : m_efx->fixtures())
    {
        Fixture *fxi = m_doc->fixture(ef->head().fxi);
        int row = m_simMapping->rowCount();
        m_simMapping->insertRow(row);
        QString name = fxi ? fxi->name() : tr("Fixture %1").arg(ef->head().fxi);
        m_simMapping->setItem(row, 0, new QTableWidgetItem(name));
        auto *combo = new QComboBox;
        combo->addItem(tr("(nicht zugeordnet)"), -1);
        m_simMapping->setCellWidget(row, 1, combo);
        combo->setProperty("efx_row", row);
        connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                [this, row](int) { slotSimulatorSpotChanged(row, 1); });
    }

    // --- Options row ---
    m_simSamples = new QSpinBox(m_simTab);
    m_simSamples->setRange(8, 512);
    m_simSamples->setValue(64);
    m_simSamples->setSuffix(tr(" Punkte"));

    auto *form = new QFormLayout;
    form->addRow(tr("Szenario:"), scenRow);
    form->addRow(tr("Auflösung:"), m_simSamples);

    // --- Calculate button + status ---
    m_simCalcBtn = new QPushButton(tr("Berechne via Simulator"), m_simTab);
    m_simCalcBtn->setEnabled(false);
    m_simStatus  = new QLabel(m_simTab);
    m_simStatus->setWordWrap(true);

    auto *layout = new QVBoxLayout(m_simTab);
    layout->addLayout(form);
    layout->addWidget(m_simMapping, 1);
    layout->addWidget(m_simCalcBtn);
    layout->addWidget(m_simStatus);

    // --- Connections ---
    connect(refreshBtn, &QPushButton::clicked,
            this, &EFXEditor::slotSimulatorRefreshScenarios);
    connect(m_simScenario, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &EFXEditor::slotSimulatorScenarioChanged);
    connect(m_simCalcBtn, &QPushButton::clicked,
            this, &EFXEditor::slotSimulatorCalculate);

    connect(sim, &SimulatorClient::scenariosFetched,
            this, &EFXEditor::slotSimulatorScenariosReceived);
    connect(sim, &SimulatorClient::spotsFetched,
            this, &EFXEditor::slotSimulatorSpotsReceived);
    connect(sim, &SimulatorClient::fxCalculated,
            this, &EFXEditor::slotSimulatorFXReceived);
    connect(sim, &SimulatorClient::requestFailed,
            this, &EFXEditor::slotSimulatorError);
}

void EFXEditor::slotSimulatorRefreshScenarios()
{
    m_simScenario->clear();
    m_simStatus->setText(tr("Lade Szenarien..."));
    SimulatorClient::instance()->fetchScenarios();
}

void EFXEditor::slotSimulatorScenariosReceived(const QJsonArray &scenarios)
{
    m_simScenario->clear();
    for (const QJsonValue &v : scenarios)
    {
        QJsonObject sc = v.toObject();
        QString label = sc.value("scenario_name").toString();
        if (sc.value("is_production").toBool())
            label += tr(" [Produktion]");
        m_simScenario->addItem(label, sc.value("scenario_id").toInt());
    }
    m_simStatus->setText(tr("%1 Szenario(s) geladen.").arg(scenarios.count()));
}

void EFXEditor::slotSimulatorScenarioChanged(int index)
{
    if (index < 0) return;
    m_simScenarioId = m_simScenario->itemData(index).toInt();
    m_simCalcBtn->setEnabled(false);
    m_simStatus->setText(tr("Lade Spots..."));
    SimulatorClient::instance()->fetchSpots(m_simScenarioId);
}

void EFXEditor::slotSimulatorSpotsReceived(int scenarioId, const QJsonArray &spots)
{
    if (scenarioId != m_simScenarioId) return;
    m_simSpots = spots;

    // Rebuild spot combos in mapping table
    for (int row = 0; row < m_simMapping->rowCount(); ++row)
    {
        auto *combo = qobject_cast<QComboBox *>(m_simMapping->cellWidget(row, 1));
        if (!combo) continue;
        combo->clear();
        combo->addItem(tr("(nicht zugeordnet)"), -1);
        for (const QJsonValue &v : spots)
        {
            QJsonObject sp = v.toObject();
            combo->addItem(sp.value("name").toString(), sp.value("id").toInt());
        }
        // Auto-assign by index if only one spot
        if (spots.count() == 1) combo->setCurrentIndex(1);
    }
    m_simCalcBtn->setEnabled(m_simMapping->rowCount() > 0 && spots.count() > 0);
    m_simStatus->setText(tr("%1 Spot(s) gefunden.").arg(spots.count()));
}

void EFXEditor::slotSimulatorSpotChanged(int /*row*/, int /*column*/) {}

void EFXEditor::slotSimulatorCalculate()
{
    if (m_simScenarioId < 0) return;

    // Build fixtures array from mapping table
    QJsonArray fixtures;
    const QList<EFXFixture *> &efxFixtures = m_efx->fixtures();
    for (int row = 0; row < m_simMapping->rowCount() && row < efxFixtures.count(); ++row)
    {
        auto *combo = qobject_cast<QComboBox *>(m_simMapping->cellWidget(row, 1));
        if (!combo) continue;
        int spotId = combo->currentData().toInt();
        if (spotId < 0) continue; // not mapped

        quint32 fxiId = efxFixtures[row]->head().fxi;
        QJsonObject fi;
        fi["spot_id"]        = spotId;
        fi["qlc_fixture_id"] = QString::number(fxiId);
        fixtures.append(fi);
    }

    if (fixtures.isEmpty())
    {
        m_simStatus->setText(tr("Kein Fixture einem Spot zugeordnet."));
        return;
    }

    // Sample the EFX path as normalized XY movements (t = 0..1)
    QPolygonF polygon;
    m_efx->preview(polygon);

    QJsonArray movements;
    int total = polygon.size();
    if (total < 2) { m_simStatus->setText(tr("Kein Pfad vorhanden.")); return; }

    for (int i = 0; i < total; ++i)
    {
        double t = (double)i / (total - 1);
        // preview() returns points in range 0..255, normalize to 0..1
        double x = polygon[i].x() / 255.0;
        double y = polygon[i].y() / 255.0;
        x = qBound(0.0, x, 1.0);
        y = qBound(0.0, y, 1.0);
        QJsonObject mv;
        mv["t"] = t;
        mv["x"] = x;
        mv["y"] = y;
        movements.append(mv);
    }

    QJsonObject body;
    body["api_version"]  = "1";
    body["hall_id"]      = m_simScenarioId;
    body["fixtures"]     = fixtures;
    body["movements"]    = movements;
    body["coord_system"] = "normalized";

    m_simCalcBtn->setEnabled(false);
    m_simStatus->setText(tr("Berechne..."));
    SimulatorClient::instance()->calculateFX(body);
}

void EFXEditor::slotSimulatorFXReceived(const QJsonObject &result)
{
    m_simCalcBtn->setEnabled(true);

    QJsonArray results = result.value("results").toArray();
    if (results.isEmpty())
    {
        m_simStatus->setText(tr("Simulator gab keine Ergebnisse zurück."));
        return;
    }

    // Build a keyframe-based EFX JSON effect and add it to the library
    // Format follows EFX-JSON-DESIGN.md: effect with "keyframes" per channel binding
    for (const QJsonValue &rv : results)
    {
        QJsonObject r = rv.toObject();
        QString qlcId = r.value("qlc_fixture_id").toString();
        QJsonArray kfs = r.value("keyframes").toArray();

        // Create a unique effect id: "sim-<scenarioId>-spot<spotId>"
        QString effectId = QString("sim-%1-spot%2")
            .arg(m_simScenarioId).arg(r.value("spot_id").toInt());

        // Build EFX JSON effect with raw pan/tilt keyframes
        QJsonArray panKfs, tiltKfs;
        for (const QJsonValue &kv : kfs)
        {
            QJsonObject kf = kv.toObject();
            if (kf.value("unreachable").toBool()) continue;
            double t   = kf.value("t").toDouble();
            int pan16  = kf.value("pan").toInt();
            int tilt16 = kf.value("tilt").toInt();
            // Normalize 16-bit (0..65535) to 0..1
            panKfs.append(QJsonObject{{"t", t}, {"v", pan16 / 65535.0}});
            tiltKfs.append(QJsonObject{{"t", t}, {"v", tilt16 / 65535.0}});
        }

        QJsonObject effect;
        effect["id"]      = effectId;
        effect["version"] = 1;
        effect["name"]    = QString("Sim %1 – %2")
            .arg(r.value("spot_name").toString()).arg(m_efx->name());
        effect["params"]  = QJsonArray();

        QJsonArray channels;
        // Pan channel binding
        QJsonObject panCh;
        panCh["type"]      = "pan";
        panCh["keyframes"] = panKfs;
        channels.append(panCh);
        // Tilt channel binding
        QJsonObject tiltCh;
        tiltCh["type"]      = "tilt";
        tiltCh["keyframes"] = tiltKfs;
        channels.append(tiltCh);

        effect["channels"] = channels;

        // Add to user library (in-memory + written to ~/.qlcplus/effects/)
        QByteArray effectBytes = QJsonDocument(effect).toJson(QJsonDocument::Compact);
        EFXLibrary::instance()->addUserEffect(effectBytes);

        // Apply to the EFX if only one result
        if (results.count() == 1)
            m_efx->setEffect(effectId);
    }

    // Refresh algorithm combo
    m_effectIds.clear();
    m_algorithmCombo->clear();
    for (const EFXEffectPtr &e : EFXLibrary::instance()->effects())
    {
        m_algorithmCombo->addItem(e->name());
        m_effectIds.append(e->id());
    }
    int idx = m_effectIds.indexOf(m_efx->effectId());
    if (idx >= 0) m_algorithmCombo->setCurrentIndex(idx);

    m_simStatus->setText(tr("✓ %1 Effekt(e) berechnet und übernommen.")
                             .arg(results.count()));
    redrawPreview();
}

void EFXEditor::slotSimulatorError(const QString &message)
{
    m_simCalcBtn->setEnabled(true);
    m_simStatus->setText(
        QString("<font color='red'>%1</font>").arg(message));
}

