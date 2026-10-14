#include "PreferenceStore.h"

namespace stm32_audio::host {

PreferenceStore::LoadResult PreferenceStore::load()
{
    const std::optional<std::string> content = file_.read();

    dirty_ = false;
    known_ = content;
    if (!content) {
        return LoadResult::Missing;
    }
    if (!parsePreferences(*content, prefs_)) {
        file_.keepUnreadable(*content);
        prefs_ = Preferences();
        return LoadResult::Unreadable;
    }
    return LoadResult::Loaded;
}

bool PreferenceStore::save()
{
    const std::string content = serializePreferences(prefs_);

    if (!file_.write(content)) {
        ++writeFailures_;
        return false;
    }
    known_ = content;
    dirty_ = false;
    return true;
}

bool PreferenceStore::poll(std::uint64_t nowMs)
{
    bool reloaded = false;

    if (!lastCheckMs_ || nowMs - *lastCheckMs_ >= kCheckIntervalMs) {
        lastCheckMs_ = nowMs;
        const std::optional<std::string> content = file_.read();
        if (content && content != known_) {
            Preferences fromFile;
            if (parsePreferences(*content, fromFile)) {
                prefs_ = std::move(fromFile);
                dirty_ = false;  // the file wins
                reloaded = true;
            }
            known_ = content;  // an unreadable edit is not retried every second
        }
    }
    if (dirty_ && (!lastSaveMs_ || nowMs - *lastSaveMs_ >= kSaveIntervalMs)) {
        lastSaveMs_ = nowMs;
        (void)save();
    }
    return reloaded;
}

bool PreferenceStore::flush()
{
    return !dirty_ || save();
}

}  // namespace stm32_audio::host
