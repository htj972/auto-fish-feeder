#pragma once

struct DNSServer {
    bool active = false;

    template <typename T> void start(int, const char *, T) {
        active = true;
    }

    void stop() {
        active = false;
    }

    void processNextRequest() {
    }
};
