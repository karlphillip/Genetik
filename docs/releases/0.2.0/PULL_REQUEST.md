Title: Prepare Genetik 0.2.0 development pre-release

This brings the complete development stack since v0.1.0 into review: canonical
workspace outputs; owned typed mathematics/regression; opt-in scientific operators;
bounded expression parsing; scalar templates; first partials; weighted value/partial
regression; and bounded root integral models with full-model integrand fitting.
All mathematical specializations use the generic OOP engine and existing execution,
randomness, budget and publication contracts.

Version 0.2.0 starts a new SameMinorVersion series. See docs/MIGRATION.md for explicit
package request changes and consistent translation-unit recompilation. Public target,
include spelling, strict FP, C++20, Threads and minimum CMake3.16 remain supported.

The exact review packet supplies candidate SHA, hosted matrix/job links, named
portable/TSan results, source archive/checksum and extracted-source verification.
Do not merge until those results cover the exact candidate. Proposed integration
is an ordinary fast-forward preserving that SHA; no squash/rebase/merge commit.
Recheck current main and qualify any changed integration candidate before tagging.
Release remains a development pre-release for the explicitly tested Linux scope.
