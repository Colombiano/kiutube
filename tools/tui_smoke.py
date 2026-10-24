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
    # PT: passos sequenciais: --expect espera substring; --send escreve bytes.
    # EN: sequential steps: --expect waits for a substring; --send writes bytes.
    # ES: pasos secuenciales: --expect espera substring; --send escribe bytes.
    # ZH: 顺序步骤：--expect 等待子串；--send 写入字节。
    steps = []
    ap = argparse.ArgumentParser()
    ap.add_argument("binary")
    ap.add_argument("--expect", action="append")
    ap.add_argument("--send", action="append")
    ap.add_argument("--expect-exit", type=float, default=None)
    ap.add_argument("--timeout", type=float, default=15.0)
    ap.add_argument("--rows", type=int, default=24)
    ap.add_argument("--cols", type=int, default=100)
    args = ap.parse_args()

    expects = args.expect or []
    sends = args.send or []
    n = max(len(expects), len(sends))
    for i in range(n):
        steps.append(("expect", expects[i]) if i < len(expects) else None)
        steps.append(("send", sends[i]) if i < len(sends) else None)
    steps = [s for s in steps if s]

    master, slave = open_pty(args.rows, args.cols)
    proc = subprocess.Popen([args.binary], stdin=slave, stdout=slave, stderr=slave, close_fds=True)
    os.close(slave)

    out = []
    ok = True
    for kind, value in steps:
        deadline = time.time() + args.timeout
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
        else:  # send
            os.write(master, value.encode("utf-8").decode("unicode_escape").encode("latin-1"))
            time.sleep(0.3)

    if ok and args.expect_exit is not None:
        try:
            proc.wait(timeout=args.expect_exit)
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
