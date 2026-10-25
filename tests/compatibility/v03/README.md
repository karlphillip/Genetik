# Published0.3.0 checkpoint producer fixture

These three headers are byte-for-byte copies from published commit
ac2786e824198cead70b32ae44229be35baed1ff, indexed in SOURCE.json. They are test-only,
never installed, and compiled in a separate producer process/translation unit.
All other scalar/core dependencies remain byte-identical to that baseline in this
clean release integration. The producer's genuine0.3.0 envelope and scalar codec
are read by a0.4.0 consumer built with the same flags/runtime. Full trajectory/state
comparison follows restoration, not merely a rewritten version token. Negative
fixtures test the codec/runtime whitelist. No published file is modified.
