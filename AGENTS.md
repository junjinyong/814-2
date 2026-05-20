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
- Stage 2 of the roadmap is implemented:
  - `Evaluate.cpp` now returns a composite `Fitness`
  - the current fitness fields are `prefix_score`, `frontier_hits`, and `cover_9999`
  - `frontier_hits` currently counts readable integers in the next `256` values after the first missing integer
  - fitness comparison is lexicographic in the order above
  - `Evaluate.cpp` still provides an `annealing_value()` scalar embedding of that lexicographic order, but the current stage 5 main search loop no longer uses simulated-annealing acceptance
- Stage 3 of the roadmap is implemented:
  - `Search.cpp` now maintains a bounded global elite archive of strong boards
  - the current elite archive capacity is `16`
  - local-best improvements are inserted into the archive
  - after stagnation, a thread now restarts from a lightly perturbed elite board instead of only reheating around its own local best
- Stage 4 of the roadmap is implemented:
  - the main move set now mixes one-cell mutation, swap mutation, and small patch mutation
  - the move schedule is now adaptive: cheap one-cell moves dominate early, while swap/patch moves are used more during stagnation
  - archive-based restarts now use the expanded move set for larger perturbations
  - restart seeding now mixes elite-based restarts with occasional full random restarts for diversity
  - short greedy local improvement is now reserved for promising accepted candidates and restart seeds, rather than every candidate
- Stage 5 is architecturally implemented:
  - each thread now maintains a small local population instead of a single active board
  - offspring now come from either mutation-only reproduction or crossover-plus-mutation reproduction
  - parent selection is tournament-based, and strong offspring replace weaker local individuals
  - stagnation refresh currently reseeds the weaker part of the population from archive/random restart seeds
  - crossover is currently patch/row-band style and remains strictly thread-local
  - crossover is now adaptive: mutation dominates during progress, while crossover becomes more frequent only after stagnation builds up
  - a shared migration pool now allows periodic export/import of strong individuals across thread-local populations
- The current baseline search is a finite-run steady-state local-population memetic evolutionary search with archive-assisted refresh and migration.
- The current stage 5 pool policy is tiered:
  - the strongest `local_stable_keep` individuals are protected from probabilistic churn
  - the remaining tail acts as an exchangeable exploration pool
  - exploratory candidates can replace tail members through a diversity-biased probabilistic admission rule using `annealing_value()` and `Random.cpp::update()`
  - regular immigrant candidates from restart-seeded or random boards can now enter the local population even when they are not produced by parent-based mutation/crossover
  - newly inserted tail candidates now receive a short tenure (`protected_turns`) during which normal tail replacement, migration, and refresh avoid evicting them immediately
- The current main search loop is therefore hybrid:
  - elite/stable slots are still managed by mostly elitist evolutionary replacement
  - only the exchangeable tail currently uses SA-like probabilistic admission
  - there is still no global temperature schedule across the whole population
- `Random.cpp` now uses thread-local RNG state, which avoids the previous shared-generator race.
- `main.cpp` currently prints the final best score and board after all worker threads finish.
- A longer user-run of the stage 1 baseline on 10 threads reached a final best score of `2551`, and the printed global-best log remained monotone throughout the run.
- A short stage 2 smoke run showed repeated global-best improvements with unchanged `prefix_score` but improved secondary metrics, confirming that the richer fitness is actively affecting search decisions.
- A longer user-run of the stage 2 solver on 10 threads reached a final best score of `3565` with `frontier_hits=247` and `cover_9999=9571`, a substantial improvement over the stage 1 baseline.
- A short stage 3 smoke run built and ran successfully, showing continued monotone global-best updates under the archive-based restart policy.
- A short stage 4 smoke run built and ran successfully, confirming that the expanded move set and local-improvement pass integrate cleanly with the archive-based solver.
- After tuning, the stage 4 solver still built and ran successfully with the cheaper default move policy and conditional local improvement.
- A user-run after the stage 4 tuning reached `3676` substantially faster than the pre-tuning stage 4 version, indicating that cheap-default moves plus conditional heavy refinement is an effective policy and should be preserved unless stronger evidence appears.
- A short smoke run after the first stage 5 substep built and ran successfully, confirming that the local-population search integrates cleanly with the existing archive and mutation/refinement policy.
- A short smoke run after adding crossover to stage 5 built and ran successfully, confirming that crossover-based offspring integrate cleanly with the local-population solver.
- A full user-run of the current stage 5 crossover version finished at `3064`, which is substantially worse than the tuned stage 4 result of `3676`. The current crossover design should therefore be treated as a regression until improved or disabled.
- A short smoke run after tuning stage 5 crossover scheduling built and ran successfully, confirming that the adaptive crossover policy integrates cleanly with the current local-population solver.
- A full user-run after tuning stage 5 crossover scheduling finished at `3305`. This is better than the previous `3064` stage 5 result, but it still underperforms the tuned stage 4 result of `3676`, so the current stage 5 remains a net regression in end-to-end score.
- A short smoke run after adding migration built and ran successfully, confirming that migration integrates cleanly with the current stage 5 solver. Long-run migration performance is still unverified.
- A full user-run after adding migration finished at `2964`, which is worse than both the pre-migration stage 5 result (`3305`) and the tuned stage 4 result (`3676`). The current migration design should therefore be treated as a harmful source of homogenization rather than a successful improvement.
- A short smoke run after adding a protected stable core plus probabilistically replaceable exploration tail built and ran successfully. Long-run performance of this new admission policy is still unverified.
- A full user-run after adding the protected stable core plus probabilistically replaceable exploration tail finished at `3468`, which is a large improvement over the immediately previous stage 5 variants (`3305` and `2964`) but still below the tuned stage 4 result of `3676`.
- A short smoke run after adding newborn tenure for newly inserted exploration-tail individuals built and ran successfully. Long-run performance of the tenure policy is still unverified.

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
