# Development Rules

All development in this project must follow the rules below.

## 1. Build System — CMake

- All C++ projects must use **CMake** with a `CMakeLists.txt` file.
- Keep `CMakeLists.txt` structured and organized.

## 2. Reduce Code Redundancy

- Do NOT duplicate logic across modules or projects.
- For frequently used functions or shared source code, create a dedicated **`library/`** folder and place reusable code inside it.
- Before writing new code, check if a similar utility already exists in the shared library.

## 3. Coding Standards

- Follow all rules defined in [Coding/rule.md](../../Coding/rule.md).
- File signature **VIATECH-GENERAL** on every source file.

## 4. Memory Management

- Use `std::shared_ptr` (or `std::unique_ptr` where ownership is exclusive) to pass data across functions, threads, and modules.
- Avoid raw memory copies when the same data can be shared via a smart pointer.

## 5. Threading

- Replace polling loops with `std::condition_variable` (`wait_for` with timeout) for low-power waits.
- Dedicated consumer threads, one queue + CV each (data-capture / publish pipelines).

## 6. External Rules

- Configuration → [Coding/config_rule.md](../../Coding/config_rule.md) + [config.md](config.md)
- Testing → [Coding/testing_rule.md](../../Coding/testing_rule.md)
- HTTPS → [Coding/HTTP.md](../../Coding/HTTP.md)
- ZMQ → [Coding/ZMQ.md](../../Coding/ZMQ.md)
- Errors & logging → [Coding/error_logging.md](../../Coding/error_logging.md)
- File signatures → [Coding/signature.md](../../Coding/signature.md)
- Folder structure → [Coding/folder_structure.md](../../Coding/folder_structure.md)
