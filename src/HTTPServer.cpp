#include "HTTPServer.h"

HTTPServer::HTTPServer(LockController &lc, const uint16_t port)
    : lock(lc), server(port) {
}

void HTTPServer::begin() {
    server.begin();
    Serial.println("HTTP server started");
}

void HTTPServer::handleClient() {
    WiFiClient client = server.available();
    if (!client) return;

    // Wait briefly for data
    const unsigned long timeout = millis() + 200;
    while (!client.available() && millis() < timeout) { delay(1); }

    if (client.available()) processRequest(client);
    client.stop();
}

void HTTPServer::processRequest(WiFiClient &client) {
    String firstLine = client.readStringUntil('\n');
    firstLine.trim();

    // Skip headers
    while (client.available()) {
        String line = client.readStringUntil('\n');
        if (line == "\r" || line.length() == 0) break;
    }

    // Validate method
    if (!firstLine.startsWith("GET ")) {
        sendJSON(client, 405, R"({"error":"Method not allowed. Use GET."})");
        return;
    }

    // Parse path
    constexpr int pathStart = 4;
    const int pathEnd = firstLine.indexOf(' ', pathStart);
    if (pathEnd < 0) {
        sendJSON(client, 400, R"({"error":"Bad request"})");
        return;
    }
    String fullPath = firstLine.substring(pathStart, pathEnd);
    String path, query;
    const int qPos = fullPath.indexOf('?');
    if (qPos >= 0) {
        path = fullPath.substring(0, qPos);
        query = fullPath.substring(qPos + 1);
    } else {
        path = fullPath;
        query = "";
    }

    // Route request
    if (path == "/" || path == "/status") handleStatus(client);
    else if (path == "/lock") handleLock(client);
    else if (path == "/unlock") handleUnlock(client);
    else if (path == "/toggle_mode") handleToggleMode(client);
    else if (path == "/set_timeout") handleSetTimeout(client, query);
    else if (path == "/set_threshold") handleSetThreshold(client, query);
    else if (path == "/set_alarm_timeout") handleSetAlarmTimeout(client, query);
    else if (path == "/set_pin") handleSetPin(client, query);
    else if (path == "/logs") handleLogs(client, query);
    else handleNotFound(client);
}

// --- Handlers ---

void HTTPServer::handleStatus(WiFiClient &client) {
    char buf[200];
    lock.getStatusJSON(buf, sizeof(buf));
    sendJSON(client, 200, buf);
}

void HTTPServer::handleLock(WiFiClient &client) {
    lock.remoteLock();
    sendJSON(client, 200, R"({"ok":true,"action":"lock"})");
}

void HTTPServer::handleUnlock(WiFiClient &client) {
    lock.remoteUnlock();
    sendJSON(client, 200, R"({"ok":true,"action":"unlock"})");
}

void HTTPServer::handleToggleMode(WiFiClient &client) {
    lock.toggleMode();
    const char *modeStr = (lock.getMode() == MODE_AUTO) ? "AUTO" : "MANUAL";
    char buf[64];
    snprintf(buf, sizeof(buf),
             R"({"ok":true,"action":"toggle_mode","mode":"%s"})", modeStr);
    sendJSON(client, 200, buf);
}

void HTTPServer::handleSetTimeout(WiFiClient &client, const String &query) {
    const long val = extractParam(query, "ms");
    if (val < 0) {
        sendJSON(client, 400, R"({"error":"Missing param: ms"})");
        return;
    }
    lock.setAutoLockTimeout((unsigned long) val);
    char buf[64];
    snprintf(buf, sizeof(buf),
             R"({"ok":true,"action":"set_timeout","ms":%ld})", val);
    sendJSON(client, 200, buf);
}

void HTTPServer::handleSetThreshold(WiFiClient &client, const String &query) {
    const long val = extractParam(query, "count");
    if (val < 1 || val > 10) {
        sendJSON(client, 400,
                 "{\"error\":\"Missing or invalid param: count (1-10)\"}");
        return;
    }
    lock.setWrongCodeThreshold((uint8_t) val);
    char buf[64];
    snprintf(buf, sizeof(buf),
             R"({"ok":true,"action":"set_threshold","count":%ld})", val);
    sendJSON(client, 200, buf);
}

