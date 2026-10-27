"""Discover and run Warforge-style module::test selectors against native Game."""
from __future__ import annotations

import argparse
import contextlib
import importlib
import inspect
import json
from pathlib import Path
import random
import sys
import time
import traceback
from datetime import datetime, timezone

from . import snake_test as st


def _tier(module, name):
    if hasattr(module, "TIER"):
        return module.TIER
    if name.endswith(("_visual", "_showcase")):
        return "visual"
    if name.endswith(("_benchmark", "_bench", "_performance")):
        return "bench"
    if name.endswith("_hunt"):
        return "hunt"
    return "logic"


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("selectors", nargs="*", help="module or module::test_name (commas accepted)")
    parser.add_argument("--single", action="append", default=[], help="Warforge-compatible selector argument")
    parser.add_argument("--tier", default="logic")
    parser.add_argument("--all", action="store_true")
    parser.add_argument("--list", action="store_true")
    parser.add_argument("--repeat", type=int, default=1)
    parser.add_argument("--native", action="store_true", help="Use the real hidden raylib game host")
    parser.add_argument("--exe", type=Path)
    parser.add_argument("--data", type=Path)
    parser.add_argument("--json", type=Path, help="Write structured results to this file")
    args = parser.parse_args(argv)
    if args.repeat < 1:
        parser.error("--repeat must be positive")
    root = Path(__file__).resolve().parents[1]
    sys.path.insert(0, str(root / "game_tests"))
    # Both import styles refer to the same API/active fixture.
    sys.modules["snake_test"] = st
    selectors = [part for item in args.selectors + args.single for part in item.split(",") if part]
    requested_modules = {selector.split("::")[0] for selector in selectors}
    available = {}
    module_errors = []
    for path in sorted((root / "game_tests").glob("*.py")):
        if path.stem.startswith("_") or path.stem in ("run", "snake_test"):
            continue
        if requested_modules and path.stem not in requested_modules:
            continue
        try:
            module = importlib.import_module("game_tests." + path.stem)
        except Exception:
            module_errors.append({"test": path.stem, "status": "FAIL", "error": traceback.format_exc()})
            continue
        for name, function in inspect.getmembers(module, inspect.isfunction):
            if name.startswith("test_") and function.__module__ == module.__name__:
                available[path.stem + "::" + name] = (function, _tier(module, path.stem))
    selected = set()
    if selectors:
        for selector in selectors:
            matches = {name for name in available if name == selector or name.split("::")[0] == selector}
            if not matches:
                broken = next((error for error in module_errors if error["test"] == selector.split("::")[0]), None)
                if broken:
                    print(broken["error"], file=sys.stderr)
                    parser.error("Could not load selected module: " + broken["test"])
                parser.error("Unknown or empty selector: " + selector)
            selected.update(matches)
    else:
        tiers = set(args.tier.split(","))
        selected = {name for name, (_, tier) in available.items() if args.all or tier in tiers}
    if args.list:
        for error in module_errors:
            print(error["error"], file=sys.stderr)
        for name in sorted(selected):
            function, tier = available[name]
            print(f"{name} [{tier}] budget={getattr(function, 'timeout_ticks', 600)}")
        return 1 if module_errors else 0
    if not selected:
        parser.error("The selection contains no tests")
    results = list(module_errors)
    run_start = time.perf_counter()
    try:
        with contextlib.ExitStack() as sessions:
            game = sessions.enter_context(st.Game(args.exe, native=args.native, data=args.data))
            executable = game.executable
            for repeat in range(args.repeat):
                for name in sorted(selected):
                    function, tier = available[name]
                    started = time.perf_counter()
                    result = {"test": name, "tier": tier, "repeat": repeat + 1}
                    try:
                        if game.closed:
                            game = sessions.enter_context(st.Game(executable, native=args.native, data=args.data))
                        game.reset()
                        game.tick_budget = getattr(function, "timeout_ticks", 600)
                        game.ticks_used = 0
                        random.seed(0)
                        with st.bind(game):
                            function()
                        game.invariants()
                        result["status"] = "PASS"
                    except st.TestSkipped as error:
                        result.update(status="SKIP", reason=str(error))
                    except Exception:
                        result.update(status="FAIL", error=traceback.format_exc())
                    result.update(ticks=game.ticks_used, seconds=time.perf_counter() - started)
                    results.append(result)
                    print(f"[{result['status']}] {name} ({result['ticks']} ticks, {result['seconds']:.3f}s)")
                    if "error" in result:
                        print(result["error"])
    except Exception:
        results.append({"test": "host", "status": "FAIL", "error": traceback.format_exc()})
        print(results[-1]["error"])
    failed = sum(result["status"] == "FAIL" for result in results)
    passed = sum(result["status"] == "PASS" for result in results)
    document = {"passed": passed, "failed": failed, "seconds": time.perf_counter() - run_start,
                "native": args.native, "results": results}
    output = args.json
    if output is None:
        stamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%S.%fZ")
        output = root / "artifacts" / "runs" / (stamp + "-gametest") / "result.json"
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(document, indent=2), encoding="utf-8")
    print(f"{passed} passed, {failed} failed. Results: {output}")
    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
