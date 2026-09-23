# Tests

This script:

```bash
./run_tests.sh

```

prepares a virtual environment in `tests/.venv`, and then runs the tests.
Optionally you can add `--log-level=debug` for more detailed logging.

The test set-up expects a DALI-USB device connected to `/dev/ttyUSB0`, electrically connected with its inputs to a DALI power supply.

## Dependencies

The dependencies are declared in `tests/pyproject.toml` as dependency groups,
see PEP 735. The `test` group holds what the test suite imports, the `lint`
group holds the `ruff` formatter and linter. Installing them

```bash
python3 -m pip install --group test --group lint
```

needs pip 25.1 or newer, which `run_tests.sh` takes care of.

## Formatting and linting

```bash
ruff format scripts/ force_collision/
ruff check scripts/ force_collision/
```

The settings live in the same `tests/pyproject.toml`. The line length is not
enforced.
