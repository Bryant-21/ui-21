#pragma once
#include "perf/MetricStore.h"

#include <nlohmann/json.hpp>

#include <filesystem>
#include <map>
#include <string>
#include <vector>

namespace b21ui::perf {
    // Everything a finished capture needs to be written; built on the render thread, written elsewhere.
    struct CaptureData {
        std::vector<MetricMeta> columns;  // metric index order
        std::vector<double> times;        // seconds since recording started, one per frame
        std::vector<FrameRow> rows;       // one per frame; a row may be shorter than `columns`
        nlohmann::json info = nlohmann::json::object();  // timestamp, game, mods, settings...
    };

    // "B21UI_Capture_20261012_101500"
    std::string CaptureStem(int year, int month, int day, int hour, int minute, int second);
    std::string CsvQuote(std::string_view text);

    // frame,time_s, then one column per metric id. Numbers carry the last value forward; text is quoted.
    bool WriteCsv(const CaptureData& data, const std::filesystem::path& file);
    // Per-metric statistics over the frames that pushed the metric, plus `info`.
    nlohmann::json Summarize(const CaptureData& data);
    bool WriteJson(const CaptureData& data, const std::filesystem::path& file);

    // metric id -> average, from a summary written by WriteJson. Throws on malformed input.
    std::map<std::string, double> ReadBaseline(const nlohmann::json& summary);
}
