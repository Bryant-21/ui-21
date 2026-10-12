#include <doctest/doctest.h>
#include "b21ui/Abi.h"
#include "perf/CaptureWriter.h"
#include "perf/MetricStore.h"
#include "perf/PerfMath.h"

#include <cmath>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <thread>

using namespace b21ui::perf;

namespace {
    std::string ReadFile(const std::filesystem::path& file) {
        std::ifstream in(file, std::ios::binary);
        std::stringstream text;
        text << in.rdbuf();
        return text.str();
    }

    std::filesystem::path TempDir(const char* name) {
        auto dir = std::filesystem::temp_directory_path() / "ui21_tests" / name;
        std::filesystem::remove_all(dir);
        std::filesystem::create_directories(dir);
        return dir;
    }
}

TEST_CASE("metric ids name their owner before the first dot") {
    CHECK(OwnerOf("tales.world.cells") == "tales");
    CHECK(OwnerOf("b21ui.frame.ms") == "b21ui");
    CHECK(OwnerOf("solo") == "solo");
    CHECK(ValidId("tales.world.cells"));
    CHECK_FALSE(ValidId(""));
    CHECK_FALSE(ValidId(".leading"));
    CHECK_FALSE(ValidId("has space"));
    CHECK_FALSE(ValidId("a,b"));
    CHECK_FALSE(ValidId("quote\"d"));
}

TEST_CASE("percentiles, stats and frame lows") {
    std::vector<float> values;
    for (int i = 1; i <= 100; ++i) values.push_back(static_cast<float>(i));
    CHECK(Percentile(values, 0.5) == doctest::Approx(50));
    CHECK(Percentile(values, 0.99) == doctest::Approx(99));
    CHECK(Percentile(values, 1.0) == doctest::Approx(100));
    CHECK(Percentile(values, 0.0) == doctest::Approx(1));
    CHECK(Percentile({}, 0.5) == 0.0);
    const auto stats = Compute(values);
    CHECK(stats.samples == 100);
    CHECK(stats.average == doctest::Approx(50.5));
    CHECK(stats.minimum == 1);
    CHECK(stats.maximum == 100);

    // 990 frames at 10 ms and 10 at 40 ms: the 1% low lands on the 99th percentile (10 ms), the 0.1% on 40 ms.
    std::vector<float> frames(990, 10.0F);
    frames.insert(frames.end(), 10, 40.0F);
    const auto lows = FrameLows(frames);
    CHECK(lows.low1Fps == doctest::Approx(100.0));
    CHECK(lows.low01Fps == doctest::Approx(25.0));
    CHECK(FrameLows({}).low1Fps == 0.0);
}

TEST_CASE("metric store registers idempotently and closes frames") {
    MetricStore store;
    const auto a = store.Register("mod.a", "A", "G", B21UI_UNIT_MS, 0);
    CHECK(a == 1);
    CHECK(store.Register("mod.a", "other label", "", B21UI_UNIT_COUNT, 0) == a);
    CHECK(store.Meta(a)->label == "A");
    CHECK(store.Meta(a)->owner == "mod");
    CHECK(store.Register("bad id", "", "", 0, 0) == 0);
    const auto text = store.Register("mod.status", "", "", B21UI_UNIT_TEXT, 0);
    CHECK(store.Meta(text)->label == "mod.status");
    CHECK(*store.Find("mod.status") == text);
    CHECK_FALSE(store.Find("mod.none"));

    store.Push(a, 1.0);
    store.Push(a, 3.0);  // the last push of a frame wins
    store.PushText(text, "loading");
    FrameRow row;
    store.CloseFrame(&row);
    REQUIRE(row.values.size() == 2);
    CHECK(row.values[0] == 3.0F);
    CHECK(std::isnan(row.values[1]));
    REQUIRE(row.texts.size() == 1);
    CHECK(row.texts[0].second == "loading");

    store.PushText(text, "loading");  // unchanged text is not repeated
    store.CloseFrame(&row);
    CHECK(std::isnan(row.values[0]));
    CHECK(row.texts.empty());

    for (int i = 1; i <= 10; ++i) {
        store.Push(a, i);
        store.CloseFrame(nullptr);
    }
    MetricStore::Snapshot s;
    REQUIRE(store.Read(a, s));
    CHECK(s.hasValue);
    CHECK(s.current == 10);
    CHECK(s.stats.samples == 11);
    CHECK(s.stats.maximum == 10);
    CHECK(s.stats.minimum == 1);
    float history[4]{};
    CHECK(store.History(a, history, 4) == 4);
    CHECK(history[0] == 7);
    CHECK(history[3] == 10);
    REQUIRE(store.Read(text, s));
    CHECK(s.text == "loading");
}

