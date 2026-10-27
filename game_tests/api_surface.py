"""Contracts for the test driver itself, using the actual C++ host."""
from . import snake_test as st
import json
import os
import subprocess
import contextlib
import io
from collections import deque
from pathlib import Path
import queue
import threading
import tempfile
import types
from unittest import mock


def test_headless_discovery_supports_a_core_only_build():
    with tempfile.TemporaryDirectory() as directory:
        root = Path(directory)
        executable = root / "build-tests" / "bin" / ("snake_game_test.exe" if os.name == "nt" else "snake_game_test")
        executable.parent.mkdir(parents=True)
        executable.touch()
        with mock.patch.object(st, "ROOT", root), mock.patch.dict(os.environ, {}, clear=True):
            assert st.find_executable() == executable


def test_catalog_and_weapon_specs_are_native():
    catalog = st.catalog()
    weapons = [entry for entry in catalog["upgrades"] if entry["kind"] == "weapon"]
    assert len(weapons) == 9, catalog
    assert len(catalog["evolutions"]) == 9, catalog
    for weapon in weapons:
        assert len(weapon["ranks"]) == weapon["max_rank"], weapon
        assert weapon["ranks"][0]["damage"] > 0, weapon
        assert weapon["evolved_spec"]["evolved"], weapon


def test_reset_clears_actors_counters_and_test_settings():
    st.equip("snake_nest")
    st.spawn_enemy(864, 800, hp=100000)
    st.step(100)
    before = st.snapshot()
    assert before["counts"]["mini_snakes"] > 0, before
    st.configure(director=True, freeze_player=False, invulnerable=False, auto_choose=False)
    st.reset(seed=55)
    state = st.snapshot()
    assert state["tick"] == 0 and state["seed"] == 55, state
    assert not state["owned"] and all(count == 0 for count in state["counts"].values()), state
    settings = st.configure()
    assert settings == {"director": False, "grey_snakes": False, "freeze_player": True,
                        "invulnerable": True, "auto_choose": True}, settings


