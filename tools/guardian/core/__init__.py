# SPDX-License-Identifier: MIT OR GPL-2.0-or-later
"""Core hardware, memory, and static analyzers for BMO Handheld firmware."""

from .ast_linter import FirmwareAstLinter, LintFinding
from .bus_model import SUPPORTED_RESOLUTIONS, BusMetrics, HardwareBusModel
from .clang_tidy_runner import ClangTidyFinding, ClangTidyRunner
from .cppcheck_runner import CppcheckFinding, CppcheckRunner
from .elf_analyzer import ElfAnalysisResult, ElfAnalyzer, SymbolEntry
from .psram_budget import PsramBudgetAnalyzer, PsramBudgetReport

__all__ = [
    "SUPPORTED_RESOLUTIONS",
    "BusMetrics",
    "ClangTidyFinding",
    "ClangTidyRunner",
    "CppcheckFinding",
    "CppcheckRunner",
    "ElfAnalysisResult",
    "ElfAnalyzer",
    "FirmwareAstLinter",
    "HardwareBusModel",
    "LintFinding",
    "PsramBudgetAnalyzer",
    "PsramBudgetReport",
    "SymbolEntry",
]
