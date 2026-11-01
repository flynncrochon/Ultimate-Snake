"""Warforge-style, deterministic Python control of the native game.

The subprocess is the real C++ Game, not a Python reimplementation. Logic
tests need only the headless host. Native capture tests use the game binary.
"""
from __future__ import annotations

import contextlib
import json
import os
from pathlib import Path
import queue
import subprocess
import threading
import time
from collections import deque

ROOT = Path(__file__).resolve().parents[1]
EAST, SOUTH, WEST, NORTH = (1, 0), (0, 1), (-1, 0), (0, -1)
TILE = 8
FIXED_DT = 1.0 / 60.0


class GameTestError(RuntimeError):
    """A structured error from the native test host."""


class TestSkipped(Exception):
    pass


def find_executable(native=False):
    variable = "SNAKE_NATIVE_EXE" if native else "SNAKE_GAME_TEST_EXE"
    configured = os.environ.get(variable)
    if configured:
        path = Path(configured).expanduser().resolve()
        if not path.is_file():
            raise FileNotFoundError(f"{variable} does not name an executable: {path}")
        return path
    suffix = ".exe" if os.name == "nt" else ""
    name = ("snake_survivors" if native else "snake_game_test") + suffix
    candidates = ([ROOT / "build" / "bin" / name, ROOT / "build" / "Release" / name]
                  if native else [ROOT / "build" / "bin" / name, ROOT / "build-tests" / "bin" / name,
                                  ROOT / "build" / name, ROOT / "build-tests" / name,
                                  ROOT / "build" / "Release" / name])
    for path in candidates:
        if path.is_file():
            return path
    raise FileNotFoundError(f"Build {name} first, or set {variable}. Tried: {candidates}")


