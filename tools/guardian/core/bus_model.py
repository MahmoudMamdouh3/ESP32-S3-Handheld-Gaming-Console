"""
tools.guardian.core.bus_model — Mathematical Hardware Bus & Frame Budget Physics Model
Provides theoretical calculations and hardware boundaries for the ESP32-S3 FSPI bus,
memory bandwidth, frame transmission times, and maximum compute budgets.
"""

from dataclasses import dataclass
from typing import Dict, List, Tuple


@dataclass(frozen=True)
class ConsoleResolution:
    name: str
    native_w: int
    native_h: int
    rendered_w: int
    rendered_h: int
    color_depth_bits: int
    target_fps: float
    description: str

    @property
    def frame_bytes(self) -> int:
        return self.rendered_w * self.rendered_h * (self.color_depth_bits // 8)

    @property
    def pixels_per_frame(self) -> int:
        return self.rendered_w * self.rendered_h


# Canonical display resolution database for all supported systems on BMO
SUPPORTED_RESOLUTIONS: Dict[str, ConsoleResolution] = {
    "GAMEBOY_DMG_SCALED": ConsoleResolution(
        name="Game Boy DMG (1.5x Scaled)",
        native_w=160,
        native_h=144,
        rendered_w=240,
        rendered_h=216,
        color_depth_bits=16,
        target_fps=59.73,
        description="Nintendo Game Boy scaled 3:2 to fit 240x216 centered viewport",
    ),
    "GAMEBOY_COLOR_SCALED": ConsoleResolution(
        name="Game Boy Color (1.5x Scaled)",
        native_w=160,
        native_h=144,
        rendered_w=240,
        rendered_h=216,
        color_depth_bits=16,
        target_fps=59.73,
        description="Nintendo Game Boy Color scaled 3:2 to 240x216",
    ),
    "NES_NATIVE": ConsoleResolution(
        name="NES (Native 256x240)",
        native_w=256,
        native_h=240,
        rendered_w=256,
        rendered_h=240,
        color_depth_bits=16,
        target_fps=60.098,
        description="Nintendo Entertainment System native 256x240 on 320x240 display",
    ),
    "DOOM_NATIVE": ConsoleResolution(
        name="DOOM (Native 320x200)",
        native_w=320,
        native_h=200,
        rendered_w=320,
        rendered_h=200,
        color_depth_bits=16,
        target_fps=35.0,
        description="Classic DOOM 320x200 centered vertically on 320x240 display",
    ),
    "SMS_NATIVE": ConsoleResolution(
        name="Sega Master System (Native 256x192)",
        native_w=256,
        native_h=192,
        rendered_w=256,
        rendered_h=192,
        color_depth_bits=16,
        target_fps=59.92,
        description="Sega Master System native 256x192 centered",
    ),
    "GAMEGEAR_NATIVE": ConsoleResolution(
        name="Game Gear (Native 160x144)",
        native_w=160,
        native_h=144,
        rendered_w=160,
        rendered_h=144,
        color_depth_bits=16,
        target_fps=59.92,
        description="Sega Game Gear native 160x144 centered",
    ),
    "PCE_NATIVE": ConsoleResolution(
        name="PC Engine (256x240)",
        native_w=256,
        native_h=240,
        rendered_w=256,
        rendered_h=240,
        color_depth_bits=16,
        target_fps=59.82,
        description="NEC PC Engine / TurboGrafx-16 standard resolution",
    ),
    "ATARI2600_NATIVE": ConsoleResolution(
        name="Atari 2600 (160x192)",
        native_w=160,
        native_h=192,
        rendered_w=160,
        rendered_h=192,
        color_depth_bits=16,
        target_fps=60.0,
        description="Atari 2600 NTSC 160x192 centered",
    ),
    "PICO8_NATIVE": ConsoleResolution(
        name="PICO-8 (128x128)",
        native_w=128,
        native_h=128,
        rendered_w=128,
        rendered_h=128,
        color_depth_bits=16,
        target_fps=30.0,
        description="PICO-8 fantasy console native 128x128 centered",
    ),
    "GENESIS_NATIVE": ConsoleResolution(
        name="Sega Genesis (320x224)",
        native_w=320,
        native_h=224,
        rendered_w=320,
        rendered_h=224,
        color_depth_bits=16,
        target_fps=59.92,
        description="Sega Genesis / Mega Drive mode 4/5 centered",
    ),
    "SNES_NATIVE": ConsoleResolution(
        name="Super Nintendo (256x224)",
        native_w=256,
        native_h=224,
        rendered_w=256,
        rendered_h=224,
        color_depth_bits=16,
        target_fps=60.098,
        description="Super Nintendo SNES standard 256x224 centered",
    ),
    "WONDERSWAN_NATIVE": ConsoleResolution(
        name="WonderSwan (224x144)",
        native_w=224,
        native_h=144,
        rendered_w=224,
        rendered_h=144,
        color_depth_bits=16,
        target_fps=75.47,
        description="Bandai WonderSwan / Color horizontal orientation",
    ),
    "NGP_NATIVE": ConsoleResolution(
        name="Neo Geo Pocket (160x152)",
        native_w=160,
        native_h=152,
        rendered_w=160,
        rendered_h=152,
        color_depth_bits=16,
        target_fps=59.73,
        description="SNK Neo Geo Pocket / Color centered",
    ),
    "LYNX_NATIVE": ConsoleResolution(
        name="Atari Lynx (160x102)",
        native_w=160,
        native_h=102,
        rendered_w=160,
        rendered_h=102,
        color_depth_bits=16,
        target_fps=75.0,
        description="Atari Lynx LCD native 160x102 centered",
    ),
    "COLECO_NATIVE": ConsoleResolution(
        name="ColecoVision (256x192)",
        native_w=256,
        native_h=192,
        rendered_w=256,
        rendered_h=192,
        color_depth_bits=16,
        target_fps=60.0,
        description="ColecoVision / SG-1000 TMS9918A native 256x192",
    ),
    "MENU_FULLSCREEN": ConsoleResolution(
        name="Menu UI Fullscreen (320x240)",
        native_w=320,
        native_h=240,
        rendered_w=320,
        rendered_h=240,
        color_depth_bits=16,
        target_fps=60.0,
        description="Full 320x240 framebuffer for BMO Carousel & Museum UI",
    ),
}


@dataclass
class BusMetrics:
    resolution_key: str
    resolution: ConsoleResolution
    spi_clock_hz: int
    cpu_clock_hz: int
    spi_transfer_ms: float
    frame_period_ms: float
    max_theoretical_fps: float
    sequential_cpu_budget_ms: float
    sequential_cpu_budget_percent: float
    parallel_dma_cpu_budget_ms: float
    parallel_dma_cpu_budget_percent: float
    spi_bus_utilization_percent: float
    dma_required_for_60fps: bool
    status: str


class HardwareBusModel:
    """Physics model of the ESP32-S3 MCU and ST7789 display SPI bus."""

    def __init__(self, spi_clock_hz: int = 80_000_000, cpu_clock_hz: int = 240_000_000):
        self.spi_clock_hz = spi_clock_hz
        self.cpu_clock_hz = cpu_clock_hz
        self.bit_time_ns = 1_000_000_000 / spi_clock_hz  # 12.5 ns @ 80MHz
        self.byte_time_ns = self.bit_time_ns * 8          # 100 ns @ 80MHz
        self.addr_window_overhead_us = 1.2                # SPI setup overhead

    def calculate_metrics(self, res_key: str) -> BusMetrics:
        if res_key not in SUPPORTED_RESOLUTIONS:
            raise KeyError(f"Unknown resolution key: {res_key}")
        res = SUPPORTED_RESOLUTIONS[res_key]
        
        # Raw wire transfer time
        raw_bytes = res.frame_bytes
        transfer_ns = (raw_bytes * self.byte_time_ns) + (self.addr_window_overhead_us * 1000)
        transfer_ms = transfer_ns / 1_000_000.0
        
        frame_period_ms = 1000.0 / res.target_fps
        max_theoretical_fps = 1000.0 / transfer_ms if transfer_ms > 0 else 0.0
        
        # In sequential execution (no DMA): CPU cannot run while SPI is blocking
        sequential_cpu_budget_ms = max(0.0, frame_period_ms - transfer_ms)
        sequential_cpu_budget_percent = (sequential_cpu_budget_ms / frame_period_ms) * 100.0
        
        # In parallel execution (with DMA double-buffering): CPU can use 100% of frame period
        # as long as CPU computation <= frame_period_ms and transfer_ms <= frame_period_ms
        parallel_dma_cpu_budget_ms = frame_period_ms
        parallel_dma_cpu_budget_percent = 100.0
        
        spi_bus_utilization = (transfer_ms / frame_period_ms) * 100.0
        
        # Is DMA required for 60 FPS without dropping frames?
        # If SPI transfer consumes > 50% of the frame budget in sequential mode,
        # CPU emulator cycles are heavily pinched and will drop frames on complex scenes.
        dma_required = sequential_cpu_budget_percent < 40.0 or transfer_ms > frame_period_ms
        
        if transfer_ms > frame_period_ms:
            status = "CRITICAL_SPI_SATURATION"
        elif dma_required:
            status = "PINCHED_NEEDS_DMA"
        else:
            status = "HEALTHY_SEQUENTIAL"
            
        return BusMetrics(
            resolution_key=res_key,
            resolution=res,
            spi_clock_hz=self.spi_clock_hz,
            cpu_clock_hz=self.cpu_clock_hz,
            spi_transfer_ms=transfer_ms,
            frame_period_ms=frame_period_ms,
            max_theoretical_fps=max_theoretical_fps,
            sequential_cpu_budget_ms=sequential_cpu_budget_ms,
            sequential_cpu_budget_percent=sequential_cpu_budget_percent,
            parallel_dma_cpu_budget_ms=parallel_dma_cpu_budget_ms,
            parallel_dma_cpu_budget_percent=parallel_dma_cpu_budget_percent,
            spi_bus_utilization_percent=spi_bus_utilization,
            dma_required_for_60fps=dma_required,
            status=status,
        )

    def calculate_all(self) -> List[BusMetrics]:
        return [self.calculate_metrics(k) for k in SUPPORTED_RESOLUTIONS.keys()]
