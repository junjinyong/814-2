# AGENTS.md

## Purpose

This repository is for developing a solver for BOJ 18789, `814 - 2`.

This document is the primary handoff note for LLM agents working in this repo. Treat it as both:

- a self-contained briefing for a new agent that has not seen the code before
- a living scratchpad that must be updated when important facts are discovered

If you learn something that a future agent would need in order to continue productively, write it here before you finish your turn.

## Working Contract For LLM Agents

Read this file before making substantial changes.

When working in this repository:

1. Use this document as the source of truth for project context.
2. Verify facts from code or measurements when possible; do not promote guesses to facts.
3. If you inspect code, discover a bug, validate an assumption, measure a score, or choose a direction that materially affects future work, update this file.
4. Keep the document self-contained. A new agent should be able to read this file and immediately understand:
   - what problem is being solved
   - what the repo currently contains
   - what is known, unknown, and recently validated
5. Prefer concise, high-signal updates over long diaries. Preserve durable facts; avoid clutter from one-off command output.
6. Do not overwrite or revert user changes unless explicitly instructed.
7. Delete or rewrite stale notes when they stop being useful. Do not preserve document history for its own sake.
8. Ask the user only about material decisions, ambiguous requirements, or major tradeoffs. Decide routine implementation details locally and move forward.
9. Keep validation runs short by default. For long solver runs, either ask the user for permission first or ask the user to run the program and provide the resulting score/output.

## Recommended Loop

Use this loop whenever you work on the solver:

1. Read `AGENTS.md`.
2. Inspect the relevant code and current repository state.
3. Decide what is known, what is assumed, and what must be verified next.
4. Make the smallest useful change that advances the solver or improves understanding.
5. Validate the change if feasible.
6. Update `AGENTS.md` with new durable information.

## Roadmap

Planned staged upgrades for the solver. Each stage must leave behind a working solver, not a half-finished rewrite.

1. Replace the current shared-state search with a trustworthy thread-local baseline:
   - each thread keeps its own board, score, temperature, and RNG
   - threads publish only improved global best solutions
   - mutation remains simple, starting with one-cell edits
2. Extend evaluation from a single raw score to a richer fitness:
   - actual BOJ prefix score
   - frontier coverage near the first missing integer
   - broad coverage such as readable count within `1..9999`
3. Add an elite archive:
   - store a small set of strong boards
   - use them as restart seeds instead of restarting from scratch
4. Expand the move set:
   - one-cell mutation
   - swap mutation
   - small patch mutation
   - short local improvement after mutation
5. Upgrade to an island-style memetic evolutionary solver:
   - thread-local populations
   - crossover plus mutation
   - periodic elite migration between islands

## Repository Snapshot

Current top-level files:

- `CMakeLists.txt`
- `README.md`
- `main.cpp`
- `Search.cpp`
- `Evaluate.cpp`
- `Random.cpp`

Current interpretation:

- This is an early C++ prototype for generating and evaluating candidate boards.
- The repository already contains search and evaluation code, but future agents should not assume it is correct just because it exists.
- Before editing, check for local uncommitted changes and work around them carefully.
- Local IDE state and build artifacts such as `.idea/`, `build/`, `cmake-build-*/`, `.agents/`, and `.codex/` should remain ignored rather than committed.

## Problem Overview

Target problem:

- BOJ 18789
- Title: `814 - 2`
- Type: output-only / score optimization problem

High-level goal:

- Output an `8 x 14` grid of digits (`0` through `9`).
- The score of the output is the largest integer `X` such that every integer from `1` through `X` can be read from the grid, while `X + 1` cannot.
- To pass, the submission must achieve at least `8140` points.

## Problem Statement, Rewritten

Construct and print an `8 x 14` table consisting only of digits `0` to `9`.

A number is considered readable from the table if:

- you start from any cell
- you move to a neighboring cell at each step
- neighbors include horizontal, vertical, and diagonal adjacency
- the digits on the visited cells are concatenated in order

Movement rules:

- revisiting previously used cells is allowed
- jumping over cells is not allowed
- staying on the same cell for the next digit is not allowed

Scoring rule:

- If all integers from `1` to `X` are readable and `X + 1` is not readable, the score is `X`

Input/output format:

- Input: none
- Output: exactly 8 lines, each representing one row of 14 digits

## Formal Model

Let the board be `B[8][14]`, where each `B[r][c]` is a digit in `{0,1,...,9}`.

A path of length `L >= 1` is a sequence of cells:

- `(r0, c0), (r1, c1), ..., (r_{L-1}, c_{L-1})`

such that for every consecutive pair:

- `max(|r_i - r_{i+1}|, |c_i - c_{i+1}|) = 1`

This means each step moves to one of the 8 neighboring cells, never remaining in place.

The digit string induced by the path is:

- `B[r0][c0] B[r1][c1] ... B[r_{L-1}][c_{L-1}]`

The corresponding readable number is the decimal integer represented by that digit string.

## Known Facts

- This is not a conventional exact algorithm problem; the intended work is solver construction and search.
- Because revisits are allowed, arbitrarily long digit strings can be generated in principle.
- The challenge is to design a board whose induced language of path strings covers a long prefix of positive integers.
- On 2026-05-13, `Evaluate.cpp` was corrected to use 8-direction adjacency instead of 4-direction adjacency.
- The same evaluator fix also removed obvious copy-paste bugs in deeper path expansion and changed the return value to the actual BOJ score `X`, not the first missing integer itself.
- The current evaluator enumerates path lengths 1 through 5 and stores readable integers below `20000`.
- For the pass threshold `8140`, this evaluator is effectively exact with respect to digit length, because every relevant target integer has at most 4 digits.
- Stage 1 of the roadmap is implemented:
  - each thread now runs an independent local search chain with its own board, score, temperature, and RNG
  - the mutation operator is still a single-cell digit change
  - threads publish only strictly better global best solutions under a mutex
- The baseline search is currently a finite-run local simulated annealing style search with local reheating after stagnation.
- `Random.cpp` now uses thread-local RNG state, which avoids the previous shared-generator race.
- `main.cpp` currently prints the final best score and board after all worker threads finish.

## Open Questions To Track

Future agents should verify and document answers here when resolved:

- How should leading zeros be treated in practice for score evaluation?
- What is the most efficient exact evaluator for checking whether a candidate board covers `1..X`?
- What search objective works best: direct score, softened score, coverage deficits, automaton-based heuristics, or hybrid methods?
- What board structures or digit-placement motifs are empirically strong for early coverage?
- Which parts of the existing C++ code are salvageable, and which should be rewritten?

## Update Policy

When you learn something durable, add it in the appropriate section instead of scattering notes randomly.

This file is not an append-only log. If a note becomes outdated, redundant, or no longer useful for future work, delete it or rewrite it in place.

Good candidates for updates:

- corrected understanding of the problem
- verified semantics or edge cases
- benchmarked scores with exact commands
- algorithm decisions that future agents should continue from
- code architecture notes that affect future edits
- removal of stale assumptions that would otherwise mislead a future agent

Do not leave critical discoveries only in chat history; copy them here.
