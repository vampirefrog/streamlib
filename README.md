# StreamLib

[![Linux Build](https://github.com/vampirefrog/streamlib/actions/workflows/linux.yml/badge.svg)](https://github.com/vampirefrog/streamlib/actions/workflows/linux.yml) [![MSYS2 MINGW64 Build](https://github.com/vampirefrog/streamlib/actions/workflows/msys2-mingw64.yml/badge.svg)](https://github.com/vampirefrog/streamlib/actions/workflows/msys2-mingw64.yml)

Streaming I/O for stdio, memory blocks and files in zip archives. Supports memory mapping where available, and transparent gzip decompression.

| module   |    | gzip | open | read | write | seek | eof | tell | mmap | munmap | close |
|----------|----|------|------|------|-------|------|-----|------|------|--------|-------|
| mem      | R  | no   | ⛔   | ⛔   | ⛔    | ⛔   | ⛔  | ⛔   | ⛔   | ⛔     | ⛔    |
|          | R  | yes  | ☑️   | ☑️   | ⛔    | ☑️³  | ☑️  | ☑️   | ☑️²  | ☑️²    | ☑️    |
|          | W  | no   | ☑️   | ☑️   | ☑️    | ☑️   | ☑️  | ☑️   | ☑️   | ☑️     | ☑️    |
|          | W  | yes  | ☑️   | ⛔   | ☑️    | ☑️¹  | ⛔  | ☑️   | ⛔   | ⛔     | ☑️    |
|          | RW | no   | ☑️   | ☑️   | ☑️    | ☑️   | ☑️  | ☑️   | ☑️   | ☑️     | ☑️    |
|          | RW | yes  | ⛔   | ⛔   | ⛔    | ⛔   | ⛔  | ⛔   | ⛔   | ⛔     | ⛔    |
| file     | R  | no   | ☑️   | ☑️   | ⛔    | ☑️   | ☑️  | ☑️   | ☑️   | ☑️     | ☑️    |
|          | R  | yes  | ☑️   | ☑️   | ⛔    | ☑️³  | ☑️  | ☑️   | ☑️²  | ☑️²    | ☑️    |
|          | W  | no   | ☑️   | ⛔   | ☑️    | ☑️   | ☑️  | ☑️   | ☑️   | ☑️     | ☑️    |
|          | W  | yes  | ☑️   | ⛔   | ☑️    | ☑️¹  | ⛔  | ☑️   | ⛔   | ⛔     | ☑️    |
|          | RW | no   | ☑️   | ☑️   | ☑️    | ☑️   | ☑️  | ☑️   | ☑️   | ☑️     | ☑️    |
|          | RW | yes  | ⛔   | ⛔   | ⛔    | ⛔   | ⛔  | ⛔   | ⛔   | ⛔     | ⛔    |
| zip_file | R  | no   | ☑️   | ☑️   | ⛔    | ☑️⁴  | ☑️  | ☑️   | ☑️   | ☑️     | ☑️    |
|          | R  | yes  | ☑️   | ☑️   | ⛔    | ☑️³ʼ⁴| ☑️  | ☑️   | ☑️²  | ☑️²    | ☑️    |
|          | W  | no   | ⛔   | ⛔   | ⛔    | ⛔   | ⛔  | ⛔   | ⛔   | ⛔     | ⛔    |
|          | W  | yes  | ⛔   | ⛔   | ⛔    | ⛔   | ⛔  | ⛔   | ⛔   | ⛔     | ⛔    |
|          | RW | no   | ⛔   | ⛔   | ⛔    | ⛔   | ⛔  | ⛔   | ⛔   | ⛔     | ⛔    |
|          | RW | yes  | ⛔   | ⛔   | ⛔    | ⛔   | ⛔  | ⛔   | ⛔   | ⛔     | ⛔    |

1. [Forward only](https://www.zlib.net/manual.html#Gzip)
2. Slurp into memory block
3. [Slow](https://www.zlib.net/manual.html#Gzip)
4. [Uncompressed data only](https://libzip.org/documentation/zip_fseek.html#DESCRIPTION)

# Building

1. Clone the repository:

```sh
git clone https://github.com/vampirefrog/streamlib.git
cd streamlib
```

2. Compile the library:

    ```sh
    make HAVE_LIBZIP=1 HAVE_GZIP=1
    ```