TEST_CASE("metric store history wraps at its window") {
    MetricStore store;
    const auto id = store.Register("mod.x", "", "", B21UI_UNIT_COUNT, 0);
    for (std::uint32_t i = 0; i < MetricStore::kHistory + 10; ++i) {
        store.Push(id, i);
        store.CloseFrame(nullptr);
    }
    const auto history = store.History(id);
    REQUIRE(history.size() == MetricStore::kHistory);
    CHECK(history.front() == 10);
    CHECK(history.back() == MetricStore::kHistory + 9);
}

TEST_CASE("metric store accepts pushes from many threads") {
    MetricStore store;
    std::vector<std::thread> threads;
    for (int t = 0; t < 4; ++t)
        threads.emplace_back([&store, t] {
            const auto id = store.Register(("mod.t" + std::to_string(t)).c_str(), "", "", B21UI_UNIT_COUNT, 0);
            for (int i = 0; i < 2000; ++i) store.Push(id, i);
        });
    for (int i = 0; i < 200; ++i) store.CloseFrame(nullptr);
    for (auto& thread : threads) thread.join();
    store.CloseFrame(nullptr);
    CHECK(store.Count() == 4);
    for (std::uint32_t id = 1; id <= 4; ++id) CHECK(store.Current(id) == 1999);
}

TEST_CASE("pin overrides only store differences from the headline default") {
    PinOverrides pins;
    CHECK(pins.IsPinned("a.headline", true));
    CHECK_FALSE(pins.IsPinned("a.plain", false));
    CHECK(pins.Set("a.plain", false, true));
    CHECK(pins.Set("a.headline", true, false));
    CHECK_FALSE(pins.Set("a.headline", true, false));
    CHECK(pins.PinnedList() == "a.plain");
    CHECK(pins.UnpinnedList() == "a.headline");
    CHECK(pins.Set("a.headline", true, true));  // back to the default: no override left
    CHECK(pins.UnpinnedList().empty());

    PinOverrides loaded;
    loaded.Load("x.one, x.two,,bad id", "x.three,x.one");
    CHECK(loaded.IsPinned("x.one", false));
    CHECK(loaded.IsPinned("x.two", false));
    CHECK_FALSE(loaded.IsPinned("x.three", true));
    CHECK(loaded.IsPinned("x.one", true));  // pinned wins over a conflicting unpinned entry
    CHECK(loaded.PinnedList() == "x.one,x.two");
    CHECK(loaded.UnpinnedList() == "x.three");
    CHECK(SplitList(" a.b , c.d ") == std::vector<std::string>{"a.b", "c.d"});
}

TEST_CASE("value formatting by unit") {
    CHECK(FormatValue(12.345, B21UI_UNIT_MS) == "12.35 ms");
    CHECK(FormatValue(60.04, B21UI_UNIT_FPS) == "60.0 FPS");
    CHECK(FormatValue(512.4, B21UI_UNIT_MB) == "512 MB");
    CHECK(FormatValue(42, B21UI_UNIT_COUNT) == "42");
    CHECK(FormatValue(2.5, B21UI_UNIT_COUNT) == "2.5");
    CHECK(FormatValue(37.25, B21UI_UNIT_PERCENT) == "37.2%");
    CHECK(FormatValue(NAN, B21UI_UNIT_MS) == "n/a");
    CHECK(std::string(UnitName(B21UI_UNIT_TEXT)) == "text");
}

