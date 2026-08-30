/**
 * console_history_data.js
 * Authoritative historical database of video game console hardware specifications,
 * engineering breakthroughs, game development history, and landmark titles.
 */

const CONSOLE_HISTORY_DATABASE = {
  ROM_ATARI: {
    id: "ROM_ATARI",
    name: "Atari 2600",
    company: "Atari, Inc.",
    year: 1977,
    generation: "2nd Generation (8-bit)",
    tagline: "The Pioneer of Programmable Home Consoles & Cartridge Culture",
    badgeColor: "#E056FD",
    ext: ".a26, .a78",
    specs: {
      cpu: "MOS Technology 6507 @ 1.19 MHz",
      ram: "128 Bytes (Integrated in 6532 RIOT)",
      vram: "0 Bytes (Racing the Beam via TIA)",
      resolution: "160 × 192 pixels (Variable clock lines)",
      colors: "128 colors (NTSC) / 104 colors (PAL), 4 colors per scanline",
      sound: "Television Interface Adaptor (TIA) — 2 audio channels (square/noise)"
    },
    historicalSignificance: 
      "The Atari 2600 revolutionized the industry by separating game software from hardware through ROM cartridges. " +
      "Engineers had to 'race the beam'—calculating every scanline in real-time within 76 CPU cycles with zero framebuffer RAM. " +
      "It established the home video game industry and introduced the world to home-based arcade conversions.",
    engineeringConstraints:
      "Having only 128 BYTES of RAM and NO video memory meant game state, object positions, and timers had to fit in fewer bytes " +
      "than a modern tweet. David Crane (Pitfall!) and Carol Shaw (River Raid) engineered sprite-mirroring and cycle-counting wizardry.",
    hallmarkGames: [
      { title: "Pitfall!", year: 1982, significance: "Created the side-scrolling platformer genre with 255 screens procedurally generated in 4KB ROM." },
      { title: "Space Invaders", year: 1980, significance: "First official arcade license for home consoles; quadrupled Atari console sales." },
      { title: "River Raid", year: 1982, significance: "Pioneered procedural scrolling shooter mechanics, designed by industry-first female designer Carol Shaw." },
      { title: "Adventure", year: 1979, significance: "Created the action-adventure genre and contained the first famous hidden video game Easter Egg (Warren Robinett)." }
    ]
  },

  ROM_COLEM: {
    id: "ROM_COLEM",
    name: "ColecoVision",
    company: "Coleco Industries",
    year: 1982,
    generation: "2nd Generation (8-bit)",
    tagline: "The Arcade-Quality Revolution in the Living Room",
    badgeColor: "#00D2D3",
    ext: ".col, .sg",
    specs: {
      cpu: "Zilog Z80A @ 3.58 MHz",
      ram: "1 KB System RAM",
      vram: "16 KB Video RAM (Texas Instruments TMS9918A VDP)",
      resolution: "256 × 192 pixels",
      colors: "16 fixed colors, 32 hardware sprites",
      sound: "Texas Instruments SN76489 — 3 tone generators + 1 noise generator"
    },
    historicalSignificance:
      "ColecoVision bridged the gulf between blurry home consoles and vibrant arcade cabinets. " +
      "With 16KB of dedicated VRAM and hardware sprite generation, it offered near-perfect ports of arcade hits like Donkey Kong, " +
      "proving that home systems could deliver authentic arcade graphics and audio.",
    engineeringConstraints:
      "The TMS9918A VDP limited sprites to 4 per scanline, requiring clever multiplexing. " +
      "Coleco bundled Donkey Kong as a pack-in game—a masterstroke that sold 1 million units in 14 months.",
    hallmarkGames: [
      { title: "Donkey Kong", year: 1982, significance: "The benchmark arcade pack-in that defined ColecoVision's visual supremacy." },
      { title: "Zaxxon", year: 1982, significance: "Brought isometric 3D scrolling shooting into home living rooms." },
      { title: "Cabbage Patch Kids: Adventures in the Park", year: 1984, significance: "Pioneered multi-directional platforming mechanics." },
      { title: "Looping", year: 1983, significance: "Showcased complex physics and obstacle navigation." }
    ]
  },

  ROM_NES: {
    id: "ROM_NES",
    name: "Nintendo NES / Famicom",
    company: "Nintendo Co., Ltd.",
    year: 1983,
    generation: "3rd Generation (8-bit)",
    tagline: "The Resurrector of Modern Video Games & Game Design Standards",
    badgeColor: "#FF5E57",
    ext: ".nes",
    specs: {
      cpu: "Ricoh 2A03 (6502 core + custom APU) @ 1.79 MHz",
      ram: "2 KB Work RAM + Cartridge Bank-Switching (Mappers)",
      vram: "2 KB Picture Processing Unit (PPU) VRAM",
      resolution: "256 × 240 pixels (NTSC visible: 256 × 224)",
      colors: "54-color master palette, 25 colors on screen simultaneously",
      sound: "Custom 5-Channel APU (2 Pulse, 1 Triangle, 1 Noise, 1 DPCM Sample)"
    },
    historicalSignificance:
      "Following the 1983 North American video game crash, Nintendo revived the global market with the Famicom/NES. " +
      "Shigeru Miyamoto and Takashi Tezuka established modern game design grammars: camera scrolling, momentum physics, " +
      "metroidvania non-linear exploration, and cinematic narrative direction.",
    engineeringConstraints:
      "2KB RAM forced developers to invent Memory Management Controllers (MMC/Mappers) inside cartridges to bank-switch " +
      "ROM data and add extra RAM. Composers like Koji Kondo wrote immortal soundtracks using just 5 primitive wave channels.",
    hallmarkGames: [
      { title: "Super Mario Bros.", year: 1985, significance: "The gold standard of platforming physics, smooth horizontal scrolling, and level design." },
      { title: "The Legend of Zelda", year: 1986, significance: "First console game with battery-backed save RAM; created open-world exploration." },
      { title: "Metroid", year: 1986, significance: "Invented the Metroidvania genre with atmospheric backtracking, power-ups, and a female protagonist." },
      { title: "Mega Man 2", year: 1988, significance: "Masterclass in non-linear boss order, tight controls, and chiptune sound engineering." }
    ]
  },

  ROM_SMS: {
    id: "ROM_SMS",
    name: "Sega Master System",
    company: "Sega Enterprises",
    year: 1985,
    generation: "3rd Generation (8-bit)",
    tagline: "Sega's High-Color 8-Bit Heavyweight & Arcade Heritage",
    badgeColor: "#3B82F6",
    ext: ".sms",
    specs: {
      cpu: "Zilog Z80 @ 3.58 MHz (Twice as fast as the NES 6502)",
      ram: "8 KB Work RAM (4× more than NES)",
      vram: "16 KB Video RAM (8× more than NES)",
      resolution: "256 × 192 pixels (also 256 × 224, 256 × 240)",
      colors: "64-color palette, 32 colors on screen simultaneously (2 palettes of 16)",
      sound: "Texas Instruments SN76489 (3 square + 1 noise) + Optional YM2413 FM Synth"
    },
    historicalSignificance:
      "The Master System was technically superior to the NES in raw CPU speed, RAM, and color palette depth. " +
      "It dominated Europe and Brazil (where it remains culturally legendary to this day) and established Sega's arcade-at-home identity.",
    engineeringConstraints:
      "32 simultaneous colors allowed much more vibrant art than the NES. Games like Phantasy Star introduced full 3D dungeon rendering " +
      "and anime cutscenes in an 8-bit cartridge.",
    hallmarkGames: [
      { title: "Phantasy Star", year: 1987, significance: "Massive 4Mb cartridge with 3D dungeon graphics, animated battles, and complex storytelling." },
      { title: "Alex Kidd in Miracle World", year: 1986, significance: "Sega's beloved mascot platformer before Sonic the Hedgehog." },
      { title: "Wonder Boy III: The Dragon's Trap", year: 1989, significance: "Pioneered animal transformations and open Metroidvania progression." },
      { title: "Sonic the Hedgehog (8-bit)", year: 1991, significance: "Distinct level design and tight mechanics tailored for the 8-bit hardware." }
    ]
  },

  ROM_PCE: {
    id: "ROM_PCE",
    name: "PC Engine / TurboGrafx-16",
    company: "NEC Home Electronics & Hudson Soft",
    year: 1987,
    generation: "4th Generation Bridge (16-bit Graphics)",
    tagline: "The Tiny Powerhouse that Ushered in the 16-Bit Era",
    badgeColor: "#FF6B6B",
    ext: ".pce",
    specs: {
      cpu: "Hudson Soft HuC6280 (8-bit 65C02 custom) @ 7.16 MHz",
      gpu: "HuC6260 Video Color Encoder + HuC6270 Video Display Controller (16-bit)",
      ram: "8 KB Work RAM",
      vram: "64 KB Video RAM",
      resolution: "256 × 240 to 512 × 240 pixels",
      colors: "512-color palette, up to 482 colors on screen simultaneously!",
      sound: "Custom 6-Channel Wavetable Stereo Audio (Custom 5-bit wavetable synthesis)"
    },
    historicalSignificance:
      "PC Engine released in 1987 in Japan (outselling the Famicom for a period) on revolutionary credit-card-sized HuCards. " +
      "It paired an ultra-fast 7.16 MHz CPU with a true 16-bit graphics processor capable of nearly 500 simultaneous colors, " +
      "delivering stunning arcade conversions and the premier home shoot-'em-up library.",
    engineeringConstraints:
      "With 64KB of VRAM and 6 programmable wavetable stereo channels, developers could produce rich synth audio and gigantic, " +
      "smoothly scrolling multi-layer backgrounds years before the SNES arrived.",
    hallmarkGames: [
      { title: "Castlevania: Rondo of Blood", year: 1993, significance: "Considered the pinnacle of traditional Castlevania action and design." },
      { title: "Soldier Blade", year: 1992, significance: "The absolute benchmark for 16-bit vertical scrolling shoot-'em-up design." },
      { title: "Bonk's Adventure", year: 1989, significance: "Iconic platformer showcasing giant animated sprites and vibrant character art." },
      { title: "Blazing Lazers (Gunhed)", year: 1989, significance: "Showcased speed, screen-filling particle effects, and thunderous wavetable audio." }
    ]
  },

  ROM_GB: {
    id: "ROM_GB",
    name: "Nintendo Game Boy",
    company: "Nintendo Co., Ltd.",
    year: 1989,
    generation: "4th Generation Handheld",
    tagline: "Gunpei Yokoi's Masterpiece: Lateral Thinking with Withered Technology",
    badgeColor: "#48DBFB",
    ext: ".gb",
    specs: {
      cpu: "Sharp LR35902 (Z80/8080 hybrid) @ 4.19 MHz",
      ram: "8 KB Internal Work RAM",
      vram: "8 KB Video RAM",
      resolution: "160 × 144 pixels",
      colors: "4 shades of olive green / gray on reflective STN LCD",
      sound: "4-Channel Stereo APU (2 Pulse, 1 Custom Wave, 1 Noise)",
      battery: "30+ hours on 4 AA batteries"
    },
    historicalSignificance:
      "Designed by Gunpei Yokoi, the Game Boy prioritized battery endurance and durability over fancy color screens. " +
      "It became a cultural phenomenon, selling 118 million units (with GBC). It introduced Pokémon, popularized Tetris globally, " +
      "and proved that pure gameplay mechanics trump raw graphics processing power.",
    engineeringConstraints:
      "4 shades of green required pristine sprite silhouette design and distinct contrast shading. " +
      "Satoshi Tajiri spent 6 years squeezing Pokémon Red & Blue's 151 monsters and trading link protocols into a 1MB cartridge.",
    hallmarkGames: [
      { title: "Tetris", year: 1989, significance: "The greatest puzzle game of all time; universal appeal that sold millions of handhelds." },
      { title: "Pokémon Red / Blue", year: 1996, significance: "Created the multi-billion dollar monster collection, breeding, and battle RPG genre." },
      { title: "The Legend of Zelda: Link's Awakening", year: 1993, significance: "Packed a full, emotionally resonant Zelda adventure into a tiny 512KB cart." },
      { title: "Super Mario Land 2: 6 Golden Coins", year: 1992, significance: "Introduced Wario to the Mario universe and pushed DMG graphics to the limit." }
    ]
  },

  ROM_LYNX: {
    id: "ROM_LYNX",
    name: "Atari Lynx",
    company: "Atari Corporation (Designed by Epyx)",
    year: 1989,
    generation: "4th Generation Handheld",
    tagline: "The World's First Color Handheld & Hardware 3D Sprite Scaler",
    badgeColor: "#10AC84",
    ext: ".lnx",
    specs: {
      cpu: "MOS 65SC02 @ 4.0 MHz (Mikey custom chip)",
      gpu: "Suzy 16-bit 16 MHz Co-Processor (Hardware Sprite Scaling & Blitter)",
      ram: "64 KB DRAM",
      resolution: "160 × 102 pixels",
      colors: "4096-color palette, 16 colors per scanline on backlit Color LCD",
      sound: "4-Channel 8-bit DAC stereo sound"
    },
    historicalSignificance:
      "Created by former Amiga engineers RJ Mical and Dave Needle at Epyx, the Lynx was decades ahead of its time. " +
      "Its custom 'Suzy' co-processor featured hardware sprite zoom, scaling, distortion, and collision detection—allowing " +
      "pseudo-3D arcade experiences like Blue Lightning and California Games on a handheld in 1989.",
    engineeringConstraints:
      "The Lynx featured ambidextrous flipping (left-handed mode via screen inversion) and true hardware sprite warping. " +
      "However, its 6 AA batteries lasted only 4 hours, contrasting with the Game Boy's 30-hour battery life.",
    hallmarkGames: [
      { title: "Blue Lightning", year: 1989, significance: "Demonstrated real-time hardware sprite scaling for high-speed jet dogfights." },
      { title: "California Games", year: 1989, significance: "The vibrant surf/skate pack-in showcasing full-color backlit handheld gaming." },
      { title: "Todd's Adventures in Slime World", year: 1990, significance: "Massive interconnected map with support for up to 8-player ComLynx cable." },
      { title: "Chip's Challenge", year: 1989, significance: "Chuck Sommerville's puzzle masterpiece debuted on Lynx before PC porting." }
    ]
  },

  ROM_GENESIS: {
    id: "ROM_GENESIS",
    name: "Sega Genesis / Mega Drive",
    company: "Sega Enterprises",
    year: 1988,
    generation: "4th Generation (16-bit)",
    tagline: "Genesis Does What Nintendon't: Blast Processing & 90s Attitude",
    badgeColor: "#2E86DE",
    ext: ".gen, .md, .smd",
    specs: {
      cpu: "Motorola 68000 @ 7.67 MHz + Zilog Z80 @ 3.58 MHz (Sound Co-Processor)",
      ram: "64 KB Work RAM",
      vram: "64 KB Video RAM",
      resolution: "320 × 224 pixels (also 256 × 224)",
      colors: "512-color palette, 64 simultaneous colors across 4 palettes",
      sound: "Yamaha YM2612 (6 FM Synthesis Channels) + Texas Instruments SN76489 PSG"
    },
    historicalSignificance:
      "The Genesis defined 1990s cool and sparked the golden age of the Console Wars. " +
      "Its Motorola 68000 CPU enabled lightning-fast gameplay, complex physics simulation (Sonic's momentum engine), " +
      "and iconic gritty FM synth basslines that gave birth to electronic music in video games.",
    engineeringConstraints:
      "Yuji Naka utilized the 68000's speed to develop full-loop track curvature and 60fps velocity physics in Sonic. " +
      "Composers like Yuzo Koshiro (Streets of Rage) pushed the YM2612 FM chip to create legendary house and techno club beats.",
    hallmarkGames: [
      { title: "Sonic the Hedgehog 2", year: 1992, significance: "Masterclass in speed, level design, and momentum; introduced the Spin Dash and Tails." },
      { title: "Streets of Rage 2", year: 1992, significance: "The pinnacle of 16-bit side-scrolling beat-'em-ups with an era-defining techno soundtrack." },
      { title: "Gunstar Heroes", year: 1993, significance: "Treasure's technical showcase with multi-jointed bosses, screen rotations, and explosive action." },
      { title: "Phantasy Star IV", year: 1993, significance: "One of the greatest 16-bit JRPGs with manga-panel cutscenes and deep macro battle systems." }
    ]
  },

  ROM_GG: {
    id: "ROM_GG",
    name: "Sega Game Gear",
    company: "Sega Enterprises",
    year: 1990,
    generation: "4th Generation Handheld",
    tagline: "Full-Color Master System Power in the Palm of Your Hand",
    badgeColor: "#F368E0",
    ext: ".gg",
    specs: {
      cpu: "Zilog Z80 @ 3.58 MHz",
      ram: "8 KB Work RAM",
      vram: "16 KB Video RAM",
      resolution: "160 × 144 pixels (Crop of 256 × 192 VDP output)",
      colors: "4096-color master palette, 32 simultaneous colors on backlit color screen",
      sound: "Texas Instruments SN76489 (3 tone + 1 noise with stereo panning)"
    },
    historicalSignificance:
      "Sega modified the Master System hardware into a landscape handheld with a full-color backlit screen and a huge 4096-color palette. " +
      "It became Sega's most successful handheld, selling 10.6 million units, allowing gamers to play color arcade ports on the go.",
    engineeringConstraints:
      "The Game Gear rendered the full 256x192 Master System display inside its VDP and cropped the central 160x144 window. " +
      "Sega released the Master Gear Converter, enabling full backward compatibility with home Master System cartridges.",
    hallmarkGames: [
      { title: "Sonic the Hedgehog: Triple Trouble", year: 1994, significance: "The premier portable Sonic game built specifically for Game Gear hardware." },
      { title: "Shinobi II: The Silent Fury", year: 1992, significance: "Colored ninja elemental abilities and tight, responsive action platforming." },
      { title: "Defenders of Oasis", year: 1992, significance: "Sega's rich portable Arabian-nights-themed original JRPG." },
      { title: "Columns", year: 1990, significance: "Addictive color-matching jewel puzzle pack-in." }
    ]
  },

  ROM_SNES: {
    id: "ROM_SNES",
    name: "Super Nintendo (SNES)",
    company: "Nintendo Co., Ltd.",
    year: 1990,
    generation: "4th Generation (16-bit)",
    tagline: "The Pinnacle of 2D Pixel Art, Mode 7 Scaling & Sampled Audio",
    badgeColor: "#9C88FF",
    ext: ".sfc, .smc",
    specs: {
      cpu: "Ricoh 5A22 (16-bit 65C816 core) @ 3.58 MHz",
      ppu: "Dual Custom PPUs (PPU1 & PPU2) supporting Mode 0 through Mode 7 affine matrix transformations",
      ram: "128 KB Work RAM",
      vram: "64 KB Video RAM",
      sound: "Sony SPC700 8-bit Sound CPU + DSP with 64 KB dedicated Audio RAM (8-channel 32kHz ADPCM stereo)",
      resolution: "256 × 224 pixels (Interlaced modes up to 512 × 448)",
      colors: "32,768-color palette (15-bit RGB), up to 256 colors on screen simultaneously"
    },
    historicalSignificance:
      "The SNES represents the golden apex of 2D sprite artistry, ambient sampled music, and timeless game design. " +
      "Ken Kutaragi's Sony SPC700 sound chip brought orchestral strings, acoustic pianos, and realistic percussion into living rooms. " +
      "Mode 7 affine matrix scaling allowed pseudo-3D flight simulation (Pilotwings, F-Zero, Super Mario Kart).",
    engineeringConstraints:
      "The 65C816 CPU ran at 3.58 MHz (slower than the Genesis 68000), but was augmented by specialized co-processors inside cartridges " +
      "like the Super FX (3D polygonal rendering in Star Fox), DSP-1 (Mario Kart trigonometry), and SA-1 co-processors.",
    hallmarkGames: [
      { title: "Chrono Trigger", year: 1995, significance: "The Dream Team (Sakaguchi, Horii, Toriyama, Mitsuda) masterpiece: multiple endings, seamless active-time battles, and time travel." },
      { title: "Super Metroid", year: 1994, significance: "Environmental storytelling, isolation, and sequence-breaking design perfection." },
      { title: "Super Mario World", year: 1990, significance: "The quintessential launch title; 96 exits, secret paths, and introduced Yoshi." },
      { title: "Donkey Kong Country", year: 1994, significance: "Rare's pre-rendered 3D SGI graphics wizardry that rejuvenated the 16-bit generation." }
    ]
  },

  ROM_WAD: {
    id: "ROM_WAD",
    name: "DOOM / PC Game Engine",
    company: "id Software (John Carmack & John Romero)",
    year: 1993,
    generation: "3D Graphics & FPS Genesis",
    tagline: "The Shareware Juggernaut that Sparked the 3D Gaming Revolution",
    badgeColor: "#FF4757",
    ext: ".wad",
    specs: {
      engine: "id Tech 1 / DOOM Engine (Binary Space Partitioning + Raycasting)",
      render: "Software 3D Perspective Projection, 2.5D Non-Euclidean Sector Heights",
      resolution: "320 × 200 pixels @ 35 FPS (VGA Mode 13h)",
      colors: "256-color indexed palette with dynamic light depth diminish tables",
      memory: "Target: 4 MB RAM on Intel 80486 PC"
    },
    historicalSignificance:
      "DOOM wasn't just a game; it was an earthquake. John Carmack's Binary Space Partitioning (BSP) rendering engine and " +
      "John Romero's aggressive level design defined the First-Person Shooter (FPS) genre. " +
      "It pioneered network deathmatches, modding via open WAD (Where's All the Data) architecture, and revolutionized software distribution.",
    engineeringConstraints:
      "Carmack achieved 35 FPS on humble 386/486 CPUs by pre-computing BSP trees to eliminate hidden surface calculations " +
      "and column-based vertical texture mapping. Running full DOOM on our ESP32-S3 microcontroller is a testament to this engineering elegance.",
    hallmarkGames: [
      { title: "DOOM", year: 1993, significance: "The landmark FPS that transformed gaming culture, multiplayer LAN, and 3D graphics." },
      { title: "DOOM II: Hell on Earth", year: 1994, significance: "Introduced the Super Shotgun, expanded monster rosters, and urban level layouts." },
      { title: "Freedoom: Phase 1 & 2", year: 2006, significance: "Complete open-source game content creating a libre, DRM-free FPS heritage." },
      { title: "FreeDM", year: 2008, significance: "Fast-paced pure competitive arena deathmatch WAD." }
    ]
  },

  ROM_NGP: {
    id: "ROM_NGP",
    name: "Neo Geo Pocket & Color",
    company: "SNK Corporation",
    year: 1998,
    generation: "5th Generation Handheld",
    tagline: "The Arcade Fighter King with the Legendary Microswitch Clicky Stick",
    badgeColor: "#FFA502",
    ext: ".ngp, .ngc",
    specs: {
      cpu: "Toshiba TLCS-900H 16-bit / 32-bit RISC core @ 6.144 MHz + Z80 sound co-cpu",
      ram: "12 KB Work RAM",
      vram: "16 KB Video RAM",
      resolution: "160 × 152 pixels (Reflective TFT Color LCD)",
      colors: "4096-color palette, 146 colors on screen simultaneously (Color mode)",
      controls: "Mechanical 8-way Microswitch Clicky Thumbstick (arcade precision)",
      battery: "40+ hours of battery life on 2 AA batteries"
    },
    historicalSignificance:
      "SNK brought its legendary arcade fighting franchises (King of Fighters, Samurai Shodown, Metal Slug) to a compact handheld. " +
      "Its defining hardware feature was a real mechanical microswitch thumbstick that clicked with arcade tactile precision, " +
      "making diagonal inputs and quarter-circle fighting specials effortless.",
    engineeringConstraints:
      "SNK developers translated complex 4-button arcade fighters into intuitive 2-button control schemes using tap/hold velocity detection, " +
      "producing some of the most fluid, expressive chibi sprite animations ever crafted.",
    hallmarkGames: [
      { title: "SNK vs. Capcom: Card Fighters Clash", year: 1999, significance: "Widely regarded as one of the deepest, most addictive collectible card battle RPGs ever created." },
      { title: "Match of the Millennium (SvC)", year: 1999, significance: "The definitive portable crossover fighter featuring Capcom and SNK rosters." },
      { title: "Metal Slug: 1st & 2nd Mission", year: 1999, significance: "Transferred the chaotic run-and-gun arcade energy into a mission-structured handheld format." },
      { title: "Sonic the Hedgehog Pocket Adventure", year: 1999, significance: "Yuji Naka-directed masterpiece blending Sonic 2 level concepts with new layouts." }
    ]
  },

  ROM_GBC: {
    id: "ROM_GBC",
    name: "Game Boy Color",
    company: "Nintendo Co., Ltd.",
    year: 1998,
    generation: "5th Generation Handheld",
    tagline: "Full Color & Double CPU Speed for the World's Favorite Handheld",
    badgeColor: "#2ED573",
    ext: ".gbc",
    specs: {
      cpu: "Sharp LR35902 (Z80 hybrid) with Dual-Speed Mode @ 8.388 MHz (2× Game Boy speed)",
      ram: "32 KB Work RAM (4× Game Boy)",
      vram: "16 KB Video RAM (2 banks of 8KB)",
      resolution: "160 × 144 pixels (Transreflective Sharp Color TFT LCD)",
      colors: "32,768-color palette (15-bit RGB), up to 56 colors on screen simultaneously",
      features: "Built-in Infrared (IR) Communication Port, full backward compatibility color palettes"
    },
    historicalSignificance:
      "The Game Boy Color doubled CPU frequency and quadrupled RAM while retaining flawless backward compatibility with all 600+ original " +
      "Game Boy cartridges (dynamically applying custom color palettes upon boot). It served as the launchpad for Pokémon Gold & Silver " +
      "and pushed portable pixel art into a radiant new era.",
    engineeringConstraints:
      "Cartridge bank switching allowed massive 4MB cartridges with real-time internal clock crystals (RTC) for day/night cycles in Pokémon.",
    hallmarkGames: [
      { title: "The Legend of Zelda: Oracle of Ages / Seasons", year: 2001, significance: "Capcom-developed dual linked Zelda epic with password and link-cable cross-continuity." },
      { title: "Pokémon Gold / Silver / Crystal", year: 1999, significance: "Doubled the world map (Johto + Kanto), introduced day/night cycles, breeding, and animated sprites." },
      { title: "Shantae", year: 2002, significance: "WayForward's late-generation technical tour-de-force with fluid hand-drawn animations and dynamic lighting." },
      { title: "Super Mario Bros. Deluxe", year: 1999, significance: "Packed the original 1985 NES masterpiece plus The Lost Levels, challenge modes, and multiplayer races." }
    ]
  },

  ROM_WSWAN: {
    id: "ROM_WSWAN",
    name: "Bandai WonderSwan & Color",
    company: "Bandai & Koto Laboratory (Gunpei Yokoi)",
    year: 1999,
    generation: "5th Generation Handheld",
    tagline: "Gunpei Yokoi's Final Vision: Dual Horizontal & Vertical Orientation",
    badgeColor: "#ECCC68",
    ext: ".ws, .wsc",
    specs: {
      cpu: "NEC V30 MZ (16-bit 8086 compatible) @ 3.072 MHz",
      ram: "64 KB Unified SRAM",
      resolution: "224 × 144 pixels (Widescreen 14:9 format)",
      colors: "4096-color palette, 241 colors on screen simultaneously (Color version)",
      orientation: "Dual Form Factor (Playable in Horizontal or Vertical 'TATE' orientation via 2 sets of D-pads)",
      battery: "30+ hours on a SINGLE AA battery!"
    },
    historicalSignificance:
      "Created by Game Boy mastermind Gunpei Yokoi after departing Nintendo, the WonderSwan was engineered around the philosophy of " +
      "maximum battery efficiency and unique ergonomics. Featuring two sets of directional buttons (X-buttons and Y-buttons), games could " +
      "be played horizontally or flipped 90 degrees vertically (TATE mode) for vertical shoot-'em-ups and manga-style text adventures.",
    engineeringConstraints:
      "The 16-bit NEC V30 MZ CPU achieved blazing performance while sipping so little power that it ran for over 30 hours on just ONE single AA battery. " +
      "Square Enix supported the console with premier remakes of Final Fantasy I, II, and IV.",
    hallmarkGames: [
      { title: "Klonoa: Moonlight Museum", year: 1999, significance: "Charming 2.5D puzzle-platformer designed specifically for the WonderSwan widescreen." },
      { title: "Final Fantasy IV (WSC)", year: 2001, significance: "Gorgeous color remake of the 16-bit masterpiece with refined pixel art." },
      { title: "Judgement Silversword", year: 2004, significance: "Legendary vertical TATE mode shmup developed by indie master M-KAI on the WonderWitch kit." },
      { title: "Digimon Adventure: Anode/Cathode Tamer", year: 1999, significance: "Bandai's flagship handheld tactical RPG." }
    ]
  },

  ROM_PICO8: {
    id: "ROM_PICO8",
    name: "PICO-8 Fantasy Console",
    company: "Lexaloffle Games (Joseph White / Zep)",
    year: 2015,
    generation: "Modern Fantasy Computer",
    tagline: "The Modern Renaissance of Deliberate Constraints & Creative Purity",
    badgeColor: "#FF78C4",
    ext: ".p8",
    specs: {
      engine: "Lua-based Virtual Architecture @ 8 MHz effective virtual clock",
      display: "128 × 128 pixels (Square retro canvas)",
      colors: "16-color fixed curated palette (Classic + Secret Palette)",
      sound: "4-Channel Sound Synthesizer (8 custom waveforms per channel with SFX/music tracker)",
      storage: "32 KB Compressed Cartridges stored inside PNG images (Cartridge Cards!)",
      memory: "2 MB Virtual Memory Budget"
    },
    historicalSignificance:
      "PICO-8 ignited the 'Fantasy Console' revolution. By intentionally creating a virtual machine with strict 1980s-style limitations " +
      "(128x128 screen, 16 colors, 32KB code budget in Lua), it liberated thousands of indie developers from modern development bloat. " +
      "It proved that tight artistic constraints spark unbounded creativity, giving birth to indie legends like Celeste.",
    engineeringConstraints:
      "All code, sprites, map tiles, and music must fit in a 32,768-byte cartridge. Games are distributed as standard PNG images where the " +
      "game data is steganographically embedded into the two least significant bits of the image channels.",
    hallmarkGames: [
      { title: "Celeste (Classic)", year: 2015, significance: "Maddy Thorson & Noel Berry built the original Celeste prototype in 4 days on PICO-8 before the global hit." },
      { title: "Slipways", year: 2018, significance: "Grand space grand-strategy trade network game packed into 32KB." },
      { title: "High Stakes", year: 2021, significance: "Atmospheric gothic vampire poker thriller with fluid 16-color animations." },
      { title: "Just One Boss", year: 2019, significance: "Masterclass in multi-phase boss fight choreography within strict memory limits." }
    ]
  }
};

