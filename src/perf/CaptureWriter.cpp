#include "perf/CaptureWriter.h"
#include "b21ui/Abi.h"

#include <cmath>
#include <format>
#include <fstream>
#include <stdexcept>

namespace b21ui::perf {
    std::string CaptureStem(int year, int month, int day, int hour, int minute, int second) {
        return std::format("B21UI_Capture_{:04}{:02}{:02}_{:02}{:02}{:02}", year, month, day, hour, minute, second);
    }

    std::string CsvQuote(std::string_view text) {
        std::string out = "\"";
        for (const char c : text) {
            if (c == '"') out += '"';
            out += (c == '\r' || c == '\n') ? ' ' : c;
        }
        return out + '"';
    }

    bool WriteCsv(const CaptureData& data, const std::filesystem::path& file) {
        std::ofstream out(file, std::ios::binary | std::ios::trunc);
        if (!out) return false;
        out << "frame,time_s";
        for (const auto& column : data.columns) out << ',' << column.id;
        out << '\n';
        const auto width = data.columns.size();
        std::vector<float> last(width, std::nanf(""));
        std::vector<std::string> text(width);
        std::string line;
        for (std::size_t frame = 0; frame < data.rows.size(); ++frame) {
            const auto& row = data.rows[frame];
            for (std::size_t i = 0; i < row.values.size() && i < width; ++i)
                if (!std::isnan(row.values[i])) last[i] = row.values[i];
            for (const auto& [index, value] : row.texts)
                if (index < width) text[index] = value;
            line = std::format("{},{:.6f}", frame, frame < data.times.size() ? data.times[frame] : 0.0);
            for (std::size_t i = 0; i < width; ++i) {
                line += ',';
                if (data.columns[i].unit == B21UI_UNIT_TEXT) line += CsvQuote(text[i]);
                else if (!std::isnan(last[i])) line += std::format("{:.6g}", last[i]);
            }
            line += '\n';
            out << line;
        }
        return static_cast<bool>(out);
    }

    nlohmann::json Summarize(const CaptureData& data) {
        auto summary = data.info;
        const double duration = data.times.empty() ? 0.0 : data.times.back();
        summary["frames"] = data.rows.size();
        summary["durationSeconds"] = duration;
        auto metrics = nlohmann::json::array();
        std::vector<float> values;
        for (std::size_t i = 0; i < data.columns.size(); ++i) {
            const auto& meta = data.columns[i];
            nlohmann::json metric{{"id", meta.id}, {"label", meta.label}, {"group", meta.group}, {"owner", meta.owner},
                                  {"unit", UnitName(meta.unit)}};
            if (meta.unit == B21UI_UNIT_TEXT) {
                std::string lastText;
                for (const auto& row : data.rows)
                    for (const auto& [index, value] : row.texts)
                        if (index == i) lastText = value;
                metric["last"] = lastText;
                metrics.push_back(std::move(metric));
                continue;
            }
            values.clear();
            for (const auto& row : data.rows)
                if (i < row.values.size() && !std::isnan(row.values[i])) values.push_back(row.values[i]);
            const auto stats = Compute(values);
            metric["samples"] = stats.samples;
            if (stats.samples) {
                metric["avg"] = stats.average;
                metric["min"] = stats.minimum;
                metric["max"] = stats.maximum;
                metric["p50"] = stats.p50;
                metric["p99"] = stats.p99;
            }
            if (meta.id == kFrameMs && stats.samples) {
                const auto lows = FrameLows(values);
                metric["low1Fps"] = lows.low1Fps;
                metric["low01Fps"] = lows.low01Fps;
                summary["fps"] = {{"average", stats.average > 0 ? 1000.0 / stats.average : 0.0},
                                  {"low1", lows.low1Fps}, {"low01", lows.low01Fps}};
            }
            metrics.push_back(std::move(metric));
        }
        summary["metrics"] = std::move(metrics);
        return summary;
    }

    bool WriteJson(const CaptureData& data, const std::filesystem::path& file) {
        std::ofstream out(file, std::ios::binary | std::ios::trunc);
        if (!out) return false;
        out << Summarize(data).dump(2) << '\n';
        return static_cast<bool>(out);
    }

    std::map<std::string, double> ReadBaseline(const nlohmann::json& summary) {
        const auto& metrics = summary.at("metrics");
        if (!metrics.is_array()) throw std::runtime_error("metrics is not an array");
        std::map<std::string, double> out;
        for (const auto& metric : metrics)
            if (metric.contains("avg") && metric["avg"].is_number()) out[metric.at("id").get<std::string>()] = metric["avg"].get<double>();
        return out;
    }
}
