#!/usr/bin/env python3

from __future__ import annotations

import json
import re
import subprocess
import time
from dataclasses import asdict, dataclass, replace
from pathlib import Path
from typing import Iterable


ROOT = Path(__file__).resolve().parent.parent
BUILD_DIR = ROOT / "build"
PDDL_DIR = ROOT / ".tmp" / "pddl-benchmarks" / "crafting"
FD_DIR = ROOT / ".tmp" / "planners" / "fast-downward"
SYMK_DIR = ROOT / ".tmp" / "planners" / "symk"
LOCAL_BINARY = BUILD_DIR / "tensor_planner_crafting_pddl_experiment"
COUNTS = (0, 15, 30, 256, 1024, 2560)

PLANNER_SPECS = {
    "tensor_planner_pddl": {
        "display_name": "Local planner",
        "family": "local",
        "sort_order": 0,
    },
    "fast_downward_astar_lmcut": {
        "display_name": "Fast Downward",
        "family": "fast_downward",
        "sort_order": 1,
    },
    "symk_sym_bd": {
        "display_name": "SymK",
        "family": "symk",
        "sort_order": 2,
    },
}


DOMAIN_TEMPLATE = """(define (domain crafting-distractions)
  (:requirements :strips :typing)
  (:types agent location item)

  (:predicates
    (at ?a - agent ?l - location)
    (has ?i - item)
    (missing ?i - item)
    (connected ?from - location ?to - location)
    (gather-at ?i - item ?l - location)
    (home ?l - location)
    (requires-tool ?i - item ?tool - item)
    (gather-bare-hands ?i - item)
    (recipe2-a ?item - item ?first - item)
    (recipe2-b ?item - item ?second - item)
    (recipe3-a ?item - item ?first - item)
    (recipe3-b ?item - item ?second - item)
    (recipe3-c ?item - item ?third - item)
  )

  (:action move
    :parameters (?agent - agent ?from - location ?to - location)
    :precondition (and
      (at ?agent ?from)
      (connected ?from ?to)
    )
    :effect (and
      (not (at ?agent ?from))
      (at ?agent ?to)
    )
  )

  (:action gather-free
    :parameters (?agent - agent ?item - item ?where - location)
    :precondition (and
      (at ?agent ?where)
      (gather-at ?item ?where)
      (gather-bare-hands ?item)
      (missing ?item)
    )
    :effect (and
      (not (missing ?item))
      (has ?item)
    )
  )

  (:action gather-tool
    :parameters (?agent - agent ?item - item ?where - location ?tool - item)
    :precondition (and
      (at ?agent ?where)
      (gather-at ?item ?where)
      (requires-tool ?item ?tool)
      (has ?tool)
      (missing ?item)
    )
    :effect (and
      (not (missing ?item))
      (has ?item)
    )
  )

  (:action craft2
    :parameters (?agent - agent ?item - item ?where - location ?first - item ?second - item)
    :precondition (and
      (at ?agent ?where)
      (home ?where)
      (recipe2-a ?item ?first)
      (recipe2-b ?item ?second)
      (has ?first)
      (has ?second)
      (missing ?item)
    )
    :effect (and
      (not (missing ?item))
      (has ?item)
    )
  )

  (:action craft3
    :parameters (?agent - agent ?item - item ?where - location ?first - item ?second - item ?third - item)
    :precondition (and
      (at ?agent ?where)
      (home ?where)
      (recipe3-a ?item ?first)
      (recipe3-b ?item ?second)
      (recipe3-c ?item ?third)
      (has ?first)
      (has ?second)
      (has ?third)
      (missing ?item)
    )
    :effect (and
      (not (missing ?item))
      (has ?item)
    )
  )
)
"""


@dataclass(frozen=True)
class PlannerResult:
    planner: str
    planner_display_name: str
    planner_family: str
    planner_sort_order: int
    distractions: int
    count_index: int
    solved: bool
    exit_code: int
    plan_length: int | None
    wall_time_s: float
    search_time_s: float | None
    total_time_s: float | None
    expanded: int | None
    generated: int | None
    wall_time_ratio_vs_local: float | None
    plan_length_delta_vs_local: int | None
    stdout_path: str
    plan_path: str | None


def load_existing_results(results_path: Path) -> list[PlannerResult] | None:
    if not results_path.exists():
        return None

    try:
        payload = json.loads(results_path.read_text())
    except json.JSONDecodeError:
        return None

    if not isinstance(payload, list):
        return None

    results: list[PlannerResult] = []
    for entry in payload:
        if not isinstance(entry, dict):
            return None
        try:
            results.append(PlannerResult(**entry))
        except TypeError:
            return None
    return results


