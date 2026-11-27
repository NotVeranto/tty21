#!/usr/bin/env python3
"""Exercise interactive terminal behavior through a real pseudo-terminal."""

import fcntl
import os
import pty
import select
import signal
import struct
import subprocess
import termios
import time
import re


def run_session(flags):
    master, slave = pty.openpty()
    fcntl.ioctl(slave, termios.TIOCSWINSZ, struct.pack("HHHH", 50, 120, 0, 0))
    original = termios.tcgetattr(slave)
    env = os.environ.copy()
    env.update({
        "TERM": "xterm-256color",
        "LANG": "C.UTF-8",
        "LC_ALL": "C.UTF-8",
        "TTY21_NO_ANIM": "1",
        "HOME": "/tmp",
        "XDG_DATA_HOME": "/tmp/tty21-terminal-smoke",
    })
    process = subprocess.Popen(
        ["./tty21", *flags],
        stdin=slave,
        stdout=slave,
        stderr=slave,
        close_fds=True,
        env=env,
    )
    output = bytearray()
    deadline = time.monotonic() + 8
    sent_quit = False

    try:
        while time.monotonic() < deadline:
            readable, _, _ = select.select([master], [], [], 0.05)
            if readable:
                try:
                    chunk = os.read(master, 65536)
                except OSError:
                    chunk = b""
                if chunk:
                    output.extend(chunk)

            if not sent_quit and time.monotonic() > deadline - 7.5:
                os.write(master, b"q")
                sent_quit = True

            if process.poll() is not None:
                while True:
                    readable, _, _ = select.select([master], [], [], 0)
                    if not readable:
                        break
                    try:
                        chunk = os.read(master, 65536)
                    except OSError:
                        break
                    if not chunk:
                        break
                    output.extend(chunk)
                break
        else:
            process.send_signal(signal.SIGKILL)
            raise AssertionError("tty21 did not exit after the startup-menu quit key")

        if process.returncode != 0:
            raise AssertionError(f"tty21 exited with status {process.returncode}")

        restored = termios.tcgetattr(slave)
        return bytes(output), original, restored
    finally:
        if process.poll() is None:
            process.kill()
            process.wait()
        os.close(master)
        os.close(slave)


def main():
    output, original, restored = run_session(["--ascii", "--no-color"])

    if original != restored:
        raise AssertionError("terminal settings were not restored after quitting")
    if any(byte >= 0x80 for byte in output):
        raise AssertionError("ASCII mode emitted non-ASCII bytes")
    if b"\x1b[?25l" not in output:
        raise AssertionError("interactive screen never hid the terminal cursor")
    if b"\x1b[?25h" not in output:
        raise AssertionError("terminal cursor was not restored on exit")
    if re.search(rb"\x1b\[[0-9;]*m", output):
        raise AssertionError("monochrome mode emitted an SGR style sequence")

    print("Pseudo-terminal smoke tests passed")


if __name__ == "__main__":
    main()
