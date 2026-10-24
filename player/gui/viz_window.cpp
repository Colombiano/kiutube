// PT: Implementação da janela gráfica. Duas páginas (QStackedWidget)
//     espelhando os referenciais: FFT Spectrum (barras 1/3-oct verdes com
//     eixo x log + curva de pico rosa, como fft.png) e Spectrograma 2D
//     (QCPColorMap com gradiente inferno exato + colorbar, como
//     espectrograma2d.png). Sem classes Q_OBJECT próprias: tudo via
//     lambdas + QTimer, então o MOC só processa o QCustomPlot.
// EN: GUI window implementation. Two pages (QStackedWidget) mirroring the
//     references: FFT Spectrum (green 1/3-oct bars with log x axis + pink
//     peak curve, like fft.png) and a 2D spectrogram (QCPColorMap with the
//     exact inferno gradient + colorbar, like espectrograma2d.png). No
//     Q_OBJECT classes of our own: everything through lambdas + QTimer, so
//     moc only processes QCustomPlot.
// ES: Implementación de la ventana gráfica. Dos páginas (QStackedWidget)
//     espejando las referencias: FFT Spectrum y espectrograma 2D
//     (QCPColorMap con gradiente inferno exacto + colorbar). Sin clases
//     Q_OBJECT propias: todo vía lambdas + QTimer.
// ZH: 图形窗口实现。两个页面（QStackedWidget）还原参考图：FFT 频谱
//（绿色 1/3 倍频程柱 + 对数 x 轴 + 粉色峰值曲线，类似 fft.png）与
//     2D 频谱图（QCPColorMap + 精确 inferno 渐变 + 色条，类似
//     espectrograma2d.png）。不使用自定义 Q_OBJECT 类：全部经由
//     lambda + QTimer，moc 仅处理 QCustomPlot。
#include "gui/viz_window.hpp"

#include "audio/engine.hpp"
#include "krt/log.hpp"
#include "krt/scheduler.hpp"
#include "tui/color.hpp"

#ifdef KIUTUBE2_WITH_QT

#include <QApplication>
#include <QMainWindow>
#include <QStackedWidget>
#include <QTimer>

#include "qcustomplot.h"

#include <cmath>
#include <cstdlib>
#include <deque>
#include <thread>

