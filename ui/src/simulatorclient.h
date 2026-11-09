/*
  Q Light Controller Plus
  simulatorclient.h

  Copyright (C) Immo Focken

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#ifndef SIMULATORCLIENT_H
#define SIMULATORCLIENT_H

#include <QNetworkAccessManager>
#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QString>

class QNetworkReply;

/**
 * HTTP client for the Moving-Head Simulator FX API.
 *
 * Singleton. All calls are async; results arrive via signals.
 * URL and token are persisted in QSettings under group "Simulator".
 */
class SimulatorClient : public QObject
{
    Q_OBJECT

public:
    static SimulatorClient *instance();

    QString url() const   { return m_url; }
    QString token() const { return m_token; }

    void setUrl(const QString &url);
    void setToken(const QString &token);

    /** GET /api/v1/fx/health — no auth required. */
    void testConnection();

    /** GET /api/v1/fx/scenarios */
    void fetchScenarios();

    /** GET /api/v1/fx/scenarios/{id}/spots */
    void fetchSpots(int scenarioId);

    /**
     * POST /api/v1/fx/calculate
     * body must follow fx_api.calculate_fx() schema:
     *   { api_version, hall_id, fixtures, movements, coord_system }
     */
    void calculateFX(const QJsonObject &body);

signals:
    /** Emitted after testConnection(). ok=false includes an error message. */
    void connectionTested(bool ok, const QString &message);

    /** Emitted after fetchScenarios(). */
    void scenariosFetched(const QJsonArray &scenarios);

    /** Emitted after fetchSpots(). */
    void spotsFetched(int scenarioId, const QJsonArray &spots);

    /** Emitted after calculateFX() succeeds. */
    void fxCalculated(const QJsonObject &result);

    /** Emitted on network or HTTP errors (all endpoints). */
    void requestFailed(const QString &message);

private:
    explicit SimulatorClient(QObject *parent = nullptr);

    QNetworkRequest makeRequest(const QString &path) const;
    void handleReply(QNetworkReply *reply,
                     const std::function<void(const QByteArray &)> &onSuccess);

    QNetworkAccessManager *m_nam;
    QString m_url;
    QString m_token;
};

#endif // SIMULATORCLIENT_H
