// PT: Smoke test de capa (M4h): sidecar de thumb ao lado da faixa é achado
//     primeiro; faixa inexistente ou sem arte retorna nullopt. A extração
//     embutida positiva é exercida no gate real (download com thumb).
// EN: Cover smoke test (M4h): the thumbnail sidecar next to the track is
//     found first; missing track or no art returns nullopt. Positive
//     embedded extraction is exercised by the real gate (thumb download).
// ES: Smoke test de carátula (M4h): el sidecar junto a la pista se encuentra
//     primero; sin pista o sin arte, nullopt.
// ZH: 封面冒烟测试（M4h）：曲目旁的缩略图 sidecar 被优先找到；曲目缺失或
//     无内嵌图返回 nullopt。内嵌提取的正路径由真实门槛（带缩略图下载）
//     覆盖。
#include "library/cover.hpp"

#include <cstdio>
#include <filesystem>
#include <fstream>

namespace
{

int failures = 0;

void check(bool cond, const char *what)
{
        if (!cond)
        {
                std::fprintf(stderr, "cover_smoke: FAIL: %s\n", what);
                ++failures;
        }
}

} // namespace

int main()
{
        const std::string dir = "/tmp/kiutube_cover_smoke";
        std::filesystem::remove_all(dir);
        std::filesystem::create_directories(dir);

        // PT: faixa + sidecar de thumb: capa = o sidecar.
        // EN: track + thumbnail sidecar: cover = the sidecar.
        // ES: pista + sidecar: carátula = el sidecar. / ZH: 曲目 + sidecar。
        {
                std::ofstream(dir + "/Bee Gees - Songbird [8xCLvL].webm").put('\0');
                std::ofstream(dir + "/Bee Gees - Songbird [8xCLvL].webp").put('\0');
                const auto art = library::cover_for(dir + "/Bee Gees - Songbird [8xCLvL].webm");
                check(art.has_value() && art->find(".webp") != std::string::npos, "sidecar achado primeiro");
        }

        // PT: sem sidecar e sem arte embutida (wav não suportado): nullopt.
        // EN: no sidecar and no embedded art (wav unsupported): nullopt.
        // ES: sin sidecar y sin arte: nullopt. / ZH: 无 sidecar 且无内嵌图：nullopt。
        {
                std::ofstream(dir + "/plain.wav").put('\0');
                check(!library::cover_for(dir + "/plain.wav").has_value(), "wav sem arte -> nullopt");
        }

        // PT: faixa inexistente: nullopt (sem crash).
        // EN: missing track: nullopt (no crash).
        check(!library::cover_for(dir + "/nao-existe.webm").has_value(), "inexistente -> nullopt");

        std::filesystem::remove_all(dir);
        if (failures > 0)
                return 1;
        std::printf("cover_smoke: PASS\n");
        return 0;
}
