/*
  Q Light Controller Plus
  simulatorsettingsdialog.cpp

  Copyright (C) Immo Focken

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#include "simulatorsettingsdialog.h"
#include "simulatorclient.h"

#include <QDialogButtonBox>
#include <QPushButton>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QLineEdit>
#include <QLabel>

SimulatorSettingsDialog::SimulatorSettingsDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Simulator-Verbindung"));
    setMinimumWidth(420);

    m_urlEdit = new QLineEdit(SimulatorClient::instance()->url(), this);
    m_urlEdit->setPlaceholderText("http://192.168.10.27:8090");

    m_tokenEdit = new QLineEdit(SimulatorClient::instance()->token(), this);
    m_tokenEdit->setEchoMode(QLineEdit::Password);
    m_tokenEdit->setPlaceholderText(tr("(leer = kein Token)"));

    m_testButton  = new QPushButton(tr("Verbindung testen"), this);
    m_statusLabel = new QLabel(this);
    m_statusLabel->setWordWrap(true);

    auto *testRow = new QHBoxLayout;
    testRow->addWidget(m_testButton);
    testRow->addWidget(m_statusLabel, 1);

    auto *form = new QFormLayout;
    form->addRow(tr("URL:"),   m_urlEdit);
    form->addRow(tr("Token:"), m_tokenEdit);

    auto *buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);

    auto *layout = new QVBoxLayout(this);
    layout->addLayout(form);
    layout->addLayout(testRow);
    layout->addWidget(buttons);

    connect(m_testButton, &QPushButton::clicked,
            this, &SimulatorSettingsDialog::slotTestConnection);
    connect(SimulatorClient::instance(), &SimulatorClient::connectionTested,
            this, &SimulatorSettingsDialog::slotConnectionTested);
    connect(buttons, &QDialogButtonBox::accepted,
            this, &SimulatorSettingsDialog::slotAccepted);
    connect(buttons, &QDialogButtonBox::rejected,
            this, &QDialog::reject);
}

void SimulatorSettingsDialog::slotTestConnection()
{
    // Apply current field values before testing
    SimulatorClient::instance()->setUrl(m_urlEdit->text().trimmed());
    SimulatorClient::instance()->setToken(m_tokenEdit->text().trimmed());
    m_statusLabel->setText(tr("Teste..."));
    m_testButton->setEnabled(false);
    SimulatorClient::instance()->testConnection();
}

void SimulatorSettingsDialog::slotConnectionTested(bool ok, const QString &message)
{
    m_testButton->setEnabled(true);
    m_statusLabel->setText(
        QString("<font color='%1'>%2</font>").arg(ok ? "green" : "red", message));
}

void SimulatorSettingsDialog::slotAccepted()
{
    SimulatorClient::instance()->setUrl(m_urlEdit->text().trimmed());
    SimulatorClient::instance()->setToken(m_tokenEdit->text().trimmed());
    accept();
}
