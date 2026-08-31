"""
tools.guardian.core — Core analytical and benchmarking engines for BMO Gameboy.
"""

from .ast_linter import FirmwareAstLinter, LintFinding
from .bus_model import BusMetrics, HardwareBusModel, SUPPORTED_RESOLUTIONS
from .cppcheck_runner import CppcheckFinding, CppcheckRunner
from .elf_analyzer import ElfAnalysisResult, ElfAnalyzer, SymbolEntry
from .host_bench import BenchmarkMetric, HostBenchmarkSuite
from .report_gen import ReportGenerator

__all__ = [
    "FirmwareAstLinter",
    "LintFinding",
    "HardwareBusModel",
    "BusMetrics",
    "SUPPORTED_RESOLUTIONS",
    "CppcheckRunner",
    "CppcheckFinding",
    "ElfAnalyzer",
    "ElfAnalysisResult",
    "SymbolEntry",
    "HostBenchmarkSuite",
    "BenchmarkMetric",
    "ReportGenerator",
]
