#pragma once

#include <sqlite3.h>

#include <string>

inline bool executeRawSql(const std::string& path, const char* sql) {
    sqlite3* raw = nullptr;
    bool opened = sqlite3_open(path.c_str(), &raw) == SQLITE_OK;
    bool executed = opened && sqlite3_exec(raw, sql, nullptr, nullptr, nullptr) == SQLITE_OK;
    sqlite3_close(raw);
    return executed;
}
