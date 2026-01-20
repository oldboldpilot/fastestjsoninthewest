"""
Comprehensive tests for SIMD capabilities, level selection, and waterfall conversion.

Tests the new Python API:
- get_simd_capabilities() - Returns SIMDCapabilities with detected features
- SIMDLevel enum - AUTO, AVX512, AMX, AVX2, AVX, SSE4, SSE2, SCALAR
- get_effective_simd_level(level) - Waterfall fallback to available level
- is_simd_level_available(level) - Check if specific level is available
- simd_level_name(level) - Get human-readable name
- to_python(threads=0, simd_level=0) - Convert with SIMD level selection
- to_python_parallel(threads=0, simd_level=0) - Parallel conversion with SIMD level
"""

import fastjson
import pytest
import time
import concurrent.futures


class TestSIMDCapabilities:
    """Test SIMD capability detection."""
    
    def test_get_capabilities(self):
        """Test that get_simd_capabilities returns a valid object."""
        caps = fastjson.get_simd_capabilities()
        assert caps is not None
        
        # All fields should be booleans
        assert isinstance(caps.sse2, bool)
        assert isinstance(caps.sse42, bool)
        assert isinstance(caps.avx, bool)
        assert isinstance(caps.avx2, bool)
        assert isinstance(caps.avx512f, bool)
        assert isinstance(caps.avx512bw, bool)
        assert isinstance(caps.fma, bool)
        assert isinstance(caps.vnni, bool)
        assert isinstance(caps.amx, bool)
        assert isinstance(caps.neon, bool)
        
    def test_capability_hierarchy(self):
        """Test that SIMD capabilities follow expected hierarchy."""
        caps = fastjson.get_simd_capabilities()
        
        # If AVX2 is available, SSE2 should also be available
        if caps.avx2:
            assert caps.sse2
            assert caps.sse42
            
        # If AVX-512 is available, AVX2 should also be available
        if caps.avx512f:
            assert caps.avx2
            
    def test_optimal_path_not_empty(self):
        """Test that optimal_path is always set."""
        caps = fastjson.get_simd_capabilities()
        assert caps.optimal_path is not None
        assert len(caps.optimal_path) > 0
        
    def test_bytes_per_iteration_positive(self):
        """Test that bytes_per_iteration is positive."""
        caps = fastjson.get_simd_capabilities()
        assert caps.bytes_per_iteration > 0
        
    def test_register_count_positive(self):
        """Test that register_count is positive."""
        caps = fastjson.get_simd_capabilities()
        assert caps.register_count >= 1
        
    def test_repr(self):
        """Test that capabilities have a string representation."""
        caps = fastjson.get_simd_capabilities()
        repr_str = repr(caps)
        assert "SIMDCapabilities" in repr_str
        assert "bytes/iter" in repr_str


class TestSIMDLevelEnum:
    """Test SIMDLevel enum."""
    
    def test_all_levels_exist(self):
        """Test that all expected SIMD levels exist."""
        assert hasattr(fastjson, 'SIMDLevel')
        assert hasattr(fastjson.SIMDLevel, 'AUTO')
        assert hasattr(fastjson.SIMDLevel, 'AVX512')
        assert hasattr(fastjson.SIMDLevel, 'AMX')
        assert hasattr(fastjson.SIMDLevel, 'AVX2')
        assert hasattr(fastjson.SIMDLevel, 'AVX')
        assert hasattr(fastjson.SIMDLevel, 'SSE4')
        assert hasattr(fastjson.SIMDLevel, 'SSE2')
        assert hasattr(fastjson.SIMDLevel, 'SCALAR')
        
    def test_level_values(self):
        """Test that SIMD levels have expected integer values."""
        assert fastjson.SIMDLevel.AUTO.value == 0
        assert fastjson.SIMDLevel.AVX512.value == 1
        assert fastjson.SIMDLevel.AMX.value == 2
        assert fastjson.SIMDLevel.AVX2.value == 3
        assert fastjson.SIMDLevel.AVX.value == 4
        assert fastjson.SIMDLevel.SSE4.value == 5
        assert fastjson.SIMDLevel.SSE2.value == 6
        assert fastjson.SIMDLevel.SCALAR.value == 7
        
    def test_level_names(self):
        """Test simd_level_name function."""
        assert "AUTO" in fastjson.simd_level_name(0)
        assert "512" in fastjson.simd_level_name(1)
        assert "AMX" in fastjson.simd_level_name(2)
        assert "AVX2" in fastjson.simd_level_name(3)
        assert "SCALAR" in fastjson.simd_level_name(7)


class TestWaterfallLogic:
    """Test SIMD waterfall fallback logic."""
    
    def test_auto_selects_best(self):
        """Test that AUTO selects the best available level."""
        caps = fastjson.get_simd_capabilities()
        effective = fastjson.get_effective_simd_level(fastjson.SIMDLevel.AUTO.value)
        
        # Effective level should match best_level
        assert effective == caps.best_level
        
    def test_scalar_always_available(self):
        """Test that SCALAR is always available."""
        assert fastjson.is_simd_level_available(fastjson.SIMDLevel.SCALAR.value)
        
        effective = fastjson.get_effective_simd_level(fastjson.SIMDLevel.SCALAR.value)
        assert effective == fastjson.SIMDLevel.SCALAR
        
    def test_waterfall_fallback(self):
        """Test that unavailable levels fall through to lower levels."""
        caps = fastjson.get_simd_capabilities()
        
        # If AVX-512 is not available, requesting it should waterfall down
        if not caps.avx512f:
            effective = fastjson.get_effective_simd_level(fastjson.SIMDLevel.AVX512.value)
            # Should have fallen back to a lower level
            assert effective != fastjson.SIMDLevel.AVX512
            # But shouldn't be higher than requested
            assert effective.value >= fastjson.SIMDLevel.AVX512.value
            
    def test_available_levels_effective(self):
        """Test that available levels are effective unchanged."""
        for level in [fastjson.SIMDLevel.AVX512, fastjson.SIMDLevel.AVX2, 
                      fastjson.SIMDLevel.SSE4, fastjson.SIMDLevel.SSE2,
                      fastjson.SIMDLevel.SCALAR]:
            if fastjson.is_simd_level_available(level.value):
                effective = fastjson.get_effective_simd_level(level.value)
                assert effective == level


