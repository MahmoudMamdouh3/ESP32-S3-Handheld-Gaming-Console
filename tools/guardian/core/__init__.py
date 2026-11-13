"""Core hardware, memory, and static analyzers for BMO Handheld firmware."""

from .ast_linter import FirmwareAstLinter, LintFinding
from .bus_model import BusMetrics, HardwareBusModel, SUPPORTED_RESOLUTIONS
from .clang_tidy_runner import ClangTidyFinding, ClangTidyRunner
from .cppcheck_runner import CppcheckFinding, CppcheckRunner
from .elf_analyzer import ElfAnalysisResult, ElfAnalyzer, SymbolEntry
from .psram_budget import PsramBudgetAnalyzer, PsramBudgetReport

__all__ = [
    "FirmwareAstLinter",
    "LintFinding",
    "HardwareBusModel",
    "BusMetrics",
    "SUPPORTED_RESOLUTIONS",
    "ClangTidyRunner",
    "ClangTidyFinding",
    "CppcheckRunner",
    "CppcheckFinding",
    "ElfAnalyzer",
    "ElfAnalysisResult",
    "SymbolEntry",
    "PsramBudgetAnalyzer",
    "PsramBudgetReport",
]