TEST_CASE("capture CSV carries values forward and quotes text") {
    MetricStore store;
    const auto ms = store.Register(kFrameMs, "Frame", "Frame", B21UI_UNIT_MS, 0);
    const auto cells = store.Register("mod.cells", "Cells", "World", B21UI_UNIT_COUNT, 0);
    CaptureData data;
    store.Push(ms, 10);
    store.Push(cells, 3);
    data.rows.emplace_back();
    store.CloseFrame(&data.rows.back());
    data.times.push_back(0.0);
    const auto text = store.Register("mod.state", "State", "", B21UI_UNIT_TEXT, 0);  // registered mid-capture
    store.Push(ms, 20);
    store.PushText(text, "say \"hi\"");
    data.rows.emplace_back();
    store.CloseFrame(&data.rows.back());
    data.times.push_back(0.02);
    for (std::uint32_t i = 1; i <= store.Count(); ++i) data.columns.push_back(*store.Meta(i));

    const auto dir = TempDir("csv");
    REQUIRE(WriteCsv(data, dir / "c.csv"));
    CHECK(ReadFile(dir / "c.csv") ==
          "frame,time_s,b21ui.frame.ms,mod.cells,mod.state\n"
          "0,0.000000,10,3,\"\"\n"
          "1,0.020000,20,3,\"say \"\"hi\"\"\"\n");

    REQUIRE(WriteJson(data, dir / "c.json"));
    const auto summary = nlohmann::json::parse(ReadFile(dir / "c.json"));
    CHECK(summary["frames"] == 2);
    CHECK(summary["durationSeconds"].get<double>() == doctest::Approx(0.02));
    const auto& metrics = summary["metrics"];
    REQUIRE(metrics.size() == 3);
    CHECK(metrics[0]["id"] == kFrameMs);
    CHECK(metrics[0]["avg"].get<double>() == doctest::Approx(15));
    CHECK(metrics[0]["max"].get<double>() == doctest::Approx(20));
    CHECK(metrics[0]["unit"] == "ms");
    CHECK(metrics[0].contains("low1Fps"));
    CHECK(metrics[1]["samples"] == 1);  // carried values do not count as samples
    CHECK(metrics[1]["owner"] == "mod");
    CHECK(metrics[2]["last"] == "say \"hi\"");
    CHECK(summary["fps"]["average"].get<double>() == doctest::Approx(1000.0 / 15));

    const auto baseline = ReadBaseline(summary);
    CHECK(baseline.size() == 2);
    CHECK(baseline.at(kFrameMs) == doctest::Approx(15));
    CHECK_THROWS(ReadBaseline(nlohmann::json::object()));
}

TEST_CASE("capture file names sort by time") {
    CHECK(CaptureStem(2026, 10, 2, 9, 5, 7) == "B21UI_Capture_20261002_090507");
    CHECK(CsvQuote("a\nb") == "\"a b\"");
}

TEST_CASE("framework 14 appends the perf calls at the end of the host table") {
    CHECK(offsetof(B21UI_HostApi, perfRegisterMetric) == offsetof(B21UI_HostApi, registerKeybindings) + sizeof(void*));
    CHECK(sizeof(B21UI_HostApi) == offsetof(B21UI_HostApi, perfClearBaseline) + sizeof(void*));
}

TEST_CASE("a capture's first row carries every text metric") {
    MetricStore store;
    const auto text = store.Register("mod.state", "", "", B21UI_UNIT_TEXT, 0);
    store.Register("mod.n", "", "", B21UI_UNIT_COUNT, 0);
    store.PushText(text, "ready");
    store.CloseFrame(nullptr);
    FrameRow row;
    store.CloseFrame(&row);
    CHECK(row.texts.empty());
    store.CloseFrame(&row, true);
    REQUIRE(row.texts.size() == 1);
    CHECK(row.texts[0].first == 0);
    CHECK(row.texts[0].second == "ready");
}