def run_command(
    command: list[str], workdir: Path, stdout_path: Path
) -> tuple[int, str, float]:
    start = time.perf_counter()
    completed = subprocess.run(
        command,
        cwd=workdir,
        capture_output=True,
        text=True,
        check=False,
    )
    elapsed = time.perf_counter() - start
    stdout_path.write_text(completed.stdout + "\n--- STDERR ---\n" + completed.stderr)
    return completed.returncode, completed.stdout + completed.stderr, elapsed


def planner_spec(planner_name: str) -> dict[str, str | int]:
    try:
        return PLANNER_SPECS[planner_name]
    except KeyError as error:
        raise ValueError(f"Unknown planner name: {planner_name}") from error


def count_index(distractions: int) -> int:
    try:
        return COUNTS.index(distractions)
    except ValueError as error:
        raise ValueError(f"Unsupported distraction count: {distractions}") from error


def parse_int(pattern: str, text: str) -> int | None:
    match = re.search(pattern, text, re.MULTILINE)
    return int(match.group(1)) if match else None


def parse_float(pattern: str, text: str) -> float | None:
    match = re.search(pattern, text, re.MULTILINE)
    return float(match.group(1)) if match else None


def count_plan_steps(plan_path: Path) -> int | None:
    if not plan_path.exists():
        return None
    lines = [
        line
        for line in plan_path.read_text().splitlines()
        if line.strip() and not line.startswith(";")
    ]
    return len(lines)


def build_result(
    *,
    planner_name: str,
    distractions: int,
    solved: bool,
    exit_code: int,
    plan_length: int | None,
    wall_time_s: float,
    search_time_s: float | None,
    total_time_s: float | None,
    expanded: int | None,
    generated: int | None,
    stdout_path: str,
    plan_path: str | None,
) -> PlannerResult:
    spec = planner_spec(planner_name)
    return PlannerResult(
        planner=planner_name,
        planner_display_name=str(spec["display_name"]),
        planner_family=str(spec["family"]),
        planner_sort_order=int(spec["sort_order"]),
        distractions=distractions,
        count_index=count_index(distractions),
        solved=solved,
        exit_code=exit_code,
        plan_length=plan_length,
        wall_time_s=wall_time_s,
        search_time_s=search_time_s,
        total_time_s=total_time_s,
        expanded=expanded,
        generated=generated,
        wall_time_ratio_vs_local=None,
        plan_length_delta_vs_local=None,
        stdout_path=stdout_path,
        plan_path=plan_path,
    )


def augment_results(results: list[PlannerResult]) -> list[PlannerResult]:
    local_by_count = {
        result.distractions: result
        for result in results
        if result.planner == "tensor_planner_pddl"
    }
    augmented: list[PlannerResult] = []
    for result in results:
        local_result = local_by_count.get(result.distractions)
        ratio = None
        plan_delta = None
        if local_result is not None and local_result.wall_time_s > 0.0:
            ratio = result.wall_time_s / local_result.wall_time_s
        if local_result is not None and result.plan_length is not None:
            if local_result.plan_length is not None:
                plan_delta = result.plan_length - local_result.plan_length
        augmented.append(
            replace(
                result,
                wall_time_ratio_vs_local=ratio,
                plan_length_delta_vs_local=plan_delta,
            )
        )
    return augmented


