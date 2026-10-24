// PT: Implementação MPRIS sobre sdbus-c++ (v2). Uma conexão no barramento
//     de sessão como org.mpris.MediaPlayer2.kiutube, objeto
//     /org/mpris/MediaPlayer2, interfaces MediaPlayer2 (mínimo) e Player.
//     Poller a cada 1s compara (faixa, estado, posição aproximada) e emite
//     PropertiesChanged quando muda. Thread-safe via API do engine (mutex).
// EN: MPRIS implementation on sdbus-c++ (v2). One session-bus connection as
//     org.mpris.MediaPlayer2.kiutube, object /org/mpris/MediaPlayer2,
//     MediaPlayer2 (minimal) and Player interfaces. A 1s poller compares
//     (track, state, approx position) and emits PropertiesChanged on
//     change. Thread-safe through the engine's mutexed API.
// ES: Implementación MPRIS sobre sdbus-c++ (v2). Conexión en el bus de
//     sesión, objeto /org/mpris/MediaPlayer2, poller de 1s con
//     PropertiesChanged.
// ZH: 基于 sdbus-c++（v2）的 MPRIS 实现。以 org.mpris.MediaPlayer2.kiutube
//     连接会话总线，对象 /org/mpris/MediaPlayer2；1s 轮询比较（曲目、
//     状态、近似位置），变化时发出 PropertiesChanged。
#include "sys/mpris.hpp"

#include "audio/engine.hpp"
#include "krt/log.hpp"

#ifdef KIUTUBE2_WITH_MPRIS

#include <sdbus-c++/sdbus-c++.h>

#include <atomic>
#include <chrono>
#include <string>
#include <thread>

namespace sys
{
namespace
{

constexpr const char *kPlayerIface = "org.mpris.MediaPlayer2.Player";
constexpr const char *kRootIface = "org.mpris.MediaPlayer2";

} // namespace

struct Mpris::Impl
{
        explicit Impl(audio::Engine &e) : engine(e)
        {
        }

        // PT: Estado espelhado para detectar mudanças no poller.
        // EN: Mirrored state for the poller's change detection.
        // ES: Estado espejado para detectar cambios en el poller.
        // ZH: 供轮询检测变化用的镜像状态。
        std::string last_title;
        bool last_playing = false;
        bool had_track = false;

        [[nodiscard]] std::string playback_status() const
        {
                const auto cur = engine.current();
                if (cur.path.empty())
                        return "Stopped";
                return engine.is_playing() ? "Playing" : "Paused";
        }

        // PT: Metadados como a{sv} (mapa) — o getter deduz a assinatura do tipo
        //     de retorno; Variant resolveria para "v", errado p/ Metadata.
        // EN: Metadata as a{sv} (map) — the getter deduces the signature from
        //     the return type; Variant would resolve to "v", wrong for Metadata.
        // ZH: Metadata 用 a{sv}（map）——getter 从返回类型推导签名；
        //     Variant 会推导成 "v"，对 Metadata 是错的。
        [[nodiscard]] std::map<std::string, sdbus::Variant> metadata() const
        {
                const auto cur = engine.current();
                std::map<std::string, sdbus::Variant> md;
                if (cur.path.empty())
                        return md;
                md["mpris:trackid"] = sdbus::Variant(sdbus::ObjectPath("/org/mpris/MediaPlayer2/track/0"));
                md["xesam:title"] = sdbus::Variant(cur.title);
                md["xesam:artist"] = sdbus::Variant(std::vector<std::string>{artist_hint(cur.title)});
                md["mpris:length"] = sdbus::Variant(
                    static_cast<int64_t>(std::chrono::duration_cast<std::chrono::microseconds>(cur.duration)
                                             .count()));
                return md;
        }

        void emit_changes(sdbus::IObject &o)
        {
                // PT: emite buscando os valores atuais via getters registrados.
                // EN: emits by fetching current values through the registered getters.
                // PT: initializer_list copia-inicializa os elementos (ctor explicit
                //     não serve) — monta o vetor com emplace_back.
                // EN: initializer_list copy-initializes elements (explicit ctor
                //     won't do) — build the vector with emplace_back.
                // ZH: initializer_list 是拷贝初始化（explicit 构造函数不行）——
                //     用 emplace_back 组装 vector。
                std::vector<sdbus::PropertyName> props;
                props.emplace_back("PlaybackStatus");
                props.emplace_back("Metadata");
                o.emitPropertiesChangedSignal(kPlayerIface, props);
        }

        // PT: "Artista - Título [id].ext" -> artista (antes do 1º " - "), como
        //     no split_artist_title do app. / EN: filename artist hint.
        // ZH: 文件名中的艺人提示（第一个 " - " 之前）。
        [[nodiscard]] static std::string artist_hint(const std::string &title)
        {
                const auto dash = title.find(" - ");
                if (dash == std::string::npos)
                        return "";
                return title.substr(0, dash);
        }