namespace gui
{
namespace
{

// PT: dB por página (faixas dos referenciais do usuário).
// EN: dB per page (the user's reference ranges).
// ES: dB por página (rangos de las referencias del usuario).
// ZH: 各页面的 dB 范围（用户参考图）。
constexpr double kSpecDbMin = -100.0, kSpecDbMax = -20.0;
constexpr double kSonoDbMin = -120.0, kSonoDbMax = -30.0;

// PT: grade do sonograma (colunas de tempo × fileiras de freq).
// EN: sonogram grid (time columns × frequency rows).
// ES: grilla del sonograma. / ZH: 声谱图网格（时间列 × 频率行）。
constexpr int kSonoCols = 160;
constexpr int kSonoRows = 192;
constexpr double kSecondsPerCol = 1.0 / 20.0; // PT: alinhado ao tick de 50ms / aligned with the 50ms tick

// PT: estado compartilhado entre a thread do kiutube2 e a thread GUI.
// EN: state shared between the kiutube2 thread and the GUI thread.
// ES: estado compartido entre el hilo de kiutube2 y el de la GUI.
// ZH: kiutube2 线程与 GUI 线程共享的状态。
struct Shared
{
        std::atomic<int> mode{0}; // PT: 0=fechar 1=spectrum 2=spectrogram / 0=close 1=spectrum 2=spectrogram
        std::atomic<bool> open{false};
};

// PT: Converte a LUT inferno (tui::inferno) em stops do QCPColorGradient.
// EN: Converts the inferno LUT (tui::inferno) into QCPColorGradient stops.
// ES: Convierte la LUT inferno en stops de QCPColorGradient.
// ZH: 将 inferno LUT（tui::inferno）转换为 QCPColorGradient 色标。
void load_inferno_gradient(QCPColorGradient &g)
{
        g.setLevelCount(256);
        for (int i = 0; i < 256; ++i)
        {
                const tui::Rgb c = tui::inferno(static_cast<double>(i) / 255.0);
                g.setColorStopAt(static_cast<double>(i) / 255.0, QColor(c.r, c.g, c.b));
        }
}

// PT: Corpo da thread GUI: QApplication, janela e loop de eventos moram aqui.
// EN: GUI thread body: QApplication, window and event loop live here.
// ES: Cuerpo del hilo GUI: QApplication, ventana y loop de eventos viven aquí.
// ZH: GUI 线程主体：QApplication、窗口与事件循环均在此处。
void run_gui(audio::Engine &engine, Shared &sh, std::function<void()> on_closed)
{
        int argc = 1;
        char arg0[] = "kiutube2";
        char *argv[] = {arg0, nullptr};
        QApplication app(argc, argv);

        // PT: heap + DeleteOnClose: o X deleta a janela, 'destroyed' dispara e
        //     quitOnLastWindowClosed (default) faz exec() retornar.
        // EN: heap + DeleteOnClose: the X deletes the window, 'destroyed'
        //     fires and quitOnLastWindowClosed (default) makes exec() return.
        // ES: heap + DeleteOnClose: la X elimina la ventana y exec() retorna.
        // ZH: 堆分配 + DeleteOnClose：点击 X 删除窗口，触发 destroyed，
        //     quitOnLastWindowClosed（默认）使 exec() 返回。
        auto *win = new QMainWindow;
        win->setAttribute(Qt::WA_DeleteOnClose);
        win->setWindowTitle("kiutube2 — visualizer / visualizador / 可视化器");
        win->resize(1150, 540);

        viz::Visualizer analyzer; // PT: só a pipeline FFT (analyze + mags) / FFT pipeline only
        analyzer.set_mode(viz::Mode::Spectrum);

        // ---- Página 1: FFT Spectrum (estilo fft.png) ----
        auto *spectrum = new QCustomPlot;
        spectrum->xAxis->setScaleType(QCPAxis::stLogarithmic);
        spectrum->xAxis->setTicker(QSharedPointer<QCPAxisTickerLog>(new QCPAxisTickerLog));
        spectrum->xAxis->setRange(viz::Visualizer::kFreqMin, viz::Visualizer::kFreqMax);
        spectrum->yAxis->setRange(kSpecDbMin, kSpecDbMax);
        spectrum->xAxis->setLabel("Frequency (Hz)");
        spectrum->yAxis->setLabel("PSD (dB)");
        auto *bars = new QCPBars(spectrum->xAxis, spectrum->yAxis);
        bars->setPen(Qt::NoPen);
        bars->setBrush(QColor(40, 200, 70, 200)); // PT: verde do fft.png
        bars->setWidthType(QCPBars::wtAbsolute);
        bars->setWidth(14);
        auto *peak = spectrum->addGraph();
        peak->setPen(QPen(QColor(255, 110, 150), 2)); // PT: rosa do fft.png
        peak->setLineStyle(QCPGraph::lsLine);
        peak->setBrush(Qt::NoBrush);

        // ---- Página 2: Spectrograma 2D (estilo espectrograma2d.png) ----
        auto *sono = new QCustomPlot;
        sono->xAxis->setRange(0, kSonoCols * kSecondsPerCol);
        sono->xAxis->setLabel("Time (s)");
        // PT: eixo y em log10(f) com ticker de texto (100/1k/10k) — geometria
        //     log correta no color map sem subclassing.
        // EN: y axis in log10(f) with a text ticker (100/1k/10k) — correct
        //     log geometry on the color map without subclassing.
        const double y_lo = std::log10(viz::Visualizer::kFreqMin);
        const double y_hi = std::log10(viz::Visualizer::kFreqMax);
        sono->yAxis->setRange(y_lo, y_hi);
        sono->yAxis->setLabel("Frequency (Hz)");
        auto ticker = QSharedPointer<QCPAxisTickerText>(new QCPAxisTickerText);
        for (const double f : {50.0, 100.0, 200.0, 500.0, 1000.0, 2000.0, 5000.0, 10000.0})
                ticker->addTick(std::log10(f), QString::number(static_cast<int>(f)));
        sono->yAxis->setTicker(ticker);
        auto *color_map = new QCPColorMap(sono->xAxis, sono->yAxis);
        color_map->data()->setSize(kSonoCols, kSonoRows);
        color_map->data()->setRange(QCPRange(0, kSonoCols * kSecondsPerCol), QCPRange(y_lo, y_hi));
        QCPColorGradient gradient;
        load_inferno_gradient(gradient);
        color_map->setGradient(gradient);
        color_map->setDataRange(QCPRange(kSonoDbMin, kSonoDbMax));
        color_map->setInterpolate(false);
        color_map->setTightBoundary(false);
        auto *color_scale = new QCPColorScale(sono);
        sono->plotLayout()->addElement(0, 1, color_scale);
        color_scale->setType(QCPAxis::atRight);
        color_scale->setDataRange(QCPRange(kSonoDbMin, kSonoDbMax));
        color_scale->setGradient(gradient);
        color_scale->axis()->setLabel("PSD (dB)");
        color_map->setColorScale(color_scale);

        auto *stack = new QStackedWidget;
        stack->addWidget(spectrum);
        stack->addWidget(sono);
        win->setCentralWidget(stack);
        win->show(); // PT: sem isso exec() roda sem janela e close() falha / without it exec runs windowless and close() crashes

        // ---- Estado rolling ----
        std::deque<std::vector<double>> hist; // PT: colunas de potência (dB) / power columns (dB)
        std::vector<double> peaks(viz::Visualizer::kBandCount, kSpecDbMin);
        std::vector<double> keys(viz::Visualizer::kBandCount);
        std::vector<double> vals(viz::Visualizer::kBandCount);
        for (int b = 0; b < viz::Visualizer::kBandCount; ++b)
                keys[b] = std::sqrt(viz::Visualizer::band_edge(b) * viz::Visualizer::band_edge(b + 1));

        float chunk[16384];
        QTimer timer;
        QObject::connect(&timer, &QTimer::timeout, [&] {
                const int mode = sh.mode.load();
                if (mode == 0)
                {
                        win->close();
                        return;
                }
                stack->setCurrentIndex(mode - 1);

                const std::size_t got = engine.read_viz({chunk, 16384});
                if (got > 0)
                        analyzer.feed({chunk, got * 2});
                analyzer.analyze();
                const auto &mags = analyzer.mags();

                if (mode == 1)
                {
                        for (int b = 0; b < viz::Visualizer::kBandCount; ++b)
                        {
                                const int bin_lo = std::clamp(
                                    static_cast<int>(viz::Visualizer::band_edge(b) * viz::Visualizer::kFftSize /
                                                     viz::Visualizer::kSampleRate),
                                    1, viz::Visualizer::kFftSize / 2);
                                const int bin_hi = std::clamp(
                                    static_cast<int>(viz::Visualizer::band_edge(b + 1) * viz::Visualizer::kFftSize /
                                                     viz::Visualizer::kSampleRate),
                                    1, viz::Visualizer::kFftSize / 2);
                                double g = 0.0;
                                for (int bin = bin_lo; bin <= bin_hi; ++bin)
                                        g = std::max(g, mags[bin]);
                                double db = 20.0 * std::log10(g + 1e-9);
                                db = std::clamp(db, kSpecDbMin, kSpecDbMax);
                                peaks[b] = std::max(peaks[b] - 1.5, db);
                                vals[b] = db;
                        }
                        bars->setData(QVector<double>(keys.begin(), keys.end()),
                                      QVector<double>(vals.begin(), vals.end()));
                        peak->setData(QVector<double>(keys.begin(), keys.end()),
                                      QVector<double>(peaks.begin(), peaks.end()));
                        spectrum->replot();
                }
                else
                {
                        // PT: nova coluna: potência por fileira em log10(f).
                        // EN: new column: power per row in log10(f).
                        // ES: nueva columna: potencia por fila en log10(f).
                        // ZH: 新列：按 log10(f) 的每行功率。
                        std::vector<double> column(kSonoRows);
                        for (int r = 0; r < kSonoRows; ++r)
                        {
                                const double f = std::pow(10.0, y_lo + (y_hi - y_lo) * r / (kSonoRows - 1));
                                const int bin = std::clamp(
                                    static_cast<int>(f * viz::Visualizer::kFftSize / viz::Visualizer::kSampleRate), 1,
                                    viz::Visualizer::kFftSize / 2);
                                double db = 20.0 * std::log10(mags[bin] + 1e-9);
                                column[r] = std::clamp(db, kSonoDbMin, kSonoDbMax);
                        }
                        hist.push_back(std::move(column));
                        while (static_cast<int>(hist.size()) > kSonoCols)
                                hist.pop_front();
                        for (int x = 0; x < static_cast<int>(hist.size()); ++x)
                                for (int r = 0; r < kSonoRows; ++r)
                                        color_map->data()->setCell(x, r, hist[x][r]);
                        color_map->rescaleKeyAxis(false);
                        sono->xAxis->setRange(0, kSonoCols * kSecondsPerCol);
                        sono->replot();
                }
        });
        timer.start(50);

        // PT: fechamento pela janela (X -> destruição): avisa o scheduler uma
        //     única vez. Lambda mutable para mover o callback capturado.
        // EN: closing from the window (X -> destruction): notifies the
        //     scheduler once. Mutable lambda to move the captured callback.
        // ES: cierre por la ventana (X -> destrucción): avisa al scheduler
        //     una vez. Lambda mutable para mover el callback.
        // ZH: 从窗口关闭（X -> 销毁）：仅通知调度器一次。mutable lambda 以便
        //     移动捕获的回调。
        bool *closed_flag = new bool(false);
        QObject::connect(win, &QObject::destroyed, &app,
                         [closed_flag, &sh, cb = std::move(on_closed)]() mutable {
                                 sh.open.store(false);
                                 if (!*closed_flag)
                                 {
                                         *closed_flag = true;
                                         if (cb)
                                                 krt::Scheduler::instance().post_function(std::move(cb));
                                 }
                         });

        sh.open.store(true);
        krt::log::info("gui: janela aberta / window open / ventana abierta / 窗口已打开");
        app.exec();
        sh.open.store(false);
        delete closed_flag;
        krt::log::info("gui: janela fechada / window closed / ventana cerrada / 窗口已关闭");
}

} // namespace

struct VizWindow::Impl
{
        Impl(audio::Engine &e, std::function<void()> cb) : engine(e), on_closed(std::move(cb))
        {
        }

