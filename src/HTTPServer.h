#ifndef ASM_HTTPSERVER_H
#define ASM_HTTPSERVER_H

#include <Arduino.h>
#include <WiFiS3.h>
#include "LockController.h"

class HTTPServer {
public:
    explicit HTTPServer(LockController &lc, uint16_t port = 80);

    void begin();

    void handleClient();

private:
    LockController &lock;
    WiFiServer server;

    void processRequest(WiFiClient &client);

    // Route Handlers
    void handleStatus(WiFiClient &client);

    void handleLock(WiFiClient &client);

    void handleUnlock(WiFiClient &client);

    void handleToggleMode(WiFiClient &client);

    void handleSetTimeout(WiFiClient &client, const String &query);

    void handleSetThreshold(WiFiClient &client, const String &query);

    void handleSetAlarmTimeout(WiFiClient &client, const String &query);

    void handleSetPin(WiFiClient &client, const String &query);

    void handleLogs(WiFiClient &client, const String &query) const;

    static void handleNotFound(WiFiClient &client);

    static void sendJSON(WiFiClient &client, int httpCode, const char *body);

    static long extractParam(const String &query, const char *key);
};

#endif