class TestConversionWithSIMDLevel:
    """Test to_python conversion with SIMD level selection."""
    
    @pytest.fixture
    def test_data(self):
        """Create test JSON data."""
        return fastjson.parse('''
        {
            "users": [
                {"name": "Alice", "emoji": "🚀", "age": 30},
                {"name": "Bob", "emoji": "🎉", "age": 25},
                {"name": "Charlie", "greeting": "こんにちは", "age": 35}
            ],
            "metadata": {"version": "2.0", "math": "∑∫∂"}
        }
        ''')
        
    def test_default_conversion(self, test_data):
        """Test default conversion (AUTO)."""
        result = test_data.to_python()
        assert "users" in result
        assert len(result["users"]) == 3
        assert result["users"][0]["emoji"] == "🚀"
        
    def test_explicit_avx2_conversion(self, test_data):
        """Test conversion with explicit AVX2 SIMD level."""
        result = test_data.to_python(simd_level=fastjson.SIMDLevel.AVX2.value)
        assert "users" in result
        assert result["users"][2]["greeting"] == "こんにちは"
        
    def test_scalar_conversion(self, test_data):
        """Test conversion with SCALAR fallback."""
        result = test_data.to_python(simd_level=fastjson.SIMDLevel.SCALAR.value)
        assert result["metadata"]["math"] == "∑∫∂"
        
    def test_parallel_conversion(self, test_data):
        """Test parallel conversion with SIMD level."""
        result = test_data.to_python(threads=4, simd_level=fastjson.SIMDLevel.AUTO.value)
        assert "users" in result
        assert len(result["users"]) == 3
        
    def test_to_python_parallel_explicit(self, test_data):
        """Test to_python_parallel method."""
        result = test_data.to_python_parallel(threads=2, simd_level=fastjson.SIMDLevel.AVX2.value)
        assert "metadata" in result


class TestParallelismControl:
    """Test parallelism control in conversion."""
    
    def test_single_thread(self):
        """Test single-threaded conversion."""
        data = fastjson.parse('[1, 2, 3, 4, 5]')
        result = data.to_python(threads=1)
        assert result == [1.0, 2.0, 3.0, 4.0, 5.0]
        
    def test_multi_thread(self):
        """Test multi-threaded conversion."""
        # Create large array for parallel processing
        large_json = '[' + ','.join(str(i) for i in range(10000)) + ']'
        data = fastjson.parse(large_json)
        
        result = data.to_python(threads=4)
        assert len(result) == 10000
        
    def test_thread_safety_concurrent(self):
        """Test thread safety with concurrent parsing and conversion."""
        test_json = '{"value": 42, "text": "hello world"}'
        
        def parse_and_convert(n):
            data = fastjson.parse(test_json)
            return data.to_python(threads=2)
        
        with concurrent.futures.ThreadPoolExecutor(max_workers=8) as executor:
            results = list(executor.map(parse_and_convert, range(100)))
            
        assert len(results) == 100
        assert all(r["value"] == 42.0 for r in results)


class TestWaterfallOrderDocumentation:
    """Test that waterfall order is documented."""
    
    def test_waterfall_order_exists(self):
        """Test that SIMD_WATERFALL_ORDER is available."""
        assert hasattr(fastjson, 'SIMD_WATERFALL_ORDER')
        assert isinstance(fastjson.SIMD_WATERFALL_ORDER, list)
        
    def test_waterfall_order_complete(self):
        """Test that waterfall order contains all levels."""
        order = fastjson.SIMD_WATERFALL_ORDER
        assert len(order) == 7
        assert any("AVX-512" in s for s in order)
        assert any("AVX2" in s for s in order)
        assert any("SSE" in s for s in order)
        assert any("SCALAR" in s for s in order)


class TestPerformanceWithSIMDLevels:
    """Test performance differences between SIMD levels."""
    
    def test_simd_vs_scalar_performance(self):
        """Test that SIMD conversion is not slower than scalar."""
        large_json = '[' + ','.join(f'{{"val": {i}}}' for i in range(5000)) + ']'
        data = fastjson.parse(large_json)
        
        # Warm up
        data.to_python()
        
        # Time AUTO (best available)
        start = time.perf_counter()
        for _ in range(10):
            data.to_python(simd_level=fastjson.SIMDLevel.AUTO.value)
        auto_time = time.perf_counter() - start
        
        # Time SCALAR
        start = time.perf_counter()
        for _ in range(10):
            data.to_python(simd_level=fastjson.SIMDLevel.SCALAR.value)
        scalar_time = time.perf_counter() - start
        
        # AUTO should not be significantly slower than SCALAR
        # (it's actually expected to be faster, but we're just checking correctness)
        print(f"\nAUTO: {auto_time*1000:.2f}ms, SCALAR: {scalar_time*1000:.2f}ms")
        # Allow 50% margin for variance
        assert auto_time <= scalar_time * 1.5 or auto_time < 0.1


if __name__ == "__main__":
    pytest.main([__file__, "-v"])