        audio::Engine &engine;
        std::function<void()> on_closed;
        Shared sh;
        std::thread th;
};

VizWindow::VizWindow(audio::Engine &engine, std::function<void()> on_closed)
    : impl_(std::make_unique<Impl>(engine, std::move(on_closed)))
{
}

VizWindow::~VizWindow()
{
        impl_->sh.mode.store(0);
        for (int i = 0; impl_->th.joinable() && i < 200 && impl_->sh.open.load(); ++i)
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
        if (impl_->th.joinable())
                impl_->th.join();
}

void VizWindow::show()
{
        if (impl_->th.joinable())
                return;
        impl_->sh.mode.store(1);
        impl_->sh.open.store(false);
        impl_->th = std::thread(run_gui, std::ref(impl_->engine), std::ref(impl_->sh), impl_->on_closed);
}

void VizWindow::set_mode(viz::Mode m)
{
        // PT: KIUTUBE2_NO_GUI=1 força o caminho sem janela (testes do viz
        //     CLI, ambientes sem display): mesma API, sem Qt.
        // EN: KIUTUBE2_NO_GUI=1 forces the windowless path (CLI viz tests,
        //     headless setups): same API, no Qt.
        // ES: KIUTUBE2_NO_GUI=1 fuerza el camino sin ventana (pruebas del
        //     viz CLI): misma API, sin Qt.
        // ZH: KIUTUBE2_NO_GUI=1 强制走无窗口路径（CLI viz 测试、无显示
        //     环境）：同一 API，不使用 Qt。
        static const bool no_gui = std::getenv("KIUTUBE2_NO_GUI") != nullptr;
        if (no_gui)
        {
                if (m != viz::Mode::Off)
                        krt::log::warn("gui: KIUTUBE2_NO_GUI=1, janela desativada / window disabled / 窗口已禁用");
                return;
        }
        if (m != viz::Mode::Off && !impl_->th.joinable())
                show();
        impl_->sh.mode.store(static_cast<int>(m));
}

bool VizWindow::is_open() const
{
        return impl_->sh.open.load();
}

} // namespace gui

#else // PT: sem Qt: stub que ignora tudo / no Qt: no-op stub

#include "krt/log.hpp"

namespace gui
{

struct VizWindow::Impl
{
        audio::Engine &engine;
        std::function<void()> on_closed;
};

VizWindow::VizWindow(audio::Engine &engine, std::function<void()> on_closed)
    : impl_(std::make_unique<Impl>(Impl{engine, std::move(on_closed)}))
{
        krt::log::warn("gui: Qt nao encontrada / Qt not found / Qt no encontrada / 未找到 Qt");
}

VizWindow::~VizWindow() = default;
void VizWindow::show() {}
void VizWindow::set_mode(viz::Mode) {}
bool VizWindow::is_open() const
{
        return false;
}

} // namespace gui

#endif // KIUTUBE2_WITH_QT
