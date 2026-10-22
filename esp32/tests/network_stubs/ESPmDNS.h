#pragma once

struct TestMdns {
    void end() {
    }

    bool begin(const char *) {
        return true;
    }

    void addService(const char *, const char *, int) {
    }
} inline MDNS;
