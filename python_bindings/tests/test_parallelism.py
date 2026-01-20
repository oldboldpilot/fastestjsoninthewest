import fastjson
import multiprocessing

def test_parallelism_limit():
    max_threads = multiprocessing.cpu_count()
    current_threads = fastjson.get_num_threads()
    
    expected_limit = max(1, int(max_threads * 0.3))
    
    print(f"Max CPU cores: {max_threads}")
    print(f"FastJSON current threads: {current_threads}")
    print(f"Expected limit (30%): {expected_limit}")
    
    assert current_threads == expected_limit

if __name__ == "__main__":
    test_parallelism_limit()
