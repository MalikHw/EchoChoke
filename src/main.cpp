#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/utils/web.hpp>
#include <random>
#include <filesystem>
#include <chrono>
#include <thread>
#include <regex>
using namespace geode::prelude;
namespace fs = std::filesystem;
class $modify(MyPlayLayer, PlayLayer) {
    struct Fields {
        std::vector<std::string> m_roasts;
        std::vector<std::string> m_congrats;
        bool m_loaded = false;
        async::TaskHolder<utils::web::WebResponse> m_task;
        std::mt19937 m_rng;
    };
    bool init(GJGameLevel* level, bool useReplay, bool dontSave) {
        if (!PlayLayer::init(level, useReplay, dontSave)) return false;
        if (!m_fields->m_loaded) {
            std::random_device rd;
            m_fields->m_rng = std::mt19937(rd());
            auto roastFile = Mod::get()->getSaveDir() / "roasts.txt";
            if (!fs::exists(roastFile)) {
                std::string defaults =
                    "bro died at {}%... skill issue 💀 ()\n"
                    "certified choking hazard at {}% on [] 🙏\n"
                    "{}% and still trash lmao get gud ()\n"
                    "bro really thought he had it but died at {}% 😭\n"
                    "imagine getting to {}% just to choke like that 🙏\n"
                    "{}%... my grandma plays better with one hand 💀\n"
                    "another day, another {}% fail. consistency in being trash is crazy 🥂\n"
                    "bro is allergic to 100%, currently stuck at {}% 💀\n"
                    "{}%? yeah just delete the game at this point fr 🙏\n"
                    "certified {}% moment. seek help 😭\n"
                    "ok but who actually dies at {}%? oh wait, you do 💀\n"
                    "bro's heartbeat peaked just to fail at {}%... tragic 🙏\n"
                    "{}%... i'd be embarrassed to let the webhook even send this 🥂\n"
                    "bro really saw {}% and decided to stop breathing 💀\n"
                    "nice {}% fail bro, keep it up and you'll reach 100% by 2030 🙏\n"
                    "i've seen better gameplay from a literal rock. {}%? embarrassing 😭\n"
                    "{}%... is your monitor even turned on? 💀\n"
                    "bro clicked 0.0001s too late at {}% and lost his soul 🙏\n"
                    "invest in a better gaming chair if you're dying at {}% 🥂\n"
                    "{}%? yeah i'm telling the whole server you're washed 💀\n"
                    "{}%? my cat could do better and he doesn't even have thumbs 😭 ()\n"
                    "bro really choked at {}% on []... just uninstall already 💀\n"
                    "{}%... i've seen bot accounts with better consistency 🙏\n"
                    "imagine dying at {}% in 2026 🥂\n"
                    "{}% is crazy. seek professional help 💀\n"
                    "bro's heart rate went to 200 just to fail at {}% 😭 ()\n"
                    "{}%... i'd rather watch paint dry than this gameplay 💀\n"
                    "bro really hit the pause button on life at {}% 🙏\n"
                    "{}%? yeah that's going in the fail compilation 🥂\n"
                    "certified {}% enjoyer. stay bad bro 😭 ()\n"
                    "{}%... even the level is laughing at you 💀\n"
                    "bro's gaming chair clearly isn't expensive enough for {}% 🙏\n"
                    "{}% is the new 100% for people who can't play 🥂\n"
                    "imagine being this consistent at failing at {}% 😭\n"
                    "{}%... i'm deleting the webhook so i don't have to see this trash 💀\n"
                    "bro really thought he was him until {}% happened 🙏 ()\n"
                    "meow"\n;
                (void)utils::file::writeString(roastFile, defaults);
            }
            auto rResult = utils::file::readString(roastFile);
            if (rResult) {
                std::stringstream rss(rResult.unwrap());
                std::string rLine;
                while (std::getline(rss, rLine))
                    if (!rLine.empty()) m_fields->m_roasts.push_back(rLine);
            }
            auto congratsFile = Mod::get()->getSaveDir() / "congrats.txt";
            if (!fs::exists(congratsFile)) {
                (void)utils::file::writeString(congratsFile,
                    "GG WP! () beat []! 🥂\n"
                    "massive W on [] after <> attempts! 😭\n"
                );
            }
            auto cResult = utils::file::readString(congratsFile);
            if (cResult) {
                std::stringstream css(cResult.unwrap());
                std::string cLine;
                while (std::getline(css, cLine))
                    if (!cLine.empty()) m_fields->m_congrats.push_back(cLine);
            }
            m_fields->m_loaded = true;
        }
        return true;
    }
    void destroyPlayer(PlayerObject* player, GameObject* obj) {
        bool isInvalid = m_isPracticeMode || m_isTestMode || m_level->isPlatformer();
        if (isInvalid || Mod::get()->getSettingValue<bool>("disable_roasts")) {
            PlayLayer::destroyPlayer(player, obj);
            return;
        }
        int percent = this->getCurrentPercentInt();
        auto minPercent = Mod::get()->getSettingValue<int64_t>("min_percent");
        bool isNewBest = percent > m_level->m_normalPercent;
        PlayLayer::destroyPlayer(player, obj);
        if (isNewBest && percent >= minPercent) {
            this->getScheduler()->scheduleSelector(
                schedule_selector(MyPlayLayer::captureAndSendRoast),
                this, 0.1f, 0, 0.0f, false
            );
        }
    }
    void levelComplete() {
        bool isInvalid = m_isPracticeMode || m_isTestMode || m_level->isPlatformer();
        PlayLayer::levelComplete();
        if (isInvalid) return;
        this->getScheduler()->scheduleSelector(
            schedule_selector(MyPlayLayer::captureAndSendCongrats),
            this, 0.1f, 0, 0.0f, false
        );
    }
    void captureAndSendRoast(float dt) { this->sendToDiscord(false); }
    void captureAndSendCongrats(float dt) { this->sendToDiscord(true); }
    std::string getResolutionString() {
        auto size = CCDirector::sharedDirector()->getWinSize();
        return std::to_string((int)size.width) + "x" + std::to_string((int)size.height);
    }
    std::string formatCustomMessage(std::string msg, int percent) {
        std::string userName = GJAccountManager::sharedState()->m_username;
        if (userName.empty()) userName = "Guest";
        std::string levelName = m_level->m_levelName;
        std::string attempts = std::to_string(m_level->m_attempts);
        int totalSeconds = static_cast<int>(m_level->m_workingTime);
        int mins = totalSeconds / 60;
        int secs = totalSeconds % 60;
        std::string timeStr = fmt::format("{:02}:{:02}", mins, secs);
        msg = std::regex_replace(msg, std::regex("\\(\\)"), userName);
        msg = std::regex_replace(msg, std::regex("\\[\\]"), levelName);
        msg = std::regex_replace(msg, std::regex("\\{\\}"), std::to_string(percent));
        msg = std::regex_replace(msg, std::regex("<>"), attempts);
        msg = std::regex_replace(msg, std::regex("!!"), timeStr);
        msg = std::regex_replace(msg, std::regex("~~"), getResolutionString());
        return msg;
    }
    void sendToDiscord(bool isVictory) {
        auto webhook = Mod::get()->getSettingValue<std::string>("webhook_url");
        if (webhook.empty()) return;
        std::string rawMessage;
        int percent = isVictory ? 100 : this->getCurrentPercentInt();
        auto& rng = m_fields->m_rng;
        if (isVictory) {
            if (m_fields->m_congrats.empty()) {
                rawMessage = "GG! () just beat []! 🥂";
            } else {
                std::uniform_int_distribution dis(0, (int)m_fields->m_congrats.size() - 1);
                rawMessage = m_fields->m_congrats[dis(rng)];
            }
        } else {
            if (m_fields->m_roasts.empty()) {
                rawMessage = "died at {}% lol get good";
            } else {
                std::uniform_int_distribution dis(0, (int)m_fields->m_roasts.size() - 1);
                rawMessage = m_fields->m_roasts[dis(rng)];
            }
            bool shouldPing = Mod::get()->getSettingValue<bool>("enable_ping");
            auto threshold = Mod::get()->getSettingValue<int64_t>("ping_threshold");
            std::string roleId = Mod::get()->getSettingValue<std::string>("role_id");
            if (shouldPing && percent >= threshold && !roleId.empty()) {
                rawMessage = "<@&" + roleId + "> " + rawMessage;
            }
        }
        std::string finalMessage = formatCustomMessage(rawMessage, percent);
        auto winSize = CCDirector::sharedDirector()->getWinSize();
        auto renderer = CCRenderTexture::create((int)winSize.width, (int)winSize.height);
        if (!renderer) return;
        renderer->beginWithClear(0, 0, 0, 0);
        this->visit();
        renderer->end();
        auto img = renderer->newCCImage(true);
        if (!img) return;
        auto path = Mod::get()->getSaveDir() / "ss.png";
        std::thread([this, path, finalMessage, webhook, img]() mutable {
            bool ok = img->saveToFile(path.string().c_str());
            img->release();
            if (!ok) return;
            Loader::get()->queueInMainThread([this, path, finalMessage, webhook]() {
                utils::web::MultipartForm form;
                form.param("content", finalMessage);
                auto file = form.file("file", path.string(), "image/png");
                if (file.isErr()) {
                    fs::remove(path);
                    return;
                }
                auto req = utils::web::WebRequest()
                    .bodyMultipart(form)
                    .timeout(std::chrono::seconds(15))
                    .post(webhook);
                m_fields->m_task.spawn(std::move(req), [path](utils::web::WebResponse res) {
                    fs::remove(path);
                    if (res.ok()) {
                        log::info("sent to discord");
                    } else {
                        log::error("webhook failed: {} code {}", res.errorMessage(), res.code());
                    }
                });
            });
        }).detach();
    }
};
// hi jasmin if ur reviewing this
