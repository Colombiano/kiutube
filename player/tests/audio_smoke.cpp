// PT: Smoke test do decoder (M2): gera um WAV (1s, 44,1kHz, stereo, seno de
//     440Hz) em disco, abre com MaDecoder, lê reamostrado para 48kHz, testa
//     seek e fim de faixa. Gate de regressão do caminho de decode.
// EN: Decoder smoke test (M2): writes a WAV file (1s, 44.1kHz, stereo, 440Hz
//     sine) to disk, opens it with MaDecoder, reads resampled to 48kHz, tests
//     seek and end-of-track. Regression gate for the decode path.
// ES: Smoke test del decoder (M2): genera un WAV (1s, 44,1kHz, stereo, seno
//     de 440Hz) en disco, lo abre con MaDecoder, lee remuestreado a 48kHz,
//     prueba seek y fin de pista. Gate de regresión del camino de decode.
// ZH: 解码器冒烟测试（M2）：在磁盘上生成 WAV 文件（1 秒、44.1kHz、立体声、
//     440Hz 正弦），用 MaDecoder 打开，重采样读到 48kHz，测试 seek 与曲目
//     结束。解码路径的回归门槛。
#include "audio/decoder.hpp"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

using namespace std::chrono_literals;

namespace
{

int failures = 0;

void check(bool cond, const char *what)
{
        if (!cond)
        {
                std::fprintf(stderr, "audio_smoke: FAIL: %s\n", what);
                ++failures;
        }
}

// PT: Escreve um WAV PCM16 mínimo e válido. / EN: Writes a minimal valid PCM16 WAV.
// ES: Escribe un WAV PCM16 mínimo y válido. / ZH: 写一个最小合法的 PCM16 WAV。
bool write_wav(const std::string &path)
{
        const int rate = 44100;
        const int seconds = 1;
        const int frames = rate * seconds;
        std::ofstream f(path, std::ios::binary);
        if (!f)
                return false;

        const auto put_u32 = [&f](std::uint32_t v) { f.write(reinterpret_cast<const char *>(&v), 4); };
        const auto put_u16 = [&f](std::uint16_t v) { f.write(reinterpret_cast<const char *>(&v), 2); };

        f.write("RIFF", 4);
        const std::uint32_t data_bytes = frames * 2 * 2;
        put_u32(36 + data_bytes);
        f.write("WAVE", 4);
        f.write("fmt ", 4);
        put_u32(16);
        put_u16(1); // PCM
        put_u16(2); // stereo
        put_u32(rate);
        put_u32(rate * 2 * 2);
        put_u16(4);
        put_u16(16);
        f.write("data", 4);
        put_u32(data_bytes);
        for (int i = 0; i < frames; ++i)
        {
                const double s = std::sin(2.0 * 3.141592653589793 * 440.0 * i / rate) * 0.5;
                const auto v = static_cast<std::int16_t>(s * 32767.0);
                put_u16(static_cast<std::uint16_t>(v));
                put_u16(static_cast<std::uint16_t>(v));
        }
        return static_cast<bool>(f);
}

} // namespace

int main()
{
        const std::string path = "/tmp/kiutube2_audio_smoke.wav";
        check(write_wav(path), "escrever wav / write wav / escribir wav / 写 WAV");

        audio::MaDecoder dec;
        check(dec.open(path, 48000), "abrir wav / open wav / abrir wav / 打开 WAV");

        const auto info = dec.info();
        check(!info.title.empty(), "titulo / title / título / 标题");
        check(info.duration >= 900ms && info.duration <= 1100ms, "duracao ~1s / duration ~1s / duración ~1s / 时长约 1 秒");

        // PT: Lê tudo: 1s reamostrado para 48k => ~48000 frames (tolerância ±200).
        // EN: Reads everything: 1s resampled to 48k => ~48000 frames (±200 tolerance).
        // ES: Lee todo: 1s remuestreado a 48k => ~48000 frames (tolerancia ±200).
        // ZH: 全部读完：重采样到 48k 的 1 秒 ≈ 48000 帧（容差 ±200）。
        std::vector<float> chunk(8192 * 2);
        std::size_t total = 0;
        for (int guard = 0; guard < 64; ++guard)
        {
                const std::size_t n = dec.read(chunk);
                total += n;
                if (n < chunk.size() / 2)
                        break;
        }
        check(total >= 47800 && total <= 48200, "total de frames ~48000 / frames ~48000 / frames ~48000 / 帧数约 48000");
        check(dec.at_end(), "fim de faixa / end of track / fin de pista / 曲目结束");

        // PT: Seek para 500ms e lê um pouco: deve voltar a ter áudio e não EOF imediato.
        // EN: Seek to 500ms and read a bit: audio again, not immediate EOF.
        // ES: Seek a 500ms y lee un poco: debe volver a haber audio y no EOF inmediato.
        // ZH: seek 到 500ms 再读一点：应恢复有声且不会立即 EOF。
        check(dec.seek(500ms), "seek 500ms");
        const std::size_t n_after = dec.read(chunk);
        check(n_after > 0 && !dec.at_end(), "leitura pos-seek / post-seek read / lectura post-seek / seek 后读取");

        if (failures > 0)
                return 1;
        std::printf("audio_smoke: PASS (frames=%zu / frames=%zu / frames=%zu / 帧数=%zu)\n", total, total,
                    total, total);
        return 0;
}
