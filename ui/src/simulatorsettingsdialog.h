/*
  Q Light Controller Plus
  simulatorsettingsdialog.h

  Copyright (C) Immo Focken

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#ifndef SIMULATORSETTINGSDIALOG_H
#define SIMULATORSETTINGSDIALOG_H

#include <QDialog>

class QLineEdit;
class QPushButton;
class QLabel;

class SimulatorSettingsDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SimulatorSettingsDialog(QWidget *parent = nullptr);

private slots:
    void slotTestConnection();
    void slotConnectionTested(bool ok, const QString &message);
    void slotAccepted();

private:
    QLineEdit  *m_urlEdit;
    QLineEdit  *m_tokenEdit;
    QPushButton *m_testButton;
    QLabel      *m_statusLabel;
};

#endif // SIMULATORSETTINGSDIALOG_H
