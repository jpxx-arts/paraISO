# paraISO

A set of C utilities to explore `ISO 9660` filesystems directly, reading and parsing the structure of an `.iso` file byte-by-byte according to the ECMA-119 standard.

-----

## 📜 About The Project

This project was developed as a course assignment for Operating Systems. It serves as a practical, hands-on study of C programming and low-level filesystem implementation. The goal is to build, from scratch, a set of tools capable of interacting with CD-ROM/DVD images by directly parsing the data structures defined in the ECMA-119 / ISO 9660 standard.

The implementation uses the Path Table for efficient directory lookups, as well as navigation through the Directory Entry hierarchy.

## ✨ Features

  * **`ls_iso`**: Lists files and subdirectories within a specific directory in the ISO image. Can be applied recursively and can use extension filters.
    * **`extract mode`**: Extracts files of a directory.
  * **`cat_iso`**: Displays the content of a file from within the ISO image directly to the terminal.
  * **Direct Parsing**: No third-party libraries are used for filesystem manipulation; all logic for reading the PVD, Path Table, and Directory Entries is implemented manually.

## 🚀 Getting Started

Follow the steps below to compile and run the project in a Linux environment.

### Prerequisites

You will need `git`, the `gcc` compiler, and `make` installed.

### Compilation

1.  **Clone the repository:**

    ```bash
    git clone https://github.com/jpxx-arts/paraISO.git
    ```

2.  **Enter the project directory:**

    ```bash
    cd paraISO
    ```

3.  **Compile the project using the Makefile:**

    ```bash
    make
    ```

    This command will compile the shared library and the `ls_iso` and `cat_iso` executables, placing them in the `bin/` directory.

## 💻 Usage

The executables will be available in the `bin/` directory.

### ls\_iso

Lists the contents of a directory.

**Syntax:**

```bash
./bin/ls_iso [list|extract] <path_to_iso_file> <path_to_directory_in_iso> [extension]
```

**Examples:**

```bash
# List the contents of a directory
./bin/ls_iso my_image.iso /EFI/BOOT
./bin/ls_iso list my_image.iso /EFI/BOOT

# List the files with a specific extension
./bin/ls_iso my_image.iso /EFI/BOOT .txt
./bin/ls_iso list my_image.iso /EFI/BOOT .txt

# Extract all files of a directory
./bin/ls_iso extract my_image.iso /EFI/BOOT

# Extract the files with a specific extension
./bin/ls_iso extract my_image.iso /EFI/BOOT .txt

```

### cat\_iso

Displays the contents of a file.

**Syntax:**

```bash
./bin/cat_iso <path_to_iso_file> <path_to_file_in_iso>
```

**Example:**

```bash
# Display the contents of README.TXT from the root
./bin/cat_iso my_image.iso /README.TXT
```

## 📁 Project Structure

```
paraISO/
├── bin/          # <-- Final executables (created by 'make')
├── obj/          # <-- Intermediate object files (created by 'make')
├── src/          # <-- All source code (.c, .h)
│   ├── cat_iso.c
│   ├── iso9660.c
│   ├── iso9660.h
│   └── ls_iso.c
└── Makefile      # <-- Build instructions
```