        audio::Engine &engine;
        std::unique_ptr<sdbus::IConnection> conn;
        std::unique_ptr<sdbus::IObject> obj;
        std::atomic<bool> stop{false};
        std::thread poll_th;
};

Mpris::Mpris(audio::Engine &engine) : impl_(std::make_unique<Impl>(engine))
{
        try
        {
                // PT: v2: createSessionBusConnection(ServiceName); ObjectPath tem
                //     ctor explicit de const char*.
                // EN: v2: createSessionBusConnection(ServiceName); ObjectPath has
                //     an explicit const char* ctor.
                // ZH: v2：createSessionBusConnection(ServiceName)；ObjectPath 的
                //     const char* 构造函数是 explicit。
                impl_->conn = sdbus::createSessionBusConnection(
                    sdbus::ServiceName("org.mpris.MediaPlayer2.kiutube"));
                impl_->obj = sdbus::createObject(*impl_->conn,
                                                 sdbus::ObjectPath("/org/mpris/MediaPlayer2"));

                // PT: Interface raiz mínima (CanQuit/Identity para quem lista players).
                // EN: Minimal root interface (CanQuit/Identity for player listings).
                impl_->obj
                    ->addVTable(sdbus::registerProperty("CanQuit").withGetter([] { return false; }),
                                sdbus::registerProperty("Identity")
                                    .withGetter([] { return std::string("kiutube2"); }))
                    .forInterface(sdbus::InterfaceName(kRootIface));

                // PT: Propriedades + métodos do Player (getters via lambdas — engine é
                //     thread-safe). VTable: no v2 do sdbus-c++ o registro é por
                //     addVTable()/forInterface(), e a assinatura é deduzida dos tipos.
                // EN: Player properties + methods (getters as lambdas — the engine is
                //     thread-safe). VTable: sdbus-c++ v2 registers via
                //     addVTable()/forInterface(), with the signature deduced from types.
                // ZH: Player 属性 + 方法（getter 为 lambda —— 引擎线程安全）。v2 通过
                //     addVTable()/forInterface() 注册，签名由类型推导。
                impl_->obj
                    ->addVTable(sdbus::registerProperty("PlaybackStatus")
                                    .withGetter([this] { return impl_->playback_status(); }),
                                sdbus::registerProperty("Metadata").withGetter([this] { return impl_->metadata(); }),
                                sdbus::registerProperty("Position")
                                    .withGetter([this] {
                                            return static_cast<int64_t>(
                                                std::chrono::duration_cast<std::chrono::microseconds>(
                                                    impl_->engine.position())
                                                    .count());
                                    }),
                                sdbus::registerProperty("Volume")
                                    .withGetter([this] { return static_cast<double>(impl_->engine.volume()); })
                                    .withSetter([this](const double v) { impl_->engine.set_volume(static_cast<float>(v)); }),
                                sdbus::registerProperty("CanPlay").withGetter([] { return true; }),
                                sdbus::registerProperty("CanPause").withGetter([] { return true; }),
                                sdbus::registerProperty("CanGoNext").withGetter([] { return true; }),
                                sdbus::registerProperty("CanGoPrevious").withGetter([] { return true; }),
                                sdbus::registerProperty("CanSeek").withGetter([] { return true; }),
                                sdbus::registerMethod("Play").implementedAs([this] { impl_->engine.play(); }),
                                sdbus::registerMethod("Pause").implementedAs([this] { impl_->engine.pause(); }),
                                sdbus::registerMethod("PlayPause")
                                    .implementedAs([this] {
                                            if (impl_->engine.is_playing())
                                                    impl_->engine.pause();
                                            else
                                                    impl_->engine.play();
                                    }),
                                sdbus::registerMethod("Next").implementedAs([this] { impl_->engine.next(); }),
                                sdbus::registerMethod("Previous").implementedAs([this] { impl_->engine.next(); }),
                                sdbus::registerMethod("Seek")
                                    .implementedAs([this](const int64_t offset_us) {
                                            impl_->engine.seek_by(std::chrono::microseconds(offset_us));
                                    }),
                                sdbus::registerMethod("Stop").implementedAs([this] { impl_->engine.pause(); }))
                    .forInterface(sdbus::InterfaceName(kPlayerIface));

                impl_->conn->enterEventLoopAsync();
                krt::log::info("mpris: org.mpris.MediaPlayer2.kiutube no barramento / on the bus / en el bus / 已注册");

                // PT: Poller de mudanças (faixa/estado): emite PropertiesChanged.
                // EN: Change poller (track/state): emits PropertiesChanged.
                impl_->poll_th = std::thread([this] {
                        while (!impl_->stop.load())
                        {
                                std::this_thread::sleep_for(std::chrono::seconds(1));
                                const auto cur = impl_->engine.current();
                                const bool playing = impl_->engine.is_playing();
                                const bool has = !cur.path.empty();
                                if (has != impl_->had_track || cur.title != impl_->last_title ||
                                    playing != impl_->last_playing)
                                {
                                        impl_->had_track = has;
                                        impl_->last_title = cur.title;
                                        impl_->last_playing = playing;
                                        impl_->emit_changes(*impl_->obj);
                                }
                        }
                });
        }
        catch (const std::exception &e)
        {
                krt::log::error("mpris: falha / failed / fallo / 失败: {}", e.what());
        }
}

Mpris::~Mpris()
{
        if (impl_->conn)
        {
                impl_->stop.store(true);
                if (impl_->poll_th.joinable())
                        impl_->poll_th.join();
                impl_->conn->leaveEventLoop();
        }
}

bool Mpris::running() const
{
        return impl_->conn != nullptr;
}

} // namespace sys

#else // PT: sem sdbus: stub / stub without sdbus / 无 sdbus 时的 stub

#include "krt/log.hpp"

namespace sys
{

struct Mpris::Impl
{
};

Mpris::Mpris(audio::Engine &) : impl_(std::make_unique<Impl>())
{
        krt::log::warn("mpris: sdbus-c++ ausente / missing / ausente / 缺失 (stub)");
}

Mpris::~Mpris() = default;
bool Mpris::running() const
{
        return false;
}

} // namespace sys

#endif // KIUTUBE2_WITH_MPRIS
