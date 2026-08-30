import time
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[1]
SCRIPTS_DIR = Path(__file__).resolve().parent


def benchmark(fn, label):
    start = time.perf_counter()
    fn()
    elapsed_ms = (time.perf_counter() - start) * 1000
    print(f'{label}: {elapsed_ms:.2f} ms')


def run_python_compile_check():
    for script in sorted(SCRIPTS_DIR.glob('*.py')):
        compile(script.read_text(encoding='utf-8'), str(script), 'exec')


def run_repo_scan():
    for path in sorted(REPO_ROOT.rglob('*')):
        if path.is_file() and path.suffix.lower() in {'.py', '.md', '.h', '.cpp', '.ino'}:
            path.read_text(encoding='utf-8', errors='ignore')


if __name__ == '__main__':
    print('Benchmarking repo validation path...')
    benchmark(run_python_compile_check, 'Python compile check')
    benchmark(run_repo_scan, 'Repo scan')
