# How to Run This Example

This example demonstrates how to use `nob.hpp` to build a C++ project with a static library.

## Steps to Run

1.  **Navigate to this directory:**
    Open your terminal and change the directory to `how_to`.

    ```bash
    cd path/to/nob.hpp/how_to
    ```

2.  **Compile the build script:**
    Use a C++ compiler (like g++) to compile `nob.cpp`. This will create our build program.

    ```bash
    g++ -o nob nob.cpp
    ```

3.  **Run the build:**
    Execute the compiled build program. This will compile all the source files, create a static library, and link everything into a final executable.

    ```bash
    ./nob
    ```
    You can also clean the build artifacts with:
    ```bash
    ./nob clean
    ```

4.  **Run the executable:**
    The final executable `main` will be located in the `build` directory. Run it to see the output.

    ```bash
    ./build/main
    ```

    You should see the following output:
    ```
    Lexing: Hello, World!
    Parsing: Hello, World!
    Executing: Hello, World!
    ```