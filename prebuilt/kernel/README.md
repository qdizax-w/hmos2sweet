# curtana kernel (not committed yet)

`Image.gz`, `cust-atoll-ab.dtb` and `dtbo.img` for the Redmi Note 9S are produced by
`build/build_kernel.sh` (crDroid sm6250 + `src/kernel/patches`). It needs Linux, so the easiest way is the
GitHub Actions workflow `.github/workflows/build.yml` ("build curtana prebuilts"): download its
`curtana-prebuilts` artifact and unzip it in the repository root.

sweet's kernel from hmos2sweet was removed on purpose: it is built for the sm6150 SoC and does not boot on curtana.