def make_problem_text(distractions: int) -> str:
    distraction_items = [f"distraction_recipe_{index}" for index in range(distractions)]
    item_names = [
        "flint",
        "fiber",
        "wood",
        "stone",
        "iron_ore",
        "diamond",
        *distraction_items,
        "flint_axe",
        "stick",
        "stone_pickaxe",
        "iron_pickaxe",
        "diamond_pickaxe",
    ]

    missing_facts = "\n    ".join(f"(missing {name})" for name in item_names)
    distraction_recipes = "\n    ".join(
        f"(recipe2-a {name} flint)\n    (recipe2-b {name} fiber)"
        for name in distraction_items
    )
    distraction_recipe_block = (
        f"\n    {distraction_recipes}" if distraction_recipes else ""
    )

    object_lines = [
        "agent - agent",
        "home forest river cave deep_cave - location",
        f"{' '.join(item_names)} - item",
    ]
    objects = "\n    ".join(object_lines)

    return f"""(define (problem crafting-distractions-{distractions})
  (:domain crafting-distractions)
  (:objects
    {objects}
  )
  (:init
    (at agent home)
    (home home)
    {missing_facts}

    (connected home forest)
    (connected forest home)
    (connected forest river)
    (connected river forest)
    (connected forest cave)
    (connected cave forest)
    (connected cave deep_cave)
    (connected deep_cave cave)

    (gather-at flint river)
    (gather-at fiber forest)
    (gather-at wood forest)
    (gather-at stone cave)
    (gather-at iron_ore cave)
    (gather-at diamond deep_cave)
    (gather-bare-hands flint)
    (gather-bare-hands fiber)
    (requires-tool wood flint_axe)
    (requires-tool stone flint_axe)
    (requires-tool iron_ore stone_pickaxe)
    (requires-tool diamond iron_pickaxe){distraction_recipe_block}
    (recipe2-a flint_axe flint)
    (recipe2-b flint_axe fiber)
    (recipe2-a stick wood)
    (recipe2-b stick fiber)
    (recipe2-a stone_pickaxe stone)
    (recipe2-b stone_pickaxe stick)
    (recipe2-a iron_pickaxe iron_ore)
    (recipe2-b iron_pickaxe stick)
    (recipe3-a diamond_pickaxe diamond)
    (recipe3-b diamond_pickaxe stick)
    (recipe3-c diamond_pickaxe iron_pickaxe)
  )
  (:goal (and (has diamond_pickaxe)))
)
"""


def ensure_pddl_files() -> tuple[Path, dict[int, Path]]:
    PDDL_DIR.mkdir(parents=True, exist_ok=True)
    domain_path = PDDL_DIR / "domain.pddl"
    domain_path.write_text(DOMAIN_TEMPLATE)

    problem_paths: dict[int, Path] = {}
    for distractions in COUNTS:
        problem_path = PDDL_DIR / f"problem-{distractions}.pddl"
        problem_path.write_text(make_problem_text(distractions))
        problem_paths[distractions] = problem_path
    return domain_path, problem_paths


def run_local_planner() -> list[PlannerResult]:
    stdout_path = PDDL_DIR / "tensor-planner.log"
    command = [str(LOCAL_BINARY), *[str(count) for count in COUNTS]]
    exit_code, output, elapsed = run_command(command, ROOT, stdout_path)
    results: list[PlannerResult] = []

    pattern = re.compile(
        r"planner=tensor_planner_pddl distractions=(\d+) solved=(yes|no) status=(\d+) expansions=(\d+) generated=(\d+) scorer_calls=(\d+) plan_length=(\d+) wall_us=(\d+)"
    )
    matches = list(pattern.finditer(output))
    for match in matches:
        wall_us = int(match.group(8))
        results.append(
            build_result(
                planner_name="tensor_planner_pddl",
                distractions=int(match.group(1)),
                solved=match.group(2) == "yes",
                exit_code=exit_code,
                plan_length=int(match.group(7)),
                wall_time_s=wall_us / 1_000_000.0,
                search_time_s=wall_us / 1_000_000.0,
                total_time_s=wall_us / 1_000_000.0,
                expanded=int(match.group(4)),
                generated=int(match.group(5)),
                stdout_path=str(stdout_path.relative_to(ROOT)),
                plan_path=None,
            )
        )

    if exit_code != 0 or len(results) != len(COUNTS):
        raise RuntimeError("tensor planner benchmark failed")
    return results


def run_fd_like(
    planner_name: str,
    planner_dir: Path,
    search: str,
    domain_path: Path,
    problem_path: Path,
) -> PlannerResult:
    safe_name = planner_name.replace("_", "-")
    stdout_path = PDDL_DIR / f"{safe_name}-{problem_path.stem}.log"
    plan_path = PDDL_DIR / f"{safe_name}-{problem_path.stem}.plan"
    command = [
        "python3",
        "fast-downward.py",
        "--plan-file",
        str(plan_path),
        str(domain_path),
        str(problem_path),
        "--search",
        search,
    ]
    exit_code, output, elapsed = run_command(command, planner_dir, stdout_path)
    solved = "Solution found." in output or plan_path.exists()
    return build_result(
        planner_name=planner_name,
        distractions=int(problem_path.stem.split("-")[-1]),
        solved=solved,
        exit_code=exit_code,
        plan_length=count_plan_steps(plan_path),
        wall_time_s=elapsed,
        search_time_s=parse_float(r"Search time: ([0-9.]+)s", output),
        total_time_s=parse_float(r"Total time: ([0-9.]+)s", output),
        expanded=parse_int(r"Expanded (\d+) state\(s\)\.", output),
        generated=parse_int(r"Generated (\d+) state\(s\)\.", output),
        stdout_path=str(stdout_path.relative_to(ROOT)),
        plan_path=str(plan_path.relative_to(ROOT)) if plan_path.exists() else None,
    )


