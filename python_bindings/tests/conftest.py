"""
Pytest configuration and fixtures for FastestJSONInTheWest Python bindings tests.
"""

import pytest
import sys
from pathlib import Path

# Add source directory to path for imports
sys.path.insert(0, str(Path(__file__).parent.parent / "src"))


@pytest.fixture
def sample_json_simple():
    """Fixture providing a simple JSON string."""
    return '{"name": "test", "value": 42}'


@pytest.fixture
def sample_json_complex():
    """Fixture providing a complex JSON structure."""
    return '''{
        "user": {
            "id": 1,
            "name": "Alice",
            "email": "alice@example.com",
            "roles": ["admin", "user"],
            "metadata": {
                "created": "2025-11-17",
                "active": true,
                "score": 95.5
            }
        },
        "status": "success"
    }'''


@pytest.fixture
def sample_json_array():
    """Fixture providing a JSON array."""
    return '[{"id": 1}, {"id": 2}, {"id": 3}]'


@pytest.fixture
def sample_json_unicode():
    """Fixture providing JSON with Unicode characters."""
    return '''{
        "languages": {
            "english": "hello",
            "japanese": "こんにちは",
            "arabic": "مرحبا",
            "hebrew": "שלום",
            "emoji": "😀🎉🚀"
        }
    }'''


@pytest.fixture
def sample_json_edge_cases():
    """Fixture providing JSON with edge cases."""
    return '''{
        "empty_string": "",
        "zero": 0,
        "negative": -42,
        "very_small": 1e-10,
        "very_large": 1e100,
        "empty_array": [],
        "empty_object": {},
        "null_value": null,
        "escaped_quotes": "He said \\"hello\\"",
        "escaped_backslash": "path\\\\to\\\\file"
    }'''


@pytest.fixture
def sample_json_deeply_nested():
    """Fixture providing deeply nested JSON."""
    return '{"a":{"b":{"c":{"d":{"e":{"f":{"g":{"h":{"i":{"j":42}}}}}}}}}}'


@pytest.fixture
def sample_json_large_array():
    """Fixture providing a large JSON array."""
    items = [f'{{"id": {i}, "value": {i*2}}}' for i in range(100)]
    return '[' + ','.join(items) + ']'


class JsonValidator:
    """Helper class for validating parsed JSON."""
    
    @staticmethod
    def is_valid_number(value):
        """Check if value is a valid number."""
        return isinstance(value, (int, float)) and not isinstance(value, bool)
    
    @staticmethod
    def is_valid_string(value):
        """Check if value is a valid string."""
        return isinstance(value, str)
    
    @staticmethod
    def is_valid_boolean(value):
        """Check if value is a valid boolean."""
        return isinstance(value, bool)
    
    @staticmethod
    def is_valid_null(value):
        """Check if value is a valid null."""
        return value is None
    
    @staticmethod
    def is_valid_array(value):
        """Check if value is a valid array."""
        return isinstance(value, list)
    
    @staticmethod
    def is_valid_object(value):
        """Check if value is a valid object."""
        return isinstance(value, dict)


@pytest.fixture
def json_validator():
    """Fixture providing JSON validator."""
    return JsonValidator()


# Custom markers
def pytest_configure(config):
    """Configure custom pytest markers."""
    config.addinivalue_line(
        "markers", "benchmark: mark test as a benchmark test"
    )
    config.addinivalue_line(
        "markers", "slow: mark test as slow running"
    )
    config.addinivalue_line(
        "markers", "unicode: mark test as unicode-related"
    )
    config.addinivalue_line(
        "markers", "edge_case: mark test as testing edge cases"
    )


# Pytest hooks for test reporting
def pytest_collection_modifyitems(config, items):
    """Modify test collection to add markers based on test names."""
    for item in items:
        # Auto-add markers based on test function names
        if "unicode" in item.nodeid.lower():
            item.add_marker(pytest.mark.unicode)
        if "performance" in item.nodeid.lower() or "benchmark" in item.nodeid.lower():
            item.add_marker(pytest.mark.benchmark)
        if "slow" in item.nodeid.lower():
            item.add_marker(pytest.mark.slow)


# Optional: pytest-cov configuration via hook
def pytest_sessionstart(session):
    """Called after the Session object has been created."""
    pass


def pytest_sessionfinish(session, exitstatus):
    """Called after whole test run finished."""
    pass
