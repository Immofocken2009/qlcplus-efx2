/*
  Q Light Controller Plus
  simulatorclient.cpp

  Copyright (C) Immo Focken

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt
*/

#include "simulatorclient.h"

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QSettings>
#include <QUrl>

static const char *SETTINGS_GROUP  = "Simulator";
static const char *SETTINGS_URL    = "url";
static const char *SETTINGS_TOKEN  = "token";
static const char *DEFAULT_URL     = "http://192.168.10.27:8090";

SimulatorClient *SimulatorClient::instance()
{
    static SimulatorClient s_instance;
    return &s_instance;
}

SimulatorClient::SimulatorClient(QObject *parent)
    : QObject(parent)
    , m_nam(new QNetworkAccessManager(this))
{
    QSettings s;
    s.beginGroup(SETTINGS_GROUP);
    m_url   = s.value(SETTINGS_URL,   DEFAULT_URL).toString();
    m_token = s.value(SETTINGS_TOKEN, QString()).toString();
    s.endGroup();
}

void SimulatorClient::setUrl(const QString &url)
{
    m_url = url;
    QSettings s;
    s.beginGroup(SETTINGS_GROUP);
    s.setValue(SETTINGS_URL, url);
    s.endGroup();
}

void SimulatorClient::setToken(const QString &token)
{
    m_token = token;
    QSettings s;
    s.beginGroup(SETTINGS_GROUP);
    s.setValue(SETTINGS_TOKEN, token);
    s.endGroup();
}

QNetworkRequest SimulatorClient::makeRequest(const QString &path) const
{
    QNetworkRequest req(QUrl(m_url + path));
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    if (!m_token.isEmpty())
        req.setRawHeader("Authorization", ("Bearer " + m_token).toUtf8());
    return req;
}

void SimulatorClient::handleReply(QNetworkReply *reply,
                                   const std::function<void(const QByteArray &)> &onSuccess)
{
    connect(reply, &QNetworkReply::finished, this, [this, reply, onSuccess]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            emit requestFailed(reply->errorString());
            return;
        }
        int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        QByteArray body = reply->readAll();
        if (status < 200 || status >= 300) {
            QJsonObject obj = QJsonDocument::fromJson(body).object();
            QString msg = obj.value("error").toString();
            if (msg.isEmpty()) msg = QString("HTTP %1").arg(status);
            emit requestFailed(msg);
            return;
        }
        onSuccess(body);
    });
}

void SimulatorClient::testConnection()
{
    QNetworkRequest req(QUrl(m_url + "/api/v1/fx/health"));
    req.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    // health endpoint requires no token
    QNetworkReply *reply = m_nam->get(req);
    connect(reply, &QNetworkReply::finished, this, [this, reply]() {
        reply->deleteLater();
        if (reply->error() != QNetworkReply::NoError) {
            emit connectionTested(false, reply->errorString());
            return;
        }
        QJsonObject obj = QJsonDocument::fromJson(reply->readAll()).object();
        bool ok = obj.value("ok").toBool(false);
        QString ver = obj.value("api_version").toString();
        emit connectionTested(ok, ok ? tr("Verbunden (API v%1)").arg(ver)
                                     : tr("Server antwortet, aber ok=false"));
    });
}

void SimulatorClient::fetchScenarios()
{
    QNetworkReply *reply = m_nam->get(makeRequest("/api/v1/fx/scenarios"));
    handleReply(reply, [this](const QByteArray &body) {
        QJsonDocument doc = QJsonDocument::fromJson(body);
        emit scenariosFetched(doc.array());
    });
}

void SimulatorClient::fetchSpots(int scenarioId)
{
    QNetworkReply *reply = m_nam->get(
        makeRequest(QString("/api/v1/fx/scenarios/%1/spots").arg(scenarioId)));
    handleReply(reply, [this, scenarioId](const QByteArray &body) {
        QJsonObject obj = QJsonDocument::fromJson(body).object();
        emit spotsFetched(scenarioId, obj.value("spots").toArray());
    });
}

void SimulatorClient::calculateFX(const QJsonObject &body)
{
    QNetworkRequest req = makeRequest("/api/v1/fx/calculate");
    QByteArray payload = QJsonDocument(body).toJson(QJsonDocument::Compact);
    QNetworkReply *reply = m_nam->post(req, payload);
    handleReply(reply, [this](const QByteArray &body) {
        QJsonObject obj = QJsonDocument::fromJson(body).object();
        emit fxCalculated(obj);
    });
}