def format_value(value: object, precision: int = 6) -> str:
    if value is None:
        return "-"
    if isinstance(value, bool):
        return "yes" if value else "no"
    if isinstance(value, float):
        return f"{value:.{precision}f}"
    return str(value)


def format_signed(value: int | None) -> str:
    if value is None:
        return "-"
    return f"{value:+d}"


def format_percent_delta(before: float | None, after: float | None) -> str:
    if before is None or after is None or before <= 0.0:
        return "-"
    delta = ((after - before) / before) * 100.0
    return f"{delta:+.1f}%"


def result_index(
    results: Iterable[PlannerResult],
) -> dict[tuple[str, int], PlannerResult]:
    return {(result.planner, result.distractions): result for result in results}


def render_markdown_table(headers: list[str], rows: list[list[str]]) -> str:
    table_rows = [headers, *rows]
    widths = [
        max(len(row[index]) for row in table_rows) for index in range(len(headers))
    ]
    formatted_rows = []
    for index, row in enumerate(table_rows):
        formatted_rows.append(
            " | ".join(value.ljust(widths[column]) for column, value in enumerate(row))
        )
        if index == 0:
            formatted_rows.append("-|-".join("-" * width for width in widths))
    return "\n".join(formatted_rows)


def render_table(results: Iterable[PlannerResult]) -> str:
    headers = [
        "planner",
        "planner_family",
        "distractions",
        "count_index",
        "solved",
        "plan_length",
        "wall_time_s",
        "search_time_s",
        "wall_vs_local",
        "plan_vs_local",
        "expanded",
        "generated",
    ]
    rows: list[list[str]] = []
    for result in results:
        rows.append(
            [
                result.planner_display_name,
                result.planner_family,
                str(result.distractions),
                str(result.count_index),
                format_value(result.solved),
                format_value(result.plan_length),
                format_value(result.wall_time_s),
                format_value(result.search_time_s),
                format_value(result.wall_time_ratio_vs_local, precision=3),
                format_value(result.plan_length_delta_vs_local),
                format_value(result.expanded),
                format_value(result.generated),
            ]
        )
    return render_markdown_table(headers, rows)


def render_comparison_table(results: list[PlannerResult]) -> str:
    rows_by_count: dict[int, dict[str, PlannerResult]] = {}
    for result in results:
        rows_by_count.setdefault(result.distractions, {})[result.planner] = result

    headers = [
        "distractions",
        "local_wall_s",
        "fd_wall_s",
        "symk_wall_s",
        "fd_vs_local",
        "symk_vs_local",
        "local_expanded",
        "fd_expanded",
        "symk_expanded",
    ]
    rows: list[list[str]] = []
    for distractions in COUNTS:
        result_group = rows_by_count[distractions]
        local_result = result_group["tensor_planner_pddl"]
        fd_result = result_group["fast_downward_astar_lmcut"]
        symk_result = result_group["symk_sym_bd"]
        rows.append(
            [
                str(distractions),
                format_value(local_result.wall_time_s),
                format_value(fd_result.wall_time_s),
                format_value(symk_result.wall_time_s),
                format_value(fd_result.wall_time_ratio_vs_local, precision=3),
                format_value(symk_result.wall_time_ratio_vs_local, precision=3),
                format_value(local_result.expanded),
                format_value(fd_result.expanded),
                format_value(symk_result.expanded),
            ]
        )
    return render_markdown_table(headers, rows)


def render_baseline_table(
    current_results: list[PlannerResult], baseline_results: list[PlannerResult] | None
) -> str:
    if not baseline_results:
        return "No prior `results.json` baseline was available for before/after comparison."

    current_index = result_index(current_results)
    baseline_index = result_index(baseline_results)
    headers = [
        "distractions",
        "solved_before",
        "solved_after",
        "plan_before",
        "plan_after",
        "wall_before_s",
        "wall_after_s",
        "wall_delta",
        "generated_before",
        "generated_after",
        "generated_delta",
    ]
    rows: list[list[str]] = []
    for distractions in COUNTS:
        current = current_index.get(("tensor_planner_pddl", distractions))
        baseline = baseline_index.get(("tensor_planner_pddl", distractions))
        if current is None or baseline is None:
            continue
        generated_delta = None
        if current.generated is not None and baseline.generated is not None:
            generated_delta = current.generated - baseline.generated
        rows.append(
            [
                str(distractions),
                format_value(baseline.solved),
                format_value(current.solved),
                format_value(baseline.plan_length),
                format_value(current.plan_length),
                format_value(baseline.wall_time_s),
                format_value(current.wall_time_s),
                format_percent_delta(baseline.wall_time_s, current.wall_time_s),
                format_value(baseline.generated),
                format_value(current.generated),
                format_signed(generated_delta),
            ]
        )

    if not rows:
        return "No matching local-planner rows were available in the prior `results.json` baseline."
    return render_markdown_table(headers, rows)


