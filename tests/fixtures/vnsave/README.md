# VNSAVE Fixtures

`v1/runtime-v1.0.0-s3.vnsave` is a frozen runtime-session save produced by the
`v1.0.0` tag at commit `4f3132deb61cbdd14bbffc4649309da5c7172ef6`.

Generation inputs:

1. Legacy `assets/demo/demo.vnpak` rebuilt from that tag.
2. Scene `S3`, scalar backend, `12` frame limit, `16 ms` step.
3. One frame executed before `vn_runtime_session_save_to_file`.
4. Slot `110`, timestamp `1000000`.

The fixture is 781 bytes with SHA256
`2c603a1c999429d8c5b90592577f9a71b574ef2bafccd568e4a82834827d9392`.
Do not regenerate it with newer runtime code.