def test_seeded_control_is_reproducible():
    def run():
        st.reset(seed=8801)
        st.equip("viper_fangs", rank=3)
        st.spawn_enemies([{"x": 864 + (i % 3) * 8, "y": 784 + (i // 3) * 8,
                           "hp": 100000, "speed": 16} for i in range(6)])
        st.step(120)
        state = st.snapshot()
        state.pop("timing")
        return state
    assert run() == run()


def test_pause_freezes_simulation_and_resumes_without_a_jump():
    st.configure(freeze_player=False)
    st.step(10, st.NORTH)
    before = st.snapshot()
    st.pause()
    st.step(20, st.WEST)
    paused = st.snapshot()
    assert paused["phase"] == "paused", paused
    assert paused["player"] == before["player"], (before, paused)
    assert paused["elapsed"] == before["elapsed"], (before, paused)
    st.pause()
    st.step(10, st.WEST)
    assert st.player()["position"] != before["player"]["position"]


def test_invalid_commands_return_errors_and_leave_the_session_usable():
    for command, arguments in [("not_a_command", {}), ("equip", {"id": "missing"}),
                               ("step", {"ticks": -1}),
                               ("step", {"ticks": 1, "direction": [1, 1]}),
                               ("spawn_enemy", {"kind": "not_an_enemy"})]:
        try:
            st.command(command, **arguments)
        except st.GameTestError:
            pass
        else:
            raise AssertionError((command, arguments))
    st.step(1)
    assert st.tick_count() == 1


def test_disabling_invulnerability_exposes_real_contact_damage():
    st.configure(invulnerable=False)
    st.spawn_enemy(800, 800, hp=100000)
    st.step(1)
    assert st.player()["hp"] < 100, st.snapshot()


def test_a_bad_spawn_batch_rolls_back_every_partial_target():
    existing = st.spawn_enemy(864, 800)
    before = st.snapshot()
    try:
        st.spawn_enemies([{"x": 880, "y": 800}, {"x": 896, "y": 800, "id": existing}])
    except st.GameTestError:
        pass
    else:
        raise AssertionError("A duplicate handle in a batch was accepted")
    after = st.snapshot()
    assert before["enemies"] == after["enemies"], (before, after)
    assert st.spawn_enemy(880, 800) == existing + 1, "Failed batch consumed fixture handles"


def test_malformed_json_reports_an_error_and_the_host_recovers():
    game = st.current()
    command = [str(game.executable)] + (["--test-api"] if game.native else [])
    command += ["--data", str(st.ROOT / "data")]
    options = {"creationflags": subprocess.CREATE_NO_WINDOW} if os.name == "nt" else {}
    process = subprocess.run(command, input='{\n{"cmd":"hello","request_id":7}\n'
                             '{"cmd":"quit","request_id":8}\n', cwd=st.ROOT,
                             capture_output=True, text=True, encoding="utf-8", timeout=30, **options)
    assert process.returncode == 0, (process.returncode, process.stderr)
    responses = [json.loads(line) for line in process.stdout.splitlines()]
    assert len(responses) == 3, responses
    assert not responses[0]["ok"] and responses[0]["error"]["type"] == "parse_error", responses
    assert responses[1]["ok"] and responses[1]["request_id"] == 7, responses
    assert responses[2]["ok"] and responses[2]["request_id"] == 8, responses


def test_passive_actor_clear_keeps_the_seeded_orbit_distribution():
    def verify():
        st.equip("venom_shot", rank=8, evolved=True)
        st.step(1)
        state = st.snapshot()
        positions = {tuple(round(coordinate, 3) for coordinate in point) for point in state["orbit_fangs"]}
        assert state["counts"]["orbit_fangs"] == 44 and len(positions) > 35, state
    verify()
    st.clear()
    assert st.snapshot()["counts"]["orbit_fangs"] == 0
    verify()


def test_custom_fixture_handles_do_not_block_automatic_ids_or_the_director():
    first = st.spawn_enemy(864, 800, id=1000000000)
    assert first == 1000000000
    assert st.spawn_enemy(880, 800) == 1000000001
    st.spawn_enemy(896, 800, id=1000000003)
    assert st.spawn_enemy(912, 800) == 1000000002
    assert st.spawn_enemy(928, 800) == 1000000004
    before = st.snapshot()["enemies"]
    for invalid in (1, 999999999, 1000000000.5):
        try:
            st.spawn_enemy(944, 800, id=invalid)
        except st.GameTestError:
            pass
        else:
            raise AssertionError(f"Invalid fixture handle was accepted: {invalid}")
    assert st.snapshot()["enemies"] == before
    assert st.spawn_enemy(944, 800) == 1000000005
    st.configure(director=True, spawn_clock=1)
    st.step(1)
    state = st.snapshot()
    ids = [enemy["id"] for enemy in state["enemies"]]
    assert len(ids) == len(set(ids)) and any(handle < 1000000000 for handle in ids), state


def test_timeout_closes_transport_before_a_late_reply_can_poison_next_command():
    class Pipe:
        def __init__(self):
            self.data, self.closed = "", False

        def write(self, value):
            self.data += value

        def flush(self):
            pass

        def close(self):
            self.closed = True

    class Process:
        def __init__(self):
            self.stdin, self.stdout, self.stderr = Pipe(), Pipe(), Pipe()
            self.returncode, self.killed = None, False

        def poll(self):
            return self.returncode

        def kill(self):
            self.killed, self.returncode = True, -9

        def wait(self, timeout=None):
            return self.returncode

    class LateReply:
        calls = 0

        def get(self, timeout=None):
            self.calls += 1
            if self.calls == 1:
                raise queue.Empty
            return '{"ok":true,"request_id":1,"result":{"late":true}}'

    game = object.__new__(st.Game)
    game.timeout, game._sequence, game._closed = 0, 0, False
    game._queue, game._diagnostics, game._lock = LateReply(), deque(), threading.RLock()
    game.process = Process()
    game._stdout, game._stderr = mock.Mock(), mock.Mock()
    try:
        game.command("step", ticks=600)
    except TimeoutError:
        pass
    else:
        raise AssertionError("Scripted transport timeout did not raise")
    assert game.closed and game.process.killed
    assert all(pipe.closed for pipe in (game.process.stdin, game.process.stdout, game.process.stderr))
    try:
        game.command("snapshot")
    except st.GameTestError as error:
        assert "closed" in str(error), error
    else:
        raise AssertionError("A timed-out session accepted another command")
    assert game._queue.calls == 1, "The stale reply was consumed by a later command"
    assert game.process.stdin.data.count("\n") == 1, "Forced close sent another protocol command"


def test_runner_recreates_a_dead_host_before_the_next_independent_test():
    from . import run as runner
    original = st.current()
    executable = original.executable
    created = []

    class Host:
        def __init__(self, executable=None, *, native=False, data=None):
            self.executable = Path(executable).resolve()
            self.native, self.data, self.closed = native, Path(data).resolve(), False
            self.serial = len(created) + 1
            self.tick_budget, self.ticks_used = None, 0
            created.append(self)

        def reset(self):
            assert not self.closed

        def invariants(self):
            assert not self.closed
            return {"ok": True}

        def close(self):
            self.closed = True

        def __enter__(self):
            return self

        def __exit__(self, *exception):
            self.close()

    def first():
        st.current().close()
        raise TimeoutError("Scripted host failure")

    def second():
        assert st.current().serial == 2 and not st.current().closed

    module = types.ModuleType("game_tests.api_surface")
    module.TIER = "logic"
    for name, function in (("test_a_timeout", first), ("test_b_after_timeout", second)):
        function.__name__, function.__module__ = name, module.__name__
        setattr(module, name, function)
    report = st.ROOT / "artifacts" / "gametest" / "driver-restart-result.json"
    arguments = ["--single", "api_surface::test_a_timeout", "--single", "api_surface::test_b_after_timeout",
                 "--exe", str(executable), "--data", str(st.ROOT / "data"), "--json", str(report)]
    if original.native:
        arguments.append("--native")
    with mock.patch.object(runner.importlib, "import_module", return_value=module), \
            mock.patch.object(st, "Game", Host), contextlib.redirect_stdout(io.StringIO()):
        result = runner.main(arguments)
    document = json.loads(report.read_text(encoding="utf-8"))
    assert result == 1 and document["failed"] == document["passed"] == 1, document
    assert [entry["status"] for entry in document["results"]] == ["FAIL", "PASS"], document
    assert len(created) == 2
    assert all(host.executable == executable and host.native == original.native and
               host.data == (st.ROOT / "data").resolve() for host in created)


def test_bad_capture_arguments_leave_native_rendering_usable():
    if not st.current().native:
        st.skip("Requires the native capture host")
    from ._png import read
    path = st.ROOT / "artifacts" / "gametest" / "api-capture-recovery.png"
    try:
        st.capture(path, world_only="not a boolean")
    except st.GameTestError:
        pass
    else:
        raise AssertionError("Capture accepted a malformed world_only flag")
    assert not st.current().closed
    capture = st.capture(path, world_only=True)
    image = read(capture["path"])
    assert (image.width, image.height) == (capture["width"], capture["height"]) == (800, 500)