const PROSPECTIVE_TIER3_CONSOLES = [
  {
    name: "ZX Spectrum",
    year: 1982,
    cpu: "Z80A @ 3.5 MHz",
    ram: "16 KB / 48 KB",
    significance: "The foundation of European and British game development (Rare, Codemasters, Julian Gollop)."
  },
  {
    name: "Commodore 64",
    year: 1982,
    cpu: "MOS 6510 @ 1.02 MHz",
    ram: "64 KB RAM",
    sound: "SID 6581 Sound Synthesizer",
    significance: "The best-selling single computer model in history; legendary SID chiptune synthesis."
  },
  {
    name: "MSX / MSX2",
    year: 1983,
    cpu: "Zilog Z80A @ 3.58 MHz",
    ram: "64 KB / 128 KB",
    significance: "Kazuhiko Nishi and ASCII standard; birthplace of Hideo Kojima's Metal Gear and Konami classics."
  },
  {
    name: "Atari 7800 ProSystem",
    year: 1986,
    cpu: "Custom 6502C @ 1.79 MHz",
    gpu: "MARIA Graphic Processor (100 simultaneous sprites)",
    significance: "Atari's 1986 answer to the NES featuring the high-speed MARIA graphics blitter."
  },
  {
    name: "Chip-8 / SuperChip",
    year: 1975,
    cpu: "Virtual Interpreted Language",
    resolution: "64 × 32 / 128 × 64 pixels",
    significance: "The original 1970s virtual machine designed by Joseph Weisbecker for COSMAC VIP."
  }
];

if (typeof module !== 'undefined' && module.exports) {
  module.exports = { CONSOLE_HISTORY_DATABASE, PROSPECTIVE_TIER3_CONSOLES };
}
