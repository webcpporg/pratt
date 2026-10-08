# pratt

pratt is a header-only Pratt parser engine for C++20, with a decimal
calculator built on it, one of the libraries of
[webcpp](https://github.com/webcpporg/webcpp).

## Building and testing

pratt is developed inside the webcpp superproject, as a Boost library is
developed inside Boost:

    git clone --recursive https://github.com/webcpporg/webcpp
    cd webcpp
    b2 libs/pratt/test

builds and runs the tests natively.
`toolset=clang-wasip2 testing.launcher=wasmtime` and
`toolset=clang-wasip3 testing.launcher=wasmtime` do the same for WASI, with
the toolsets that `user-config.jam` registers against wasi-sdk. A b2 project
uses pratt through `/webcpp/pratt//pratt`.

## License

Distributed under the [Boost Software License, Version 1.0](LICENSE_1_0.txt).
