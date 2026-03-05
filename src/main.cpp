#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/utils/web.hpp>
#include <random>
#include <regex>
#include <filesystem>

using namespace geode::prelude;
namespace fs = std::filesystem;

static std::vector<std::string> loadLines(fs::path p, std::string fallback) {
    std::vector<std::string> out;
    if (!fs::exists(p))
        (void)utils::file::writeString(p, fallback);
    auto res = utils::file::readString(p);
    if (!res) return out;
    std::stringstream ss(res.unwrap());
    std::string ln;
    while (std::getline(ss, ln))
        if (!ln.empty()) out.push_back(ln);
    return out;
}

static std::string pickRandom(std::vector<std::string>& v, std::mt19937& rng, std::string fallback) {
    if (v.empty()) return fallback;
    return v[std::uniform_int_distribution<size_t>(0, v.size()-1)(rng)];
}

class $modify(MyPlayLayer, PlayLayer) {
    struct Fields {
        std::vector<std::string> m_roasts;
        std::vector<std::string> m_congrats;
        bool m_loaded = false;
        async::TaskHolder<web::WebResponse> m_webhookTask;
        std::mt19937 m_rng;
    };

    bool init(GJGameLevel* level, bool useReplay, bool dontSave) {
        if (!PlayLayer::init(level, useReplay, dontSave)) return false;

        if (m_fields->m_loaded) return true;

        m_fields->m_rng = std::mt19937(std::random_device{}());

        auto dir = Mod::get()->getSaveDir();

        std::string defaultRoasts =
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
            "bro really thought he was him until {}% happened 🙏 ()\n";

        m_fields->m_roasts = loadLines(dir / "roasts.txt", defaultRoasts);
        m_fields->m_congrats = loadLines(dir / "congrats.txt",
            "GG WP! () beat []! 🥂\n"
            "massive W on [] after <> attempts! 😭\n"
        );

        m_fields->m_loaded = true;
        return true;
    }

    void destroyPlayer(PlayerObject* p, GameObject* obj) {
        bool skip = m_isPracticeMode || m_isTestMode || m_level->isPlatformer();
        int pct = getCurrentPercentInt();
        bool nb = pct > m_level->m_normalPercent;
        auto minPct = Mod::get()->getSettingValue<int64_t>("min_percent");

        PlayLayer::destroyPlayer(p, obj);

        if (skip || Mod::get()->getSettingValue<bool>("disable_roasts")) return;
        if (nb && pct >= minPct) sendWebhook(false, pct);
    }

    void levelComplete() {
        bool skip = m_isPracticeMode || m_isTestMode || m_level->isPlatformer();
        PlayLayer::levelComplete();
        if (!skip) sendWebhook(true, 100);
    }

    std::string fmtMsg(std::string msg, int pct) {
        auto usr = GJAccountManager::sharedState()->m_username;
        if (usr.empty()) usr = "Guest";

        int t = (int)m_level->m_workingTime;
        auto timeStr = fmt::format("{:02}:{:02}", t/60, t%60);

        auto winSz = CCDirector::sharedDirector()->getWinSize();
        auto res = std::to_string((int)winSz.width) + "x" + std::to_string((int)winSz.height);

        msg = std::regex_replace(msg, std::regex("\\(\\)"), usr);
        msg = std::regex_replace(msg, std::regex("\\[\\]"), std::string(m_level->m_levelName));
        msg = std::regex_replace(msg, std::regex("\\{\\}"), std::to_string(pct));
        msg = std::regex_replace(msg, std::regex("<>"), std::to_string(m_level->m_attempts));
        msg = std::regex_replace(msg, std::regex("!!"), timeStr);
        msg = std::regex_replace(msg, std::regex("~~"), res);
        return msg;
    }

    void sendWebhook(bool win, int pct) {
        auto url = Mod::get()->getSettingValue<std::string>("webhook_url");
        if (url.empty()) return;

        std::string raw;
        if (win) {
            raw = pickRandom(m_fields->m_congrats, m_fields->m_rng, "GG! () just beat []! 🥂");
        } else {
            raw = pickRandom(m_fields->m_roasts, m_fields->m_rng, "died at {}% lol get good");

            auto roleId = Mod::get()->getSettingValue<std::string>("role_id");
            if (Mod::get()->getSettingValue<bool>("enable_ping")
                && pct >= Mod::get()->getSettingValue<int64_t>("ping_threshold")
                && !roleId.empty())
            {
                raw = "<@&" + roleId + "> " + raw;
            }
        }

        auto msg = fmtMsg(raw, pct);

        auto sz = CCDirector::sharedDirector()->getWinSize();
        auto rend = CCRenderTexture::create((int)sz.width, (int)sz.height);
        rend->begin();
        this->visit();
        rend->end();

        auto img = rend->newCCImage();
        auto ssPath = Mod::get()->getSaveDir() / "ss.png";
        img->saveToFile(ssPath.string().c_str(), false);
        img->release();

        web::MultipartForm form;
        form.addField("content", msg);
        form.addFile("file", ssPath);

        auto req = web::WebRequest();
        req.bodyMultipart(form);

        m_fields->m_webhookTask.spawn(
            req.post(url),
            [ssPath](web::WebResponse res) {
                std::error_code ec;
                fs::remove(ssPath, ec);
                if (res.ok()) log::info("sent");
                else log::error("rip: {}", res.code());
            }
        );
    }
};
