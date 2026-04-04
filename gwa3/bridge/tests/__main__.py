"""Entry point for GWA3 bridge integration tests.

Usage:
    python -m bridge.tests                     # Run all tests
    python -m bridge.tests --filter "test_b*"  # Run observation tests only
    python -m bridge.tests --filter "test_me*" # Run specific tests
"""

import asyncio
import argparse
import sys


def main():
    parser = argparse.ArgumentParser(description="GWA3 Bridge Integration Tests")
    parser.add_argument(
        "--filter",
        default=None,
        help="Glob pattern to filter test names (e.g., 'test_b*', 'test_me*')",
    )
    args = parser.parse_args()

    from .runner import run_all

    exit_code = asyncio.run(run_all(filter_pattern=args.filter))
    sys.exit(exit_code)


if __name__ == "__main__":
    main()
