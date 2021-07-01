# ReGenny

ReGenny is a reverse engineering tool to reconstruct data structures in real-time and generate usable C++ header files. Header file generation is done by the sister project [SdkGenny](https://github.com/cursey/sdkgenny). 

![Early build preview](resources/readme_preview.png)

## Building

ReGenny uses [CMake](https://cmake.org/) and [vcpkg](https://vcpkg.io/en/index.html). Both need to be installed and properly configured. Currently ReGenny is only tested on Visual Studio 2019 for Windows 10 environments. 

## Status

ReGenny is still early in development but is usable. Things may change unexpectedly. Existing projects may break and need to be updated. Many more features are planned. There will be bugs.

## Design decisions

* ReGenny uses plaintext project files instead of binary ones (`.genny` and `.json`). Plaintext formats are much better for inclusion in git repositories and makes collaborating with others on ReGenny projects easier since you can diff/merge project files.
* Tree based display (like [ReClass.NET](https://github.com/ReClassNET/ReClass.NET)) but you build the tree using `.genny` files.
* ReGenny uses [SdkGenny](https://github.com/cursey/sdkgenny) for output. One of [SdkGenny](https://github.com/cursey/sdkgenny)'s primary goals is to generate good output and ReGenny reaps all the benefits from that. Instead of a single monolithic header file, ReGenny uses [SdkGenny](https://github.com/cursey/sdkgenny) to generate a proper header file hierarchy for inclusion into your project. If your project is already using [SdkGenny](https://github.com/cursey/sdkgenny) then you don't even need to generate header files since you can just use the `.genny` file directly.
* Since ReGenny uses [SdkGenny](https://github.com/cursey/sdkgenny), it supports everything `.genny` files do:
	* Namespaces
	* Enums
	* Structs
	* Classes
	* Function prototypes
	* Static function prototypes
	* Bitfields
	* Arrays
	* Multi-dimensional arrays
	* Pointers
	* Namespaces can be nested within each other
	* Structs, enums and classes can be nested within other structs/classes
	* Bring your own external types
* The `.genny` format is flexible enough to parse simple C/C++ structures directly with zero (or minimal) modification making importing existing structures into ReGenny easy.