void HTTPServer::handleSetAlarmTimeout(WiFiClient &client, const String &query) {
    const long val = extractParam(query, "ms");
    if (val < 0) {
        sendJSON(client, 400, R"({"error":"Missing param: ms"})");
        return;
    }
    lock.setAlarmTimeout((unsigned long) val);
    char buf[64];
    snprintf(buf, sizeof(buf),
             R"({"ok":true,"action":"set_alarm_timeout","ms":%ld})", val);
    sendJSON(client, 200, buf);
}

void HTTPServer::handleSetPin(WiFiClient &client, const String &query) {
    // Extract pin=XXXX
    const String keyEq = "pin=";
    const int idx = query.indexOf(keyEq);
    if (idx < 0) {
        sendJSON(client, 400, R"({"error":"Missing param: pin"})");
        return;
    }
    String newPin = query.substring(idx + keyEq.length());
    // Remove trailing params
    const int ampPos = newPin.indexOf('&');
    if (ampPos >= 0) newPin = newPin.substring(0, ampPos);

    if (newPin.length() < 1 || newPin.length() > 8) {
        sendJSON(client, 400,
                 "{\"error\":\"Invalid pin length (1-8 digits)\"}");
        return;
    }
    // Validate digits
    for (unsigned int i = 0; i < newPin.length(); i++) {
        if (!isDigit(newPin[i])) {
            sendJSON(client, 400,
                     R"({"error":"PIN must contain digits only"})");
            return;
        }
    }
    lock.setPin(newPin.c_str());
    // Do not echo PIN in response
    sendJSON(client, 200, R"({"ok":true,"action":"set_pin"})");
}

void HTTPServer::handleLogs(WiFiClient &client, const String &query) const {
    const uint8_t total = lock.getLogCount();
    const long requested = extractParam(query, "n");

    // Determine count to return
    uint8_t count;
    if (requested < 0 || requested > total) {
        count = total;
    } else if (requested == 0) {
        count = 0;
    } else {
        count = (uint8_t) requested;
    }

    // Build JSON array
    char buf[1320];
    int pos = 0;
    pos += snprintf(buf + pos, sizeof(buf) - pos,
                    R"({"total":%u,"returned":%u,"logs":[)",
                    total, count);
    for (uint8_t i = total - count; i < total; i++) {
        LogEntry e = lock.getLogEntry(i);
        pos += snprintf(buf + pos, sizeof(buf) - pos,
                        R"(%s{"t":%lu,"ev":"%s"})",
                        (i > total - count) ? "," : "",
                        e.timestamp, e.eventType);
    }
    snprintf(buf + pos, sizeof(buf) - pos, "]}");
    sendJSON(client, 200, buf);
}

void HTTPServer::handleNotFound(WiFiClient &client) {
    sendJSON(client, 404,
             "{\"error\":\"Not found\","
             "\"routes\":[\"/status\",\"/lock\",\"/unlock\","
             "\"/toggle_mode\",\"/set_timeout?ms=N\","
             "\"/set_threshold?count=N\","
             "\"/set_alarm_timeout?ms=N\","
             "\"/set_pin?pin=XXXX\","
             "\"/logs\",\"/logs?n=N\"]}");
}

// --- Helpers ---

void HTTPServer::sendJSON(WiFiClient &client, const int httpCode,
                         const char *body) {
    client.print("HTTP/1.1 ");
    client.println(httpCode);
    client.println("Content-Type: application/json");
    client.println("Connection: close");
    client.println("Access-Control-Allow-Origin: *");
    client.println();
    client.println(body);
}

long HTTPServer::extractParam(const String &query, const char *key) {
    const String keyEq = String(key) + "=";
    const int idx = query.indexOf(keyEq);
    if (idx < 0) return -1;
    const int valueStart = idx + keyEq.length();
    int valueEnd = query.indexOf('&', valueStart);
    if (valueEnd < 0) valueEnd = static_cast<int>(query.length());
    const String valueStr = query.substring(valueStart, valueEnd);
    if (valueStr.length() == 0) return -1;
    return valueStr.toInt();
}
