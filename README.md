# StreamLib

StreamLib is a versatile C library designed to handle various types of streams, including memory streams, file streams, and zip file streams. The library provides a unified interface for stream operations, making it easier to manage different data sources in a consistent manner.

## Features

- **Generic Stream Interface:** Unified interface for different stream types.
- **Memory Streams:** Handle in-memory data efficiently.
- **File Streams:** Seamless integration with file I/O operations.
- **Zip File Streams:** Support for reading streams from zip archives (when compiled with libzip).
- **Convenience Functions:** Read/write utility functions for various data types.

## Table of Contents

- [Installation](#installation)
- [Usage](#usage)
- [API Documentation](#api-documentation)
- [Contributing](#contributing)

## Installation

### Prerequisites

- C compiler (e.g., GCC)
- GNU Make (optional, for building examples and tests)
- libzip (optional, for zip file stream support)

### Building the Library

1. Clone the repository:

    ```sh
    git clone https://github.com/vampirefrog/streamlib.git
    cd streamlib
    ```

2. Compile the library:

    ```sh
    make
    ```
## Usage

### Basic Example

Below is a simple example demonstrating how to use StreamLib to read from a file stream:

```c
#include <stdio.h>
#include "stream.h"

int main() {
    struct file_stream fstream;
    if (file_stream_init(&fstream, "example.txt", "r") != 0) {
        fprintf(stderr, "Failed to open file\n");
        return 1;
    }

    char buffer[128];
    size_t read_bytes = stream_read((struct stream *)&fstream, buffer, sizeof(buffer) - 1);
    buffer[read_bytes] = '\0';

    printf("Read data: %s\n", buffer);

    stream_close((struct stream *)&fstream);
    return 0;
}
```

### Advanced Usage

For advanced usage and more examples, please refer to the `examples` directory in the repository.

## API Documentation

Detailed API documentation is available in the `docs` directory. You can also generate the documentation using Doxygen:

```sh
doxygen Doxyfile
```

This will generate the documentation in the `docs` directory, which you can open in a web browser.

## Contributing

Contributions are welcome! Please follow these steps to contribute:

1. Fork the repository.
2. Create a new branch (`git checkout -b feature-branch`).
3. Make your changes and commit them (`git commit -am 'Add new feature'`).
4. Push to the branch (`git push origin feature-branch`).
5. Create a new Pull Request.

Please ensure that your code follows the existing coding style and includes appropriate tests.

---

We hope you find StreamLib useful! If you have any questions or feedback, feel free to open an issue on GitHub. Happy coding!
