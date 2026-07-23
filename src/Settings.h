#pragma once

#include <filesystem>

namespace nppmr {

enum class ThemeMode {
    FollowNotepad,
    Light,
    Dark
};

struct ReaderSettings {
    ThemeMode theme = ThemeMode::FollowNotepad;
    int fontSizePx = 17;
    int contentWidthPx = 860;
    bool showHeader = true;
    double lineHeight = 1.72;
    bool allowRemoteImages = false;
};

class SettingsStore {
public:
    void SetConfigDirectory(const std::filesystem::path& directory);
    void Load();
    void Save() const;

    ReaderSettings& Values() noexcept { return values_; }
    const ReaderSettings& Values() const noexcept { return values_; }
    const std::filesystem::path& IniPath() const noexcept { return iniPath_; }

private:
    std::filesystem::path iniPath_;
    ReaderSettings values_;
};

} // namespace nppmr
