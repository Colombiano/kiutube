#!/usr/bin/env python3
# PT: Harness PTY genérico de smoke-test para TUIs: dispara o binário num
#     pseudo-terminal, espera strings, envia teclas e asserta saída. Gate de
#     regressão de todos os marcos da reescrita C++20.
# EN: Generic PTY smoke-test harness for TUIs: spawns the binary on a pseudo
#     terminal, expects strings, sends keys and asserts exit. Regression gate
#     for every milestone of the C++20 rewrite.
# ES: Harness PTY genérico de smoke-test para TUIs: lanza el binario en un
#     pseudo-terminal, espera cadenas, envía teclas y asegura la salida. Gate
#     de regresión de todos los hitos de la reescritura C++20.
# ZH: 通用 PTY 冒烟测试 harness：在伪终端上启动二进制，等待字符串、发送按键并
#     断言退出。C++20 重写各里程碑的回归门槛。
#
# Uso / Usage / Uso / 用法:
#   tui_smoke.py BIN --expect STR [--send STR]... [--expect-exit SEC]
#
# Exemplo / Example / Ejemplo / 示例:
#   tui_smoke.py ./build/kiutube2 --expect "kiutube2" --send "x" --expect "[x]" \
#                --send "q" --expect-exit 3
import argparse
import fcntl
import os
import pty
import select
import struct
import subprocess
import sys
import termios
import time


def open_pty(rows, cols):
    master, slave = pty.openpty()
    fcntl.ioctl(slave, termios.TIOCSWINSZ, struct.pack("HHHH", rows, cols, 0, 0))
    return master, slave


def pump(master, out, duration):
    # PT: Lê o que estiver disponível por até `duration`s; retorna False em EOF.
    # EN: Reads whatever is available for up to `duration`s; returns False on EOF.
    # ES: Lee lo disponible hasta `duration`s; devuelve False en EOF.
    # ZH: 在 `duration` 秒内读取可用数据；EOF 时返回 False。
    end = time.time() + duration
    while time.time() < end:
        r, _, _ = select.select([master], [], [], 0.05)
        if not r:
            continue
        try:
            data = os.read(master, 65536)
        except OSError:
            return False
        if not data:
            return False
        out.append(data.decode("utf-8", "replace"))
        return True
    return True