def render_high_distraction_notes(
    current_results: list[PlannerResult], baseline_results: list[PlannerResult] | None
) -> list[str]:
    current_index = result_index(current_results)
    baseline_index = result_index(baseline_results or [])
    notes: list[str] = []

    matched_counts = [
        distractions
        for distractions in COUNTS
        if ("tensor_planner_pddl", distractions) in current_index
        and ("tensor_planner_pddl", distractions) in baseline_index
    ]
    if matched_counts:
        solved_regressions = [
            distractions
            for distractions in matched_counts
            if baseline_index[("tensor_planner_pddl", distractions)].solved
            and not current_index[("tensor_planner_pddl", distractions)].solved
        ]
        if solved_regressions:
            notes.append(
                "Solved regressions vs baseline at local-planner counts: "
                + ", ".join(str(count) for count in solved_regressions)
                + "."
            )
        else:
            notes.append(
                "Local planner kept solved=yes across all baseline-matched sweep counts."
            )

    for distractions in (1024, 2560):
        current = current_index.get(("tensor_planner_pddl", distractions))
        baseline = baseline_index.get(("tensor_planner_pddl", distractions))
        if current is None or baseline is None:
            notes.append(
                f"No prior local-planner baseline row was available for distraction count {distractions}."
            )
            continue
        generated_delta = None
        if current.generated is not None and baseline.generated is not None:
            generated_delta = current.generated - baseline.generated
        notes.append(
            f"Local planner @ {distractions}: solved {format_value(baseline.solved)}→{format_value(current.solved)}, "
            f"plan {format_value(baseline.plan_length)}→{format_value(current.plan_length)}, "
            f"wall {format_value(baseline.wall_time_s)}s→{format_value(current.wall_time_s)}s ({format_percent_delta(baseline.wall_time_s, current.wall_time_s)}), "
            f"generated {format_value(baseline.generated)}→{format_value(current.generated)} ({format_signed(generated_delta)})."
        )
    return notes


def render_summary(
    results: list[PlannerResult], baseline_results: list[PlannerResult] | None
) -> str:
    high_distraction_notes = render_high_distraction_notes(results, baseline_results)
    return "\n".join(
        [
            "# Crafting planner benchmark summary",
            "",
            "## Comparison by distraction count",
            render_comparison_table(results),
            "",
            "## Before/after vs prior results.json baseline (local planner)",
            render_baseline_table(results, baseline_results),
            "",
            "## High-distraction notes",
            *[f"- {note}" for note in high_distraction_notes],
            "",
            "## Per-run details",
            render_table(results),
            "",
            "- `wall_vs_local` is each planner's wall time divided by the local planner wall time for the same distraction count.",
            "- `plan_vs_local` is each planner's plan length minus the local planner plan length for the same distraction count.",
            "- `count_index` preserves the default sweep order for stable downstream comparisons.",
        ]
    )


def main() -> int:
    domain_path, problem_paths = ensure_pddl_files()
    results_path = PDDL_DIR / "results.json"
    baseline_results = load_existing_results(results_path)

    results = run_local_planner()
    for problem_path in problem_paths.values():
        results.append(
            run_fd_like(
                "fast_downward_astar_lmcut",
                FD_DIR,
                "astar(lmcut())",
                domain_path,
                problem_path,
            )
        )
        results.append(
            run_fd_like("symk_sym_bd", SYMK_DIR, "sym_bd()", domain_path, problem_path)
        )

    results = augment_results(results)
    results.sort(key=lambda item: (item.count_index, item.planner_sort_order))

    results_path.write_text(
        json.dumps([asdict(result) for result in results], indent=2)
    )

    summary_path = PDDL_DIR / "summary.md"
    summary = render_summary(results, baseline_results)
    summary_path.write_text(summary + "\n")
    print(summary)
    print(f"\nWrote {results_path.relative_to(ROOT)}")
    print(f"Wrote {summary_path.relative_to(ROOT)}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
