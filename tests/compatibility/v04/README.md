# Published v0.4.0 checkpoint writer headers

These five headers are unmodified copies from `v0.4.0:genetik/` at published
commit `c08859e462210a1ef378b0c6c47a2b4c520f7353`. `SOURCE.json` records their
SHA-256 and Git blob identities. They are included only by checkpoint
compatibility producer targets; they are not installed and do not replace the
current library headers.

The producer targets use these version, envelope, scalar-candidate and
discrete-run codec definitions with the common scalar/discrete engine headers.
They write actual `Genetik-0.4.0/run-state-1` checkpoints. Current 0.5.0
consumers restore the committed state and continue, comparing the entire
serialized state against the producer's uninterrupted trajectory. The fixture
uses only operator IDs and run policies supported by the published v0.4.0
contract. It does not create D5/D6 checkpoints and does not imply that
experimental D6 contexts were published in v0.4.0.