def main():
    # PT: parsing manual em ORDEM: --expect/--send/--sh executam na sequência
    #     em que aparecem na linha de comando (zip por índice embaralhava o
    #     roteiro e enviava teclas na hora errada).
    # EN: Ordered manual parsing: --expect/--send/--sh execute in the exact
    #     sequence they appear on the command line (index-zipping used to
    #     scramble the script and sent keys at the wrong time).
    # ES: Parsing manual en ORDEN: --expect/--send/--sh ejecutan en la
    #     secuencia en que aparecen en la línea de comandos.
    # ZH: 手动按序解析：--expect/--send/--sh 严格按命令行中出现顺序执行
    #     （按索引 zip 会打乱步骤顺序，在错误的时间发送按键）。
    steps = []
    binary = None
    expect_exit = None
    timeout = 15.0
    rows, cols = 24, 100
    home = None
    argv = sys.argv[1:]
    i = 0
    while i < len(argv):
        a = argv[i]
        if a == "--expect":
            # PT: decodifica escapes (\x1b[.., \xE2\x96\x80) para comparar
            #     com o stream: latin-1 -> unicode_escape -> latin-1 -> utf-8.
            # EN: decodes escapes for stream comparison: latin-1 ->
            #     unicode_escape -> latin-1 -> utf-8.
            # ES: decodifica escapes para comparar con el stream.
            # ZH: 解码转义以比较输出流：latin-1 -> unicode_escape -> latin-1
            #     -> utf-8。
            steps.append(("expect", argv[i + 1].encode("latin-1").decode("unicode_escape")
                          .encode("latin-1").decode("utf-8", "replace"))); i += 2
        elif a == "--send":
            # PT: mesma cadeia de decodificação do --expect (escapes -> bytes).
            # EN: same decode chain as --expect (escapes -> bytes).
            # ES: misma cadena de decodificación que --expect.
            # ZH: 与 --expect 相同的解码链（转义 -> 字节）。
            steps.append(("send", argv[i + 1].encode("latin-1").decode("unicode_escape")
                          .encode("latin-1"))); i += 2
        elif a == "--sh":
            steps.append(("sh", argv[i + 1])); i += 2
        elif a == "--expect-exit":
            expect_exit = float(argv[i + 1]); i += 2
        elif a == "--timeout":
            timeout = float(argv[i + 1]); i += 2
        elif a == "--rows":
            rows = int(argv[i + 1]); i += 2
        elif a == "--cols":
            cols = int(argv[i + 1]); i += 2
        elif a == "--home":
            home = argv[i + 1]; i += 2
        else:
            binary = a; i += 1
    if not binary:
        print("tui_smoke: FAIL binario / binary / binario / 二进制 ausente / missing", file=sys.stderr)
        return 2

    master, slave = open_pty(rows, cols)
    env = dict(os.environ)
    if home:
        env["HOME"] = home
    # PT: o "binário" pode vir com argumentos: "./build/kiutube2 play a.wav".
    # EN: the "binary" may carry arguments: "./build/kiutube2 play a.wav".
    # ES: el "binario" puede traer argumentos: "./build/kiutube2 play a.wav".
    # ZH: “二进制”可携带参数："./build/kiutube2 play a.wav"。
    import shlex
    argv_bin = shlex.split(binary)
    proc = subprocess.Popen(argv_bin, stdin=slave, stdout=slave, stderr=slave, close_fds=True, env=env)
    os.close(slave)
    # PT: settle: cobre a janela entre o boot e o raw mode/entrada de teclas;
    #     teclas enviadas antes disso podem ser engolidas pelo modo canônico.
    # EN: settle: covers the window between boot and raw mode/key input;
    #     keys sent earlier can be swallowed by canonical mode.
    # ES: settle: cubre la ventana entre el boot y el raw mode; teclas
    #     enviadas antes pueden ser tragadas por el modo canónico.
    # ZH: settle：覆盖从启动到 raw mode/键盘输入之间的窗口期；过早发送的
    #     按键可能被规范模式吞掉。
    time.sleep(0.5)

    out = []
    ok = True
    for kind, value in steps:
        deadline = time.time() + timeout
        if kind == "expect":
            found = False
            while time.time() < deadline:
                if not pump(master, out, 0.2):
                    break
                if value in "".join(out):
                    found = True
                    break
            if not found:
                ok = False
                print(f"tui_smoke: FAIL esperando / waiting / esperando / 等待: {value!r}", file=sys.stderr)
                break
        elif kind == "send":
            os.write(master, value)  # PT: value já chega como bytes decodificados. EN: already bytes. ZH: 已是解码后的字节
            time.sleep(0.3)
        else:  # sh
            # PT: Comando externo (ex.: pactl) com o app ainda rodando. / EN: External command (e.g. pactl) with the app still running.
            # ES: Comando externo (p. ej. pactl) con la app aún corriendo. / ZH: 应用仍在运行时执行外部命令（如 pactl）。
            time.sleep(0.5)  # PT: deixa o último estado assentar antes de checar. EN: let the last state settle. ES: deja asentar el último estado. ZH: 让上一状态稳定下来。
            if subprocess.run(value, shell=True, capture_output=True).returncode != 0:
                print(f"tui_smoke: FAIL comando / command / comando / 命令: {value}", file=sys.stderr)
                ok = False
                break

    if ok and expect_exit is not None:
        try:
            proc.wait(timeout=expect_exit)
        except subprocess.TimeoutExpired:
            print("tui_smoke: FAIL processo nao saiu / did not exit / no salió / 未退出", file=sys.stderr)
            ok = False

    if proc.poll() is None:
        proc.terminate()
        try:
            proc.wait(timeout=2)
        except subprocess.TimeoutExpired:
            proc.kill()

    if ok:
        print("tui_smoke: PASS")
        return 0
    print("tui_smoke: FAIL", file=sys.stderr)
    print("---- ultimo output / last output / último output / 最后输出 ----", file=sys.stderr)
    sys.stderr.write("".join(out)[-2000:])
    return 1


if __name__ == "__main__":
    sys.exit(main())
