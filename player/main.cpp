// PT: kiutube2 — marco M0: abre o terminal, renderiza um frame, ecoa a tecla
//     pressionada e sai em q/ESC/F10. Prova a fundação: scheduler krt +
//     terminal layer própria, sem GLib, sem termbox2, sem o código C legado.
// EN: kiutube2 — milestone M0: opens the terminal, renders one frame, echoes
//     the pressed key and exits on q/ESC/F10. Proves the foundation: krt
//     scheduler + own terminal layer, no GLib, no termbox2, no legacy C code.
// ES: kiutube2 — hito M0: abre la terminal, renderiza un frame, hace eco de la
//     tecla pulsada y sale con q/ESC/F10. Prueba la fundación: scheduler krt +
//     terminal layer propia, sin GLib, sin termbox2, sin el código C legado.
// ZH: kiutube2 —— 里程碑 M0：打开终端，渲染一帧，回显按键，并在 q/ESC/F10 时退出。
//     验证基础架构：krt 调度器 + 自研终端层，无 GLib、无 termbox2、无旧 C 代码。
#include "krt/scheduler.hpp"
#include "tui/cell_buffer.hpp"
#include "tui/term.hpp"

#include <cstdio>
#include <exception>
#include <string>

namespace
{

// PT: Identificador neutro da tecla (sem palavras traduzíveis — as strings de
//     UI traduzíveis chegam com o i18n no M1).
// EN: Neutral key identifier (no translatable words — translated UI strings
//     land with i18n in M1).
// ES: Identificador neutro de la tecla (sin palabras traducibles — las cadenas
//     de UI traducibles llegan con el i18n en M1).
// ZH: 键的中性标识（不含可翻译词汇 —— 可翻译的 UI 字符串随 M1 的 i18n 加入）。
std::string key_id(const tui::KeyEvent &ev)
{
        if (ev.key == tui::Key::Char)
                return std::string(1, static_cast<char>(ev.ch)); // M0: ASCII no status
        switch (ev.key)
        {
        case tui::Key::Enter:
                return "Enter";
        case tui::Key::Tab:
                return "Tab";
        case tui::Key::Backspace:
                return "Backspace";
        case tui::Key::Esc:
                return "Esc";
        case tui::Key::Up:
                return "Up";
        case tui::Key::Down:
                return "Down";
        case tui::Key::Left:
                return "Left";
        case tui::Key::Right:
                return "Right";
        case tui::Key::Home:
                return "Home";
        case tui::Key::End:
                return "End";
        case tui::Key::Insert:
                return "Insert";
        case tui::Key::Delete:
                return "Delete";
        case tui::Key::PageUp:
                return "PageUp";
        case tui::Key::PageDown:
                return "PageDown";
        case tui::Key::F1: return "F1"; case tui::Key::F2: return "F2";
        case tui::Key::F3: return "F3"; case tui::Key::F4: return "F4";
        case tui::Key::F5: return "F5"; case tui::Key::F6: return "F6";
        case tui::Key::F7: return "F7"; case tui::Key::F8: return "F8";
        case tui::Key::F9: return "F9"; case tui::Key::F10: return "F10";
        case tui::Key::F11: return "F11"; case tui::Key::F12: return "F12";
        default: return "?";
        }
}

krt::Task<void> app_main()
{
        tui::Term term;
        tui::CellBuffer buf(term.rows(), term.cols());

        buf.clear();
        buf.put(1, 2, "kiutube2 - marco M0 / milestone M0 / hito M0 / \xe9\x87\x8c\xe7\xa8\x8b\xe7\xa2\x9f M0");
        buf.put(3, 2, "PT: fundacao C++20 (scheduler + terminal) - reescrita kiutube");
        buf.put(4, 2, "EN: C++20 foundation (scheduler + terminal) - kiutube rewrite");
        buf.put(5, 2, "ES: cimientos C++20 (scheduler + terminal) - reescritura kiutube");
        buf.put(6, 2, "ZH: C++20 \xe5\x9f\xba\xe7\xa1\x80\xef\xbc\x88\xe8\xb0\x83\xe5\xba\xa6\xe5\x99\xa8 + \xe7\xbb\x88\xe7\xab\xaf\xef\xbc\x89 - kiutube \xe9\x87\x8d\xe5\x86\x99");
        term.write_segments(buf.flush_diff());

        for (;;)
        {
                const auto ev = co_await term.next_event();
                const bool quit = ev.key == tui::Key::Esc || ev.key == tui::Key::F10 ||
                                  (ev.key == tui::Key::Char && (ev.ch == 'q' || ev.ch == 'Q'));

                buf.clear();
                buf.put(1, 2, "kiutube2 - M0");
                buf.put(3, 2, "PT/EN/ES/ZH: reescrita C++20 - fundacao / foundation / cimientos / \xe5\x9f\xba\xe7\xa1\x80");
                buf.put(8, 2, "[" + key_id(ev) + "]");
                if (quit)
                        break;
                term.write_segments(buf.flush_diff());
        }

        krt::Scheduler::instance().request_stop();
}

} // namespace

int main()
{
        try
        {
                auto app = app_main();
                app.start();
                krt::Scheduler::instance().run();
                return 0;
        }
        catch (const std::exception &e)
        {
                std::fprintf(stderr, "kiutube2: %s\n", e.what());
                return 1;
        }
}
