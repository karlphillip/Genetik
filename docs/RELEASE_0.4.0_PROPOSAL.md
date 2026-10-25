# Proposed Genetik 0.4.0 development delivery

Status: local preparation only. No remote upload is authorized. Exact candidate,
complete commit/file ranges, local results, asset hashes and proposed commands are
recorded under out/runs/discrete-d4/preflight; assets are generated from the exact
committed source after verification. Read [integration](DISCRETE_RELEASE_INTEGRATION.md),
[notes](releases/0.4.0/NOTES.md) and [migration](releases/0.4.0/MIGRATION.md).

First request one approval to push the reviewed clean candidate to
karlphillip/Genetik:qualification/discrete-mathematics, execute/review the established
three-job hosted matrix, push focused qualification fixes/reports to that branch,
and qualify its exact final report tip. Existing research branches are not uploaded
as a dependency. Keep all original67research commits and evidence locally.

After successful hosted qualification, regenerate/verify assets if the candidate
changed. Prepare a PR titled "Prepare Genetik 0.4.0 discrete mathematics development
pre-release" from qualification/discrete-mathematics into main, with the audited
D1–D3 scope, migration/compatibility, negative discovery results and qualification
links. Request separate final delivery approval before opening the PR, changing
main, tagging or publishing. Prefer an ordinary fast-forward to the exact qualified
candidate if current remote main is its ancestor; never force/squash/discard
concurrent work. A different resulting commit requires qualification before tagging.

After separately approved integration, review the complete main matrix at that
exact SHA before annotated v0.4.0. Proposed title: "Genetik 0.4.0 — development
pre-release". Proposed flags: prerelease=true, latest=false, draft=false. Upload
Genetik-0.4.0-source.tar.gz and SHA256SUMS to a temporary draft, download/hash-check,
then publish and independently verify public bytes/flags. Existing tags and
published0.1/0.2/0.3 assets remain unchanged. None of these delivery actions are
covered by the initial upload request.