class Game:
    def __init__(self, executable=None, *, native=False, data=None, timeout=30.0):
        self.executable = Path(executable).resolve() if executable else find_executable(native)
        self.native = native
        self.timeout = timeout
        self.tick_budget = None
        self.ticks_used = 0
        self._sequence = 0
        self._queue = queue.Queue()
        self._diagnostics = deque(maxlen=100)
        self._lock = threading.RLock()
        self._closed = False
        command = [str(self.executable)]
        if native:
            command.append("--test-api")
        command += ["--data", str(Path(data).resolve() if data else ROOT / "data")]
        options = {"creationflags": subprocess.CREATE_NO_WINDOW} if os.name == "nt" else {}
        self.process = subprocess.Popen(command, cwd=ROOT, stdin=subprocess.PIPE,
                                        stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                        text=True, encoding="utf-8", bufsize=1, **options)
        self._stdout = threading.Thread(target=self._read_stdout, daemon=True)
        self._stderr = threading.Thread(target=self._read_stderr, daemon=True)
        self._stdout.start()
        self._stderr.start()
        try:
            self.capabilities = self.command("hello")
            self.reset()
        except BaseException:
            self.close()
            raise

    def _read_stdout(self):
        try:
            for line in self.process.stdout:
                self._queue.put(line)
        finally:
            self._queue.put(None)

    def _read_stderr(self):
        for line in self.process.stderr:
            self._diagnostics.append(line.rstrip())

    def command(self, cmd, **arguments):
        with self._lock:
            if self._closed:
                raise GameTestError("The native test session is closed")
            self._sequence += 1
            request = {"cmd": cmd, "request_id": self._sequence, **arguments}
            try:
                encoded = json.dumps(request, allow_nan=False, separators=(",", ":"))
                self.process.stdin.write(encoded + "\n")
                self.process.stdin.flush()
            except (BrokenPipeError, OSError) as error:
                self.close(force=True)
                raise GameTestError(f"Host input failed: {error}; diagnostics={list(self._diagnostics)}") from error
            try:
                line = self._queue.get(timeout=self.timeout)
            except queue.Empty as error:
                self.close(force=True)
                raise TimeoutError(f"Native command {cmd!r} timed out after {self.timeout}s; "
                                   f"diagnostics={list(self._diagnostics)}") from error
            if line is None:
                self.close(force=True)
                raise GameTestError(f"Host exited during {cmd!r} (exit={self.process.poll()}): {list(self._diagnostics)}")
            try:
                response = json.loads(line)
            except json.JSONDecodeError as error:
                self.close(force=True)
                raise GameTestError(f"Host wrote non-JSON output: {line[:500]}") from error
            if response.get("request_id") != self._sequence:
                self.close(force=True)
                raise GameTestError(f"Mismatched native response: {response}")
            if not response.get("ok"):
                raise GameTestError(f"{cmd}: {response.get('error', response)}")
            return response["result"]

    def reset(self, seed=0x5EED, character=0, *, isolated=True, permanent=None):
        inherited={} if permanent is None else {"permanent": permanent}
        return self.command("reset", seed=seed, character=character, isolated=isolated, **inherited)

    def clear(self, *, owned=True):
        return self.command("clear", owned=owned)

    def configure(self, **settings):
        return self.command("configure", **settings)

    def player(self, **fields):
        return self.command("player", **fields) if fields else self.snapshot(full=False)["player"]

    def equip(self, upgrade, rank=1, evolved=False, *, cooldown=0):
        return self.command("equip", id=upgrade, rank=rank, evolved=evolved, cooldown=cooldown)

    def unequip(self, upgrade):
        return self.command("unequip", id=upgrade)

    def spawn_enemy(self, x=None, y=None, *, hp=1000, speed=0, kind="red", **fields):
        if x is not None:
            fields["x"] = x
        if y is not None:
            fields["y"] = y
        return self.command("spawn_enemy", hp=hp, speed=speed, kind=kind, **fields)

    def spawn_flood(self, x, y, *, direction=(-1, 0), count=640, warning=1.5):
        return self.command("spawn_flood", x=x, y=y, direction=list(direction), count=count, warning=warning)

    def spawn_enemies(self, enemies):
        return self.command("spawn_enemies", enemies=list(enemies))

    def spawn_grey_snake(self, path, **fields):
        return self.command("spawn_grey_snake", path=list(path), **fields)

    def spawn_pickup(self, x=None, y=None, *, kind="experience", value=1, **fields):
        if x is not None:
            fields["x"] = x
        if y is not None:
            fields["y"] = y
        return self.command("spawn_pickup", kind=kind, value=value, **fields)

    def set_enemy(self, handle, **fields):
        return self.command("set_enemy", id=handle, **fields)

    def enemy(self, handle):
        return next((enemy for enemy in self.snapshot()["enemies"] if enemy["id"] == handle), None)

    def step(self, ticks=1, direction=(0, 0), *, full=False, validate=False):
        if isinstance(ticks, bool) or not isinstance(ticks, int) or ticks < 0:
            raise ValueError("ticks must be a nonnegative integer")
        if self.tick_budget is not None and self.ticks_used + ticks > self.tick_budget:
            raise AssertionError({"tick_budget": self.tick_budget, "used": self.ticks_used, "requested": ticks})
        result = self.command("step", ticks=ticks, direction=list(direction),
                              full=full, validate=validate)
        self.ticks_used += ticks
        return result

    def snapshot(self, *, full=True):
        return self.command("snapshot", full=full)

    def catalog(self):
        return self.command("catalog")

    def invariants(self):
        result = self.command("invariants")
        assert result["ok"], result
        return result

    def capture(self, path, **options):
        return self.command("capture", path=str(Path(path).resolve()), **options)

    def benchmark(self, ticks=600, *, warmup=120, direction=(0, 0), validate=True):
        if warmup:
            self.step(warmup, direction)
        result = self.step(ticks, direction, validate=validate)
        return result["timing"]

    def pause(self):
        return self.command("pause")

    def choose(self, choice=0):
        return self.command("choose", choice=choice)

    def reroll(self):
        return self.command("reroll")

    def banish(self, choice=0):
        return self.command("banish", choice=choice)

    def grant_xp(self, amount):
        return self.command("grant_xp", amount=amount)

    def wait_until(self, predicate, ticks=600, *, chunk=1, message="condition", state=None):
        if ticks < 0 or chunk <= 0:
            raise ValueError("Invalid wait budget")
        waited = 0
        while True:
            result = predicate()
            if result:
                return result
            if waited == ticks:
                detail = state() if callable(state) else state
                raise AssertionError({"wait": message, "ticks": ticks, "state": detail})
            batch = min(chunk, ticks - waited)
            self.step(batch)
            waited += batch

    @property
    def closed(self):
        return self._closed

    def close(self, *, force=False):
        if getattr(self, "_closed", True):
            return
        if force:
            self._closed = True
        try:
            if self.process.poll() is None:
                if force:
                    self.process.kill()
                else:
                    self.command("quit")
                self.process.wait(timeout=5)
        except (Exception, KeyboardInterrupt):
            if self.process.poll() is None:
                self.process.kill()
            self.process.wait(timeout=5)
        finally:
            self._closed = True
            self._stdout.join(timeout=1)
            self._stderr.join(timeout=1)
            for pipe in (self.process.stdin, self.process.stdout, self.process.stderr):
                if pipe:
                    pipe.close()

    def __enter__(self):
        return self

    def __exit__(self, *exception):
        self.close()


_active_game = None


@contextlib.contextmanager
def bind(game):
    """The runner binds a fresh test world to module-level st.step(), etc."""
    global _active_game
    previous = _active_game
    _active_game = game
    try:
        yield game
    finally:
        _active_game = previous


def current():
    if _active_game is None:
        raise GameTestError("Use the game-test runner, `with st.Game() as game:`, or `with st.bind(game):`")
    return _active_game


def skip(reason):
    raise TestSkipped(str(reason))


def _forward(name):
    def call(*args, **kwargs):
        return getattr(current(), name)(*args, **kwargs)
    call.__name__ = name
    return call


for _name in ("reset", "clear", "configure", "player", "equip", "unequip", "spawn_enemy", "spawn_enemies", "spawn_grey_snake", "spawn_flood", "spawn_pickup",
              "set_enemy", "enemy", "step", "snapshot", "catalog", "invariants", "capture", "benchmark",
              "pause", "choose", "reroll", "banish", "grant_xp", "wait_until", "command"):
    globals()[_name] = _forward(_name)


def tick_count():
    return current().snapshot(full=False)["tick"]